// Station météo - étape 3 : station intérieure qui reçoit la sonde extérieure
//
// Fonctionnement :
//  1. Station de l'étape 2 (BME280 + écran OLED + page web + alertes ntfy).
//  2. Elle reçoit en plus, en ESP-NOW, les mesures de la sonde extérieure sur batterie.
//  3. L'écran, la page web et l'historique 24 h montrent l'intérieur et l'extérieur.
//  4. Nouvelles alertes : gel dehors, sonde muette, batterie de la sonde faible.
//
// Au démarrage, l'écran affiche l'adresse MAC de la station : c'est elle qu'il faut
// recopier dans le config.h de la sonde.
//
// Câblage : identique à l'étape 2 (SDA = GPIO21, SCL = GPIO22, VCC = 3V3).

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <esp_now.h>
#include <time.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "message_sonde.h"
#include "page_web.h"

#define BROCHE_SDA     21
#define BROCHE_SCL     22
#define LARGEUR_ECRAN  128
#define HAUTEUR_ECRAN  64
#define ADRESSE_ECRAN  0x3C
#define CAR_DEGRE      ((char)0xF8)  // signe degré avec cp437(true)

const uint32_t PERIODE_MESURE_MS     = 5000;
const uint32_t PERIODE_HISTORIQUE_MS = 10UL * 60 * 1000;
const int      NB_POINTS             = 144;  // 24 h à raison d'un point toutes les 10 min

Adafruit_BME280 bme;
Adafruit_SSD1306 ecran(LARGEUR_ECRAN, HAUTEUR_ECRAN, &Wire, -1);
WebServer serveur(80);

struct Mesure {
  float temperature;  // °C
  float humidite;     // %
  float pressionMer;  // hPa, ramenée au niveau de la mer
  bool valide;
};

struct Point {
  uint32_t horodatage;  // secondes depuis 1970, 0 si l'heure n'est pas encore connue
  float temperature, humidite, pression;  // intérieur
  float tempExt, humExt;                  // extérieur, NAN si la sonde est muette
};

Point historique[NB_POINTS];
int nbPoints = 0;
int prochainPoint = 0;

Mesure derniere = {NAN, NAN, NAN, false};

// Dernier message de la sonde. Il arrive dans la tâche Wi-Fi : on le copie dans un tampon
// et loop() le traite.
MessageSonde sonde = {};
uint32_t sondeRecueA = 0;  // millis() de la dernière réception, 0 = jamais reçue
MessageSonde tamponSonde;
volatile bool nouveauMessage = false;

// ---------- Capteur ----------
bool demarrerCapteur() {
  if (!bme.begin(0x76, &Wire) && !bme.begin(0x77, &Wire)) {
    uint32_t id = bme.sensorID();
    if (id == 0x58) {
      Serial.println("Erreur : c'est un BMP280 (pas d'humidité), il faut un vrai BME280");
    } else {
      Serial.printf("Erreur : BME280 introuvable (identifiant lu : 0x%02X), vérifie le câblage\n", id);
    }
    return false;
  }
  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::FILTER_OFF);
  bme.setTemperatureCompensation(CORRECTION_TEMPERATURE);
  return true;
}

Mesure lireCapteur() {
  Mesure m = {NAN, NAN, NAN, false};
  if (!bme.takeForcedMeasurement()) return m;
  m.temperature = bme.readTemperature();
  m.humidite = bme.readHumidity();
  m.pressionMer = bme.seaLevelForAltitude(ALTITUDE_M, bme.readPressure() / 100.0F);
  m.valide = !isnan(m.temperature) && !isnan(m.humidite) && !isnan(m.pressionMer);
  return m;
}

