// Station météo - étape 3 : sonde extérieure sur batterie (ESP32-C3 SuperMini + BME280)
//
// Fonctionnement :
//  1. La sonde dort (deep sleep) et se réveille toutes les 5 minutes.
//  2. Elle mesure la température, l'humidité, la pression et la tension de sa batterie.
//  3. Elle envoie le tout à la station intérieure en ESP-NOW : pas de connexion au Wi-Fi,
//     l'envoi prend une fraction de seconde, ce qui fait durer la batterie des mois.
//  4. Elle se rendort.
//
// ESP-NOW doit émettre sur le même canal que la box, puisque la station y est connectée.
// La sonde cherche ce canal au premier réveil, le garde en mémoire, et le recherche
// après trois envois ratés (si la box a changé de canal, par exemple).
//
// Câblage : BME280 sur SDA = GPIO6, SCL = GPIO7, VCC = 3V3.
// Pont diviseur 2 x 100 kΩ entre batterie + et GND, point milieu sur GPIO3.

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Adafruit_BME280.h>
#include "config.h"
#include "message_sonde.h"

#define BROCHE_SDA       6
#define BROCHE_SCL       7
#define BROCHE_BATTERIE  3
#define ECHECS_AVANT_RECHERCHE 3

// Conservés pendant le deep sleep
RTC_DATA_ATTR uint8_t canalWifi = 0;  // 0 = canal inconnu, à rechercher
RTC_DATA_ATTR uint8_t echecs = 0;
RTC_DATA_ATTR uint32_t numero = 0;

Adafruit_BME280 bme;
const uint8_t macStation[6] = MAC_STATION;
volatile bool envoiTermine = false;
volatile bool envoiReussi = false;

// ---------- Mesures ----------
bool mesurer(MessageSonde& msg) {
  Wire.begin(BROCHE_SDA, BROCHE_SCL);
  if (!bme.begin(0x76, &Wire) && !bme.begin(0x77, &Wire)) {
    Serial.printf("Erreur : BME280 introuvable (identifiant lu : 0x%02X)\n", bme.sensorID());
    return false;
  }
  // Mode forcé : une mesure puis le capteur se rendort tout seul (0,1 µA)
  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::FILTER_OFF);
  bme.setTemperatureCompensation(CORRECTION_TEMPERATURE);
  if (!bme.takeForcedMeasurement()) return false;
  msg.temperature = bme.readTemperature();
  msg.humidite = bme.readHumidity();
  msg.pression = bme.readPressure() / 100.0F;
  return !isnan(msg.temperature);
}

uint16_t lireBatterieMv() {
  uint32_t somme = 0;
  for (int i = 0; i < 8; i++) somme += analogReadMilliVolts(BROCHE_BATTERIE);
  return (uint16_t)(somme / 8 * 2 * CORRECTION_BATTERIE);  // x2 à cause du pont diviseur
}

// ---------- Canal Wi-Fi ----------
uint8_t chercherCanal() {
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == WIFI_SSID) {
      uint8_t canal = WiFi.channel(i);
      Serial.printf("Wi-Fi « %s » trouvé sur le canal %d (%d dBm)\n", WIFI_SSID, canal, WiFi.RSSI(i));
      WiFi.scanDelete();
      return canal;
    }
  }
  WiFi.scanDelete();
  Serial.printf("Erreur : Wi-Fi « %s » introuvable\n", WIFI_SSID);
  return 0;
}

// ---------- ESP-NOW ----------
// La signature du rappel d'envoi a changé avec ESP-IDF 5.5
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
void finEnvoi(const wifi_tx_info_t* info, esp_now_send_status_t statut) {
#else
void finEnvoi(const uint8_t* mac, esp_now_send_status_t statut) {
#endif
  envoiReussi = (statut == ESP_NOW_SEND_SUCCESS);
  envoiTermine = true;
}

bool envoyer(const MessageSonde& msg) {
  // Se caler sur le canal de la box sans s'y connecter
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(canalWifi, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Erreur : ESP-NOW ne démarre pas");
    return false;
  }
  esp_now_register_send_cb(finEnvoi);

  esp_now_peer_info_t station = {};
  memcpy(station.peer_addr, macStation, 6);
  station.channel = canalWifi;
  station.ifidx = WIFI_IF_STA;
  station.encrypt = false;
  esp_now_add_peer(&station);

  envoiTermine = false;
  if (esp_now_send(macStation, (const uint8_t*)&msg, sizeof(msg)) != ESP_OK) return false;
  uint32_t debut = millis();
  while (!envoiTermine && millis() - debut < 500) delay(1);
  return envoiTermine && envoiReussi;  // réussi = la station a accusé réception
}

// ---------- Sommeil ----------
void allerDormir() {
  WiFi.mode(WIFI_OFF);
  Serial.printf("Dodo pendant %d s\n", INTERVALLE_MESURE_S);
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)INTERVALLE_MESURE_S * 1000000ULL);
  esp_deep_sleep_start();
}

// ---------- Programme principal ----------
void setup() {
  Serial.begin(115200);
  delay(100);

  MessageSonde msg = {};
  msg.version = VERSION_MESSAGE;
  msg.numero = ++numero;
  msg.echecs = echecs;
  msg.batterieMv = lireBatterieMv();
  if (!mesurer(msg)) {
    msg.temperature = msg.humidite = msg.pression = NAN;  // la station saura que le capteur est muet
  }
  Serial.printf("Mesure n.%lu : %.1f °C  %.0f %%  %.1f hPa  batterie %.2f V\n",
                (unsigned long)msg.numero, msg.temperature, msg.humidite, msg.pression,
                msg.batterieMv / 1000.0);

  WiFi.mode(WIFI_STA);
  if (canalWifi == 0) canalWifi = chercherCanal();
  if (canalWifi == 0) allerDormir();  // box introuvable : on réessaiera au prochain réveil

  if (envoyer(msg)) {
    Serial.printf("Envoyé à la station (canal %d)\n", canalWifi);
    echecs = 0;
  } else {
    echecs++;
    Serial.printf("Erreur : pas d'accusé de réception de la station (%d échec(s))\n", echecs);
    if (echecs % ECHECS_AVANT_RECHERCHE == 0) canalWifi = 0;  // la box a peut-être changé de canal
  }
  allerDormir();
}

void loop() {
  // Jamais atteint : tout se passe dans setup() puis deep sleep
}
