// Message envoyé par la sonde extérieure à la station, en ESP-NOW.
// Ce fichier doit être identique dans les deux projets (sonde et station).
#pragma once
#include <stdint.h>

#define VERSION_MESSAGE 1

struct __attribute__((packed)) MessageSonde {
  uint8_t  version;      // VERSION_MESSAGE, pour ignorer les messages d'un autre format
  uint32_t numero;       // compteur d'envois depuis la mise sous tension de la sonde
  float    temperature;  // °C
  float    humidite;     // %
  float    pression;     // hPa, pression locale (non ramenée au niveau de la mer)
  uint16_t batterieMv;   // tension de la batterie en millivolts
  uint8_t  echecs;       // envois ratés depuis le dernier envoi réussi
};
