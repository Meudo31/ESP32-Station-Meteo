// Copier ce fichier en config.h et remplir les valeurs.
// config.h ne doit jamais être publié (il contient le mot de passe Wi-Fi).
#pragma once

#define WIFI_SSID       "NomDeTonWifi"
#define WIFI_MOTDEPASSE "MotDePasseWifi"

// Nom de la station sur le réseau : la page s'ouvre à l'adresse http://meteo.local
#define NOM_RESEAU "meteo"

// Sujet ntfy : n'importe qui connaissant ce nom peut lire tes alertes.
// Choisis un nom long et impossible à deviner, par exemple : meteo-meudo-4h8x2k9q
#define NTFY_SUJET   "meteo-change-moi-xxxxxxxx"
#define NTFY_SERVEUR "https://ntfy.sh"

// Fuseau horaire de la France, heure d'été comprise
#define FUSEAU_HORAIRE "CET-1CEST,M3.5.0,M10.5.0/3"

// Altitude de la station en mètres, pour ramener la pression au niveau de la mer
#define ALTITUDE_M 150

// Correction de température en °C (voir le README de l'étape 1)
#define CORRECTION_TEMPERATURE 0.0

// Alertes de température : une notification quand on sort de cette plage, une seule fois
// jusqu'au retour à la normale. Mets des valeurs extrêmes pour les désactiver.
#define ALERTE_TEMP_MIN 16.0
#define ALERTE_TEMP_MAX 27.0

// Alerte tempête : chute de pression en hPa sur 3 h
#define ALERTE_CHUTE_PRESSION 4.0
