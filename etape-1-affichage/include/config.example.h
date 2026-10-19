// Copier ce fichier en config.h et ajuster les valeurs.
#pragma once

// Altitude de la station en mètres (Géoportail ou appli GPS). Sert à ramener la
// pression au niveau de la mer, la valeur donnée par Météo-France et les applis météo.
#define ALTITUDE_M 150

// Correction de température en °C. Le BME280 chauffe un peu à cause de l'ESP32 et de l'écran :
// compare avec un thermomètre fiable après une heure de fonctionnement, puis mets ici la
// différence (exemple : -1.2 si la station affiche 1,2 °C de trop).
#define CORRECTION_TEMPERATURE 0.0
