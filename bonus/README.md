# Bonus

Ces améliorations seront codées après les étapes 1 à 3. Voici comment on les fera.

## Vent et pluie

Le kit **Misol WH-SP-RG** (35 à 50 €, [AliExpress](https://www.aliexpress.com/w/wholesale-misol-wind-rain-sensor.html)) regroupe un anémomètre, une girouette et un pluviomètre à auget, vendus comme pièces détachées des stations du commerce.

- **Anémomètre** : un contact qui se ferme à chaque tour. On compte les impulsions sur une broche d'interruption : 1 impulsion par seconde = 2,4 km/h.
- **Pluviomètre** : un auget qui bascule tous les 0,28 mm de pluie, compté de la même façon.
- **Girouette** : une résistance différente pour chacune des 16 directions, lue sur une entrée analogique avec un pont diviseur.
- Ces capteurs vont plutôt sur la sonde extérieure. Pour compter le vent, elle ne pourra plus dormir longtemps : il faudra le panneau solaire.

## Qualité de l'air

- **SCD40** (15 à 25 €) : CO2 intérieur, utile pour savoir quand aérer. Il se branche sur le même bus I2C que le BME280.
- **PMS5003** (15 à 20 €) : particules fines PM2.5 et PM10, dehors.

## Panneau solaire pour la sonde

- Un panneau de **6 V et 1 à 2 W** (5 à 10 €) sur l'entrée du TP4056, ou le module **CN3791** qui tire mieux parti d'un ciel couvert.
- Même mise en garde que pour la boîte aux lettres : pas de recharge sous 0 °C. La sonde mesure justement la température, elle pourra couper la charge quand il gèle.

## Home Assistant et historique long

- Envoi des mesures en MQTT vers Home Assistant, pour garder des années d'historique et faire des graphiques à la semaine ou au mois.
- Sans Home Assistant : enregistrement sur carte micro SD au format CSV, comme le journal de la boîte aux lettres.

## Écran e-paper

Un écran e-paper de 2,9 pouces (15 €) à la place de l'OLED : lisible en plein jour, aucune consommation entre deux mises à jour, et pas d'usure de l'écran.
