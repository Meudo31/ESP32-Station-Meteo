// Station météo - étape 2 : page web, historique 24 h et alertes ntfy
//
// Fonctionnement :
//  1. Même station qu'à l'étape 1 (BME280 + écran OLED), qui se connecte en plus au Wi-Fi.
//  2. Elle garde 24 h de mesures en mémoire (un point toutes les 10 min) et les affiche en
//     courbes sur une page web : http://meteo.local (ou l'adresse IP affichée au démarrage).
//  3. Elle envoie une notification ntfy sur le téléphone si la température sort de la plage
//     choisie ou si la pression chute vite (risque de tempête).
//
// Câblage : identique à l'étape 1 (SDA = GPIO21, SCL = GPIO22, VCC = 3V3).

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <time.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
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
  float temperature, humidite, pression;
};

Point historique[NB_POINTS];
int nbPoints = 0;
int prochainPoint = 0;

Mesure derniere = {NAN, NAN, NAN, false};

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

// ---------- Heure ----------
bool heureConnue() {
  return time(nullptr) > 1700000000;  // après novembre 2023 : l'heure vient bien d'Internet
}

// ---------- Historique et tendance ----------
// k = 0 pour le point le plus récent, 1 pour le précédent, etc.
const Point& pointIlYA(int k) {
  return historique[(prochainPoint - 1 - k + 2 * NB_POINTS) % NB_POINTS];
}

void memoriser(const Mesure& m) {
  historique[prochainPoint] = {heureConnue() ? (uint32_t)time(nullptr) : 0,
                               m.temperature, m.humidite, m.pressionMer};
  prochainPoint = (prochainPoint + 1) % NB_POINTS;
  if (nbPoints < NB_POINTS) nbPoints++;
}

// Variation de pression ramenée à 3 h (18 points), NAN avant une heure de recul
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

void verifierAlertes(const Mesure& m, float var) {
  char texte[96];

  if (!alerteFroid && m.temperature < ALERTE_TEMP_MIN) {
    snprintf(texte, sizeof(texte), "Il fait %.1f °C (seuil %.1f °C)", m.temperature, (float)ALERTE_TEMP_MIN);
    alerteFroid = envoyerNtfy("Temperature basse", texte, "cold_face");
  } else if (alerteFroid && m.temperature > ALERTE_TEMP_MIN + 0.5) {
    alerteFroid = false;  // retour à la normale : l'alerte pourra repartir
  }

  if (!alerteChaud && m.temperature > ALERTE_TEMP_MAX) {
    snprintf(texte, sizeof(texte), "Il fait %.1f °C (seuil %.1f °C)", m.temperature, (float)ALERTE_TEMP_MAX);
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
}

// ---------- Page web ----------
void ajouterNombre(String& json, float valeur, int decimales) {
  if (isnan(valeur)) json += "null";
  else json += String(valeur, decimales);
}

// Renvoie les mesures actuelles et l'historique (du plus ancien au plus récent) en JSON
void repondreDonnees() {
  float var = variation3h();
  String json;
  json.reserve(8000);
  json += "{\"actuel\":{\"t\":";
  ajouterNombre(json, derniere.temperature, 1);
  json += ",\"h\":";
  ajouterNombre(json, derniere.humidite, 0);
  json += ",\"p\":";
  ajouterNombre(json, derniere.pressionMer, 1);
  json += ",\"var\":";
  ajouterNombre(json, var, 1);
  json += ",\"tendance\":\"" + String(texteTendance(var)) + "\"";
  json += ",\"prevision\":\"" + String(textePrevision(var, derniere.pressionMer)) + "\"},";
  json += "\"historique\":[";
  for (int k = nbPoints - 1; k >= 0; k--) {
    const Point& p = pointIlYA(k);
    json += "[" + String(p.horodatage) + ",";
    ajouterNombre(json, p.temperature, 1);
    json += ",";
    ajouterNombre(json, p.humidite, 0);
    json += ",";
    ajouterNombre(json, p.pression, 1);
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
void afficher(const Mesure& m, float var) {
  ecran.clearDisplay();
  ecran.setTextColor(SSD1306_WHITE);

  // Petit témoin Wi-Fi en haut à droite
  ecran.setTextSize(1);
  ecran.setCursor(LARGEUR_ECRAN - 6, 0);
  ecran.print(WiFi.status() == WL_CONNECTED ? "W" : "-");

  if (!m.valide) {
    ecran.setCursor(0, 24);
    ecran.print("Capteur muet !");
    ecran.display();
    return;
  }

  ecran.setTextSize(3);
  ecran.setCursor(0, 0);
  ecran.printf("%.1f", m.temperature);
  ecran.setTextSize(2);
  ecran.print(CAR_DEGRE);
  ecran.print("C");

  ecran.setTextSize(1);
  ecran.setCursor(0, 28);
  ecran.printf("Humidite  %.0f %%", m.humidite);
  ecran.setCursor(0, 38);
  ecran.printf("Pression  %.0f hPa", m.pressionMer);
  ecran.setCursor(0, 48);
  ecran.printf("Tendance  %s", texteTendance(var));
  ecran.setCursor(0, 57);
  ecran.print(textePrevision(var, m.pressionMer));
  ecran.display();
}

void afficherMessage(const char* ligne1, const char* ligne2) {
  ecran.clearDisplay();
  ecran.setTextSize(1);
  ecran.setTextColor(SSD1306_WHITE);
  ecran.setCursor(0, 16);
  ecran.println(ligne1);
  ecran.println();
  ecran.println(ligne2);
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
  WiFi.begin(WIFI_SSID, WIFI_MOTDEPASSE);
  uint32_t debut = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - debut < 20000) delay(200);

  configTzTime(FUSEAU_HORAIRE, "pool.ntp.org", "time.google.com");
  demarrerServeur();

  if (WiFi.status() == WL_CONNECTED) {
    String ip = WiFi.localIP().toString();
    Serial.printf("Wi-Fi connecté, signal %d dBm\n", WiFi.RSSI());
    Serial.printf("Page web : http://%s.local ou http://%s\n", NOM_RESEAU, ip.c_str());
    afficherMessage("Page web :", ip.c_str());
    envoyerNtfy("Station meteo", "Station météo en service", "sunny");
    delay(5000);  // le temps de noter l'adresse
  } else {
    Serial.println("Erreur : Wi-Fi injoignable, la station continue en local et réessaiera");
    afficherMessage("Wi-Fi injoignable", "Mode local");
    delay(2000);
  }
}

void loop() {
  serveur.handleClient();

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
  if (derniere.valide) {
    verifierAlertes(derniere, var);
    Serial.printf("%.1f °C  %.0f %%  %.1f hPa  tendance %s\n",
                  derniere.temperature, derniere.humidite, derniere.pressionMer, texteTendance(var));
  }
}
