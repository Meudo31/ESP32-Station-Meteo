# Station météo connectée

Une station météo maison qui mesure la température, l'humidité et la pression, prévoit le temps grâce à la tendance du baromètre, affiche des courbes sur le téléphone et prévient en cas de gel ou de tempête. Le projet avance par étapes, du plus simple au plus complet ; chaque étape est un projet PlatformIO autonome et peut faire l'objet d'une vidéo.

| Étape | Dossier | Ce que ça fait | Carte |
| --- | --- | --- | --- |
| 1 | [etape-1-affichage](etape-1-affichage) | Température, humidité, pression et tendance du baromètre sur un écran OLED | ESP32 DevKit |
| 2 | [etape-2-wifi](etape-2-wifi) | Page web avec courbes sur 24 h, alertes ntfy (température, tempête) | ESP32 DevKit |
| 3 | [etape-3-sonde-exterieure](etape-3-sonde-exterieure) | Sonde extérieure sur batterie qui envoie ses mesures en ESP-NOW, alerte gel | ESP32-C3 SuperMini + ESP32 DevKit |
| Bonus | [bonus](bonus) | Vent et pluie, qualité de l'air, panneau solaire, Home Assistant | |

Le code des quatre programmes compile avec PlatformIO mais n'a pas encore été testé sur de vraies cartes.

## Liste d'achat par étape

Prix approximatifs constatés en France (AliExpress le moins cher, Amazon le plus rapide) ; chaque étape réutilise ce qui a été acheté avant.

### Étape 1 (environ 15 à 25 €)

| Pièce | Prix approx. | Rechercher |
| --- | --- | --- |
| ESP32 DevKit (ESP32-WROOM-32, 38 broches, USB-C) | 5 à 8 € | [Amazon](https://www.amazon.fr/s?k=esp32+devkit+usb-c) · [AliExpress](https://www.aliexpress.com/w/wholesale-esp32-devkit-usb-c.html) |
| Capteur **BME280** en 3,3 V (attention aux faux : voir plus bas) | 4 à 8 € | [Amazon](https://www.amazon.fr/s?k=bme280+3.3v) · [AliExpress](https://www.aliexpress.com/w/wholesale-bme280-3.3v.html) |
| Écran OLED 0,96 pouce I2C SSD1306 (128 x 64, 4 broches) | 3 à 6 € | [Amazon](https://www.amazon.fr/s?k=oled+0.96+ssd1306+i2c) · [AliExpress](https://www.aliexpress.com/w/wholesale-oled-0.96-ssd1306-i2c.html) |
| Plaque d'essai + fils Dupont | 3 à 6 € | [Amazon](https://www.amazon.fr/s?k=breadboard+fils+dupont) · [AliExpress](https://www.aliexpress.com/w/wholesale-breadboard-dupont.html) |

**Le piège du BME280** : beaucoup de modules vendus « BME280 » sont des BMP280, qui ne mesurent pas l'humidité. Le BME280 a une puce carrée (2,5 mm de côté), le BMP280 une puce rectangulaire. Au démarrage, le moniteur série dit lequel tu as reçu. Achète-en deux d'un coup si tu comptes faire l'étape 3.

### Étape 2 (rien de plus)

Tout se passe dans le code : Wi-Fi, page web et appli **ntfy** (gratuite) sur le téléphone.

### Étape 3 (environ 20 à 35 € en plus)

| Pièce | Prix approx. | Rechercher |
| --- | --- | --- |
| ESP32-C3 SuperMini | 3 à 4 € | [Amazon](https://www.amazon.fr/s?k=esp32-c3+supermini) · [AliExpress](https://www.aliexpress.com/w/wholesale-esp32-c3-supermini.html) |
| Second BME280 en 3,3 V | 4 à 8 € | voir étape 1 |
| Batterie 18650 de marque (Samsung, LG, Panasonic) + support | 5 à 8 € | [Amazon](https://www.amazon.fr/s?k=batterie+18650+samsung+support) |
| Module de charge TP4056 avec protection (6 broches, USB-C) | 1 à 2 € | [Amazon](https://www.amazon.fr/s?k=tp4056+usb-c+protection) · [AliExpress](https://www.aliexpress.com/w/wholesale-tp4056-type-c-protection.html) |
| Régulateur faible chute HT7333 (ou MCP1700-3302) | 1 € | [AliExpress](https://www.aliexpress.com/w/wholesale-ht7333.html) |
| 2 résistances de 100 kΩ + 1 condensateur de 100 nF | moins d'1 € (ou 5 € le kit) | [Amazon](https://www.amazon.fr/s?k=kit+resistances+condensateurs) |
| Boîtier étanche IP65 pour l'électronique | 3 à 6 € | [Amazon](https://www.amazon.fr/s?k=boitier+etanche+ip65+electronique) |
| Abri anti-rayonnement : 6 à 8 sous-pots blancs, une tige filetée M6, écrous (fait maison) ou abri tout fait | 5 à 8 € (25 € tout fait) | [Amazon](https://www.amazon.fr/s?k=abri+radiation+station+meteo) |

L'abri est indispensable : un capteur au soleil peut afficher 10 °C de trop. Les modèles faits de sous-pots empilés font un bon passage de vidéo.

### Bonus

Voir le [README des bonus](bonus) : kit anémomètre, girouette et pluviomètre (35 à 50 €), capteur de CO2 SCD40 (15 à 25 €), panneau solaire 6 V (5 à 10 €).

## Mise en route

Chaque dossier a son propre README avec le câblage et la mise en route. Dans tous les cas : copier `include/config.example.h` en `include/config.h`, remplir les valeurs, puis `pio run -t upload` depuis le dossier du programme. Le fichier `config.h` est exclu du dépôt.
