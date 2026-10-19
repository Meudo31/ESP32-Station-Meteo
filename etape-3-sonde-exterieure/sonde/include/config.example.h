// Copier ce fichier en config.h et remplir les valeurs.
// config.h ne doit jamais être publié (il contient le nom du Wi-Fi).
#pragma once

// La sonde ne se connecte pas au Wi-Fi : elle cherche seulement sur quel canal il émet,
// car la station l'écoute sur ce même canal.
#define WIFI_SSID "NomDeTonWifi"

// Adresse MAC de la station intérieure, affichée sur son écran et son moniteur série
// au démarrage. Exemple pour 24:6F:28:AB:CD:EF :
#define MAC_STATION {0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF}

// Intervalle entre deux mesures, en secondes. 300 s (5 min) : la batterie tient plusieurs mois.
#define INTERVALLE_MESURE_S 300

// Correction de température en °C, à régler en comparant avec un thermomètre fiable
#define CORRECTION_TEMPERATURE 0.0

// Correction de la mesure de batterie : tension au multimètre / tension affichée.
// Les résistances à 5 % et l'ADC de l'ESP32-C3 donnent quelques pourcents d'écart.
#define CORRECTION_BATTERIE 1.0
