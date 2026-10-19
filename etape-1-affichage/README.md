# Étape 1 : la station sur écran OLED (sans Wi-Fi)

L'ESP32 lit le BME280 toutes les 5 secondes et affiche la température, l'humidité et la pression sur l'écran OLED. Il garde la pression en mémoire toutes les 10 minutes : sur 3 heures, la variation donne la tendance (en hausse, stable, en baisse) et une petite prévision, exactement comme un baromètre à aiguille.

Le code compile avec PlatformIO (ESP32 DevKit, framework Arduino) mais n'a pas encore été testé sur une vraie carte.

## Câblage

Le BME280 et l'écran se partagent le même bus I2C : on les branche en parallèle.

| De | Vers |
| --- | --- |
| VCC du BME280 et VCC de l'écran | **3V3** de l'ESP32 |
| GND du BME280 et GND de l'écran | GND |
| SDA du BME280 et SDA de l'écran | GPIO21 |
| SCL du BME280 et SCL de l'écran | GPIO22 |

Éloigne le BME280 de l'ESP32 d'une dizaine de centimètres : l'ESP32 chauffe et fausse la température.

## Mise en route

1. Copie `include/config.example.h` en `include/config.h` et mets l'altitude de chez toi (Géoportail ou appli GPS).
2. Branche la carte en USB et lance `pio run -t upload`, puis `pio device monitor`.
3. Après une heure, la tendance du baromètre s'affiche. Avant, l'écran indique « calcul (1 h) ».

## Ce que dit le baromètre

| Variation sur 3 h | Tendance | Prévision affichée |
| --- | --- | --- |
| plus de +1,6 hPa | en hausse | Amélioration |
| entre -1,6 et +1,6 hPa | stable | Beau temps (> 1020 hPa), Temps variable, Temps couvert (< 1005 hPa) |
| moins de -1,6 hPa | en baisse | Dégradation |
| moins de -4 hPa | en baisse | Tempête possible |

C'est la règle des baromètres de nos grands-parents : simple, et étonnamment juste à 12 heures. On pourra comparer avec la prévision de Météo-France dans la vidéo.

## Réglages (`config.h`)

- `ALTITUDE_M` : sert à ramener la pression au niveau de la mer, pour comparer avec les applis météo. Sans elle, tu lirais environ 12 hPa de moins tous les 100 m d'altitude.
- `CORRECTION_TEMPERATURE` : après une heure de fonctionnement, compare avec un thermomètre fiable et mets l'écart ici (exemple : `-1.2`).

## Points à vérifier ensemble

- **Adresse du BME280** : le code essaie 0x76 puis 0x77. S'il dit « BMP280 », le module reçu est un faux BME280 (pas d'humidité).
- **Écran noir** : certains écrans sont à l'adresse 0x3D au lieu de 0x3C (petit pont de soudure au dos) ; change `ADRESSE_ECRAN` dans `main.cpp`.
- **Échauffement** : le capteur est lu en mode « forcé », qui le laisse dormir entre deux mesures. S'il reste trop chaud, on espacera les mesures.

## Ce qu'affiche le moniteur série

```
Démarrage de la station météo
21.4 °C  48 %  1016.2 hPa  tendance calcul (1 h) (+nan hPa/3 h)
...
21.2 °C  49 %  1014.1 hPa  tendance en baisse (-2.1 hPa/3 h)
```
