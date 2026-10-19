// Station météo - étape 1 : ESP32 DevKit + BME280 + écran OLED, sans Wi-Fi
//
// Fonctionnement :
//  1. Toutes les 5 secondes, l'ESP32 lit la température, l'humidité et la pression du BME280.
//  2. Il affiche les mesures sur l'écran OLED, avec la pression ramenée au niveau de la mer.
//  3. Toutes les 10 minutes, il garde la pression en mémoire. Sur 3 heures, la variation donne
//     une tendance (hausse, stable, baisse) et une petite prévision, comme un baromètre à aiguille.
//
// Câblage : BME280 et écran OLED sur le même bus I2C (SDA = GPIO21, SCL = GPIO22, VCC = 3V3).

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

#define BROCHE_SDA     21
#define BROCHE_SCL     22
#define LARGEUR_ECRAN  128
#define HAUTEUR_ECRAN  64
#define ADRESSE_ECRAN  0x3C

// Avec cp437(true), ce code affiche le signe degré sur l'écran OLED
#define CAR_DEGRE ((char)0xF8)

const uint32_t PERIODE_MESURE_MS     = 5000;
const uint32_t PERIODE_HISTORIQUE_MS = 10UL * 60 * 1000;  // un point de pression toutes les 10 min
const int      NB_POINTS_3H          = 19;                // 18 intervalles de 10 min = 3 h

Adafruit_BME280 bme;
Adafruit_SSD1306 ecran(LARGEUR_ECRAN, HAUTEUR_ECRAN, &Wire, -1);

struct Mesure {
  float temperature;  // °C
  float humidite;     // %
  float pressionMer;  // hPa, ramenée au niveau de la mer
  bool valide;
};

// Historique circulaire de la pression sur 3 h
float historiquePression[NB_POINTS_3H];
int nbPoints = 0;
int prochainPoint = 0;

// ---------- Capteur ----------
bool demarrerCapteur() {
  // Selon le module, le BME280 répond à l'adresse 0x76 ou 0x77
  if (!bme.begin(0x76, &Wire) && !bme.begin(0x77, &Wire)) {
    uint32_t id = bme.sensorID();
    if (id == 0x58) {
      Serial.println("Erreur : c'est un BMP280 (pas d'humidité), il faut un vrai BME280");
    } else {
      Serial.printf("Erreur : BME280 introuvable (identifiant lu : 0x%02X), vérifie le câblage\n", id);
    }
    return false;
  }
  // Mode « forcé » conseillé par Bosch pour la météo : une mesure à la demande puis sommeil,
  // ce qui limite l'échauffement du capteur
  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1,   // température
                  Adafruit_BME280::SAMPLING_X1,   // pression
                  Adafruit_BME280::SAMPLING_X1,   // humidité
                  Adafruit_BME280::FILTER_OFF);
  bme.setTemperatureCompensation(CORRECTION_TEMPERATURE);
  return true;
}

Mesure lireCapteur() {
  Mesure m = {NAN, NAN, NAN, false};
  if (!bme.takeForcedMeasurement()) return m;
  m.temperature = bme.readTemperature();
  m.humidite = bme.readHumidity();
  float pressionLocale = bme.readPressure() / 100.0F;  // Pa vers hPa
  m.pressionMer = bme.seaLevelForAltitude(ALTITUDE_M, pressionLocale);
  m.valide = !isnan(m.temperature) && !isnan(m.humidite) && !isnan(m.pressionMer);
  return m;
}

// ---------- Tendance de la pression ----------
void memoriserPression(float pression) {
  historiquePression[prochainPoint] = pression;
  prochainPoint = (prochainPoint + 1) % NB_POINTS_3H;
  if (nbPoints < NB_POINTS_3H) nbPoints++;
}

// Variation de pression ramenée à 3 h, en hPa. Renvoie NAN tant qu'on a moins d'une heure de recul.
float variation3h() {
  if (nbPoints < 7) return NAN;
  int plusRecent = (prochainPoint - 1 + NB_POINTS_3H) % NB_POINTS_3H;
  int plusAncien = (nbPoints < NB_POINTS_3H) ? 0 : prochainPoint;
  float ecart = historiquePression[plusRecent] - historiquePression[plusAncien];
  return ecart * (NB_POINTS_3H - 1) / (nbPoints - 1);
}

// Règle simple des baromètres : au-delà de 1,6 hPa en 3 h, le temps change
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

// ---------- Affichage ----------
// La police de l'écran n'a pas d'accents : les textes affichés en sont dépourvus.
void afficher(const Mesure& m, float var) {
  ecran.clearDisplay();
  ecran.setTextColor(SSD1306_WHITE);

  if (!m.valide) {
    ecran.setTextSize(1);
    ecran.setCursor(0, 24);
    ecran.print("Capteur muet !");
    ecran.display();
    return;
  }

  // Température en gros
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
  ecran.clearDisplay();
  ecran.setTextColor(SSD1306_WHITE);
  ecran.setTextSize(1);
  ecran.setCursor(0, 0);
  ecran.println("Station meteo");
  ecran.println("Bidouille Connectee");
  ecran.display();

  capteurOk = demarrerCapteur();
  delay(1500);
}

void loop() {
  uint32_t maintenant = millis();
  if (derniereMesure != 0 && maintenant - derniereMesure < PERIODE_MESURE_MS) return;
  derniereMesure = maintenant;

  if (!capteurOk) capteurOk = demarrerCapteur();  // on retente si le capteur a été rebranché
  Mesure m = capteurOk ? lireCapteur() : Mesure{NAN, NAN, NAN, false};

  if (m.valide && (nbPoints == 0 || maintenant - dernierHistorique >= PERIODE_HISTORIQUE_MS)) {
    memoriserPression(m.pressionMer);
    dernierHistorique = maintenant;
  }

  float var = variation3h();
  afficher(m, var);

  if (m.valide) {
    Serial.printf("%.1f °C  %.0f %%  %.1f hPa  tendance %s (%+.1f hPa/3 h)\n",
                  m.temperature, m.humidite, m.pressionMer, texteTendance(var), var);
  }
}