// ---------- Sonde extérieure (ESP-NOW) ----------
// La signature du rappel de réception a changé avec la version 3 du cœur Arduino
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void messageRecu(const esp_now_recv_info_t* info, const uint8_t* donnees, int taille) {
#else
void messageRecu(const uint8_t* mac, const uint8_t* donnees, int taille) {
#endif
  if (taille != sizeof(MessageSonde) || donnees[0] != VERSION_MESSAGE) return;
  memcpy(&tamponSonde, donnees, sizeof(MessageSonde));
  nouveauMessage = true;
}

bool sondeAJour() {
  return sondeRecueA != 0 && millis() - sondeRecueA < ALERTE_SONDE_MUETTE_MIN * 60000UL;
}

float tempExt() { return sondeAJour() ? sonde.temperature : NAN; }
float humExt()  { return sondeAJour() ? sonde.humidite : NAN; }

void traiterSonde() {
  if (!nouveauMessage) return;
  nouveauMessage = false;
  memcpy(&sonde, &tamponSonde, sizeof(MessageSonde));
  sondeRecueA = millis();
  if (sondeRecueA == 0) sondeRecueA = 1;
  Serial.printf("Sonde n.%lu : %.1f °C  %.0f %%  batterie %.2f V  (%d envoi(s) raté(s) avant)\n",
                (unsigned long)sonde.numero, sonde.temperature, sonde.humidite,
                sonde.batterieMv / 1000.0, sonde.echecs);
}

// ---------- Heure ----------
bool heureConnue() {
  return time(nullptr) > 1700000000;  // après novembre 2023 : l'heure vient bien d'Internet
}

// ---------- Historique et tendance ----------
const Point& pointIlYA(int k) {
  return historique[(prochainPoint - 1 - k + 2 * NB_POINTS) % NB_POINTS];
}

void memoriser(const Mesure& m) {
  historique[prochainPoint] = {heureConnue() ? (uint32_t)time(nullptr) : 0,
                               m.temperature, m.humidite, m.pressionMer, tempExt(), humExt()};
  prochainPoint = (prochainPoint + 1) % NB_POINTS;
  if (nbPoints < NB_POINTS) nbPoints++;
}

float variation3h() {
  if (nbPoints < 7) return NAN;
  int k = min(nbPoints - 1, 18);
  return (pointIlYA(0).pression - pointIlYA(k).pression) * 18.0f / k;
}

const char* texteTendance(float var) {
  if (isnan(var)) return "calcul (1 h)";
  if (var > 1.6)  return "en hausse";
  if (var < -1.6) return "en baisse";
  return "stable";
}

const char* textePrevision(float var, float pression) {
  if (isnan(var))  return "";
  if (var < -4.0)  return "Tempete possible";
  if (var < -1.6)  return "Degradation";
  if (var > 1.6)   return "Amelioration";
  if (pression > 1020) return "Beau temps";
  if (pression < 1005) return "Temps couvert";
  return "Temps variable";
}

// ---------- Notifications ntfy ----------
bool envoyerNtfy(const char* titre, const char* message, const char* tags) {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();  // pas de vérification du certificat : simple, suffisant pour ce projet
  HTTPClient http;
  String url = String(NTFY_SERVEUR) + "/" + NTFY_SUJET;
  if (!http.begin(client, url)) return false;
  http.addHeader("Title", titre);  // en-têtes HTTP sans accents (ASCII obligatoire)
  http.addHeader("Tags", tags);
  int code = http.POST((uint8_t*)message, strlen(message));
  http.end();
  Serial.printf("Réponse ntfy : %d\n", code);
  return code == 200;
}

bool alerteFroid = false, alerteChaud = false, alerteTempete = false;
bool alerteGel = false, alerteMuette = false, alerteBatterie = false;

void verifierAlertes(const Mesure& m, float var) {
  char texte[96];

  if (!alerteFroid && m.temperature < ALERTE_TEMP_MIN) {
    snprintf(texte, sizeof(texte), "Il fait %.1f °C dedans (seuil %.1f °C)", m.temperature, (float)ALERTE_TEMP_MIN);
    alerteFroid = envoyerNtfy("Temperature basse", texte, "cold_face");
  } else if (alerteFroid && m.temperature > ALERTE_TEMP_MIN + 0.5) {
    alerteFroid = false;  // retour à la normale : l'alerte pourra repartir
  }

  if (!alerteChaud && m.temperature > ALERTE_TEMP_MAX) {
    snprintf(texte, sizeof(texte), "Il fait %.1f °C dedans (seuil %.1f °C)", m.temperature, (float)ALERTE_TEMP_MAX);
    alerteChaud = envoyerNtfy("Temperature haute", texte, "hot_face");
  } else if (alerteChaud && m.temperature < ALERTE_TEMP_MAX - 0.5) {
    alerteChaud = false;
  }

  if (!isnan(var)) {
    if (!alerteTempete && var < -ALERTE_CHUTE_PRESSION) {
      snprintf(texte, sizeof(texte), "La pression chute de %.1f hPa en 3 h (%.0f hPa)", -var, m.pressionMer);
      alerteTempete = envoyerNtfy("Risque de tempete", texte, "cyclone");
    } else if (alerteTempete && var > -ALERTE_CHUTE_PRESSION / 2) {
      alerteTempete = false;
    }
  }

  // Alertes de la sonde extérieure
  float te = tempExt();
  if (!isnan(te)) {
    if (!alerteGel && te < ALERTE_GEL) {
      snprintf(texte, sizeof(texte), "Il fait %.1f °C dehors : risque de gel", te);
      alerteGel = envoyerNtfy("Risque de gel", texte, "snowflake");
    } else if (alerteGel && te > ALERTE_GEL + 1.0) {
      alerteGel = false;
    }
  }

  if (sondeRecueA != 0) {
    bool muette = !sondeAJour();
    if (!alerteMuette && muette) {
      snprintf(texte, sizeof(texte), "Aucune nouvelle de la sonde depuis %d min", ALERTE_SONDE_MUETTE_MIN);
      alerteMuette = envoyerNtfy("Sonde exterieure muette", texte, "warning");
    } else if (alerteMuette && !muette) {
      alerteMuette = !envoyerNtfy("Sonde exterieure", "La sonde donne de nouveau des nouvelles", "white_check_mark");
    }

    float volts = sonde.batterieMv / 1000.0;
    if (!alerteBatterie && !muette && volts < ALERTE_BATTERIE_V) {
      snprintf(texte, sizeof(texte), "Batterie de la sonde : %.2f V, pense à la recharger", volts);
      alerteBatterie = envoyerNtfy("Batterie sonde faible", texte, "battery");
    } else if (alerteBatterie && volts > ALERTE_BATTERIE_V + 0.3) {
      alerteBatterie = false;  // batterie rechargée
    }
  }
}

// ---------- Page web ----------
void ajouterNombre(String& json, float valeur, int decimales) {
  if (isnan(valeur)) json += "null";
  else json += String(valeur, decimales);
}

void repondreDonnees() {
  float var = variation3h();
  String json;
  json.reserve(12000);
  json += "{\"actuel\":{\"t\":";
  ajouterNombre(json, derniere.temperature, 1);
  json += ",\"h\":";
  ajouterNombre(json, derniere.humidite, 0);
  json += ",\"p\":";
  ajouterNombre(json, derniere.pressionMer, 1);
  json += ",\"var\":";
  ajouterNombre(json, var, 1);
  json += ",\"tendance\":\"" + String(texteTendance(var)) + "\"";
  json += ",\"prevision\":\"" + String(textePrevision(var, derniere.pressionMer)) + "\"";
  json += ",\"te\":";
  ajouterNombre(json, tempExt(), 1);
  json += ",\"he\":";
  ajouterNombre(json, humExt(), 0);
  json += ",\"batterie\":";
  ajouterNombre(json, sondeRecueA ? sonde.batterieMv / 1000.0 : NAN, 2);
  json += ",\"sondeIlYA\":";
  json += sondeRecueA ? String((millis() - sondeRecueA) / 1000) : "null";  // secondes
  json += "},\"historique\":[";
  for (int k = nbPoints - 1; k >= 0; k--) {
    const Point& p = pointIlYA(k);
    json += "[" + String(p.horodatage) + ",";
    ajouterNombre(json, p.temperature, 1);
    json += ",";
    ajouterNombre(json, p.humidite, 0);
    json += ",";
    ajouterNombre(json, p.pression, 1);
    json += ",";
    ajouterNombre(json, p.tempExt, 1);
    json += ",";
    ajouterNombre(json, p.humExt, 0);
    json += (k > 0) ? "]," : "]";
  }
  json += "]}";
  serveur.send(200, "application/json", json);
}

void demarrerServeur() {
  serveur.on("/", []() { serveur.send_P(200, "text/html; charset=utf-8", PAGE_WEB); });
  serveur.on("/donnees", repondreDonnees);
  serveur.onNotFound([]() { serveur.send(404, "text/plain", "Page introuvable"); });
  serveur.begin();
  if (MDNS.begin(NOM_RESEAU)) MDNS.addService("http", "tcp", 80);
}

// ---------- Affichage ----------
// Une ligne « In 21.4°C  48% » ou « Ex  8.2°C  87% » : température en gros, humidité en petit
void afficherLigne(int y, const char* nom, float temperature, float humidite) {
  ecran.setTextSize(1);
  ecran.setCursor(0, y + 4);
  ecran.print(nom);
  ecran.setTextSize(2);
  ecran.setCursor(16, y);
  if (isnan(temperature)) {
    ecran.print("  --");
  } else {
    ecran.printf("%5.1f", temperature);
    ecran.print(CAR_DEGRE);
  }
  ecran.setTextSize(1);
  ecran.setCursor(104, y + 4);
  if (!isnan(humidite)) ecran.printf("%.0f%%", humidite);
}

void afficher(const Mesure& m, float var) {
  ecran.clearDisplay();
  ecran.setTextColor(SSD1306_WHITE);

  afficherLigne(0, "In", m.valide ? m.temperature : NAN, m.valide ? m.humidite : NAN);
  afficherLigne(18, "Ex", tempExt(), humExt());

  ecran.setTextSize(1);
  ecran.setCursor(0, 38);
  if (m.valide) ecran.printf("%.0f hPa  %s", m.pressionMer, texteTendance(var));
  else ecran.print("Capteur muet !");
  ecran.setCursor(0, 48);
  ecran.print(textePrevision(var, m.pressionMer));

  // Dernière ligne : état de la sonde et du Wi-Fi
  ecran.setCursor(0, 57);
  if (sondeRecueA == 0) ecran.print("Sonde : en attente");
  else if (!sondeAJour()) ecran.print("Sonde muette !");
  else ecran.printf("Sonde %.2fV", sonde.batterieMv / 1000.0);
  ecran.setCursor(LARGEUR_ECRAN - 6, 57);
  ecran.print(WiFi.status() == WL_CONNECTED ? "W" : "-");
  ecran.display();
}

void afficherMessage(const char* ligne1, const char* ligne2, const char* ligne3 = "") {
  ecran.clearDisplay();
  ecran.setTextSize(1);
  ecran.setTextColor(SSD1306_WHITE);
  ecran.setCursor(0, 8);
  ecran.println(ligne1);
  ecran.println(ligne2);
  ecran.println();
  ecran.println(ligne3);
  ecran.display();
}

// ---------- Programme principal ----------
bool capteurOk = false;
uint32_t derniereMesure = 0;
uint32_t dernierHistorique = 0;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Démarrage de la station météo");

  Wire.begin(BROCHE_SDA, BROCHE_SCL);
  if (!ecran.begin(SSD1306_SWITCHCAPVCC, ADRESSE_ECRAN)) {
    Serial.println("Erreur : écran OLED introuvable (adresse 0x3C ?)");
  }
  ecran.cp437(true);
  afficherMessage("Station meteo", "Connexion Wi-Fi...");

  capteurOk = demarrerCapteur();

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(NOM_RESEAU);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);  // sinon la radio s'assoupit et rate des messages de la sonde
  WiFi.begin(WIFI_SSID, WIFI_MOTDEPASSE);
  uint32_t debut = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - debut < 20000) delay(200);

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(messageRecu);
  } else {
    Serial.println("Erreur : ESP-NOW ne démarre pas, la sonde ne sera pas reçue");
  }

  configTzTime(FUSEAU_HORAIRE, "pool.ntp.org", "time.google.com");
  demarrerServeur();

  String mac = WiFi.macAddress();
  Serial.printf("Adresse MAC à recopier dans la sonde : %s\n", mac.c_str());

  if (WiFi.status() == WL_CONNECTED) {
    String ip = WiFi.localIP().toString();
    Serial.printf("Wi-Fi connecté sur le canal %d, signal %d dBm\n", WiFi.channel(), WiFi.RSSI());
    Serial.printf("Page web : http://%s.local ou http://%s\n", NOM_RESEAU, ip.c_str());
    afficherMessage(("Page : " + ip).c_str(), "MAC pour la sonde :", mac.c_str());
    envoyerNtfy("Station meteo", "Station météo en service", "sunny");
    delay(8000);  // le temps de noter l'adresse et la MAC
  } else {
    Serial.println("Erreur : Wi-Fi injoignable, la station continue en local et réessaiera");
    afficherMessage("Wi-Fi injoignable", "MAC pour la sonde :", mac.c_str());
    delay(4000);
  }
}

void loop() {
  serveur.handleClient();
  traiterSonde();

  uint32_t maintenant = millis();
  if (derniereMesure != 0 && maintenant - derniereMesure < PERIODE_MESURE_MS) return;
  derniereMesure = maintenant;

  if (!capteurOk) capteurOk = demarrerCapteur();
  derniere = capteurOk ? lireCapteur() : Mesure{NAN, NAN, NAN, false};

  if (derniere.valide && (nbPoints == 0 || maintenant - dernierHistorique >= PERIODE_HISTORIQUE_MS)) {
    memoriser(derniere);
    dernierHistorique = maintenant;
  }

  float var = variation3h();
  afficher(derniere, var);
  if (derniere.valide) verifierAlertes(derniere, var);
}
