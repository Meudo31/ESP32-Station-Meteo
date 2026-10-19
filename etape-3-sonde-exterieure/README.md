# Étape 3 : la sonde extérieure sur batterie

Une petite sonde dehors, sur batterie, mesure la température, l'humidité et la pression toutes les 5 minutes et les envoie à la station intérieure en **ESP-NOW**, la liaison radio directe entre ESP32. Pas de connexion au Wi-Fi : l'envoi dure une fraction de seconde, puis la sonde se rendort, ce qui fait durer la batterie des mois. La station affiche dedans et dehors côte à côte, sur l'écran comme sur la page web, et prévient en cas de gel.

Il y a deux programmes :

| Dossier | Carte | Rôle |
| --- | --- | --- |
| [sonde](sonde) | ESP32-C3 SuperMini + BME280, sur batterie | Mesure et envoie toutes les 5 min, dort entre deux |
| [station](station) | La station de l'étape 2 | Reçoit la sonde, écran, page web, alertes |

Le code compile avec PlatformIO mais n'a pas encore été testé sur de vraies cartes.

## Câblage de la sonde

| De | Vers |
| --- | --- |
| VCC du BME280 | **3V3** de l'ESP32-C3 |
| GND du BME280 | GND |
| SDA du BME280 | GPIO6 |
| SCL du BME280 | GPIO7 |
| Batterie + (sortie OUT+ du TP4056) | Entrée du HT7333, et résistance 100 kΩ vers GPIO3 |
| GPIO3 | résistance 100 kΩ vers GND, et condensateur 100 nF vers GND |
| Sortie 3,3 V du HT7333 | Broche **3V3** de l'ESP32-C3 |
| Batterie - (OUT-) | GND |

Les deux résistances divisent la tension de la batterie par deux pour que l'ESP32-C3 puisse la mesurer. On alimente par la broche 3V3 avec le HT7333, comme pour la boîte aux lettres : c'est ce qui permet de tenir des mois.

La station garde le câblage de l'étape 2.

## Installation dehors

- L'électronique va dans le boîtier étanche, et le **BME280 dans l'abri anti-rayonnement**, relié par 4 fils : le BME280 doit respirer l'air extérieur, mais jamais voir le soleil ni la pluie.
- Abri à 1,5 m du sol, au nord si possible, loin des murs qui rayonnent la chaleur.
- Ajoute un sachet de gel de silice dans le boîtier contre la condensation.

## Mise en route

1. Flashe d'abord la **station** (dossier `station`) : copie et remplis son `config.h` comme à l'étape 2. Au démarrage, l'écran affiche son **adresse MAC** (aussi dans le moniteur série).
2. Dans le dossier `sonde`, copie `include/config.example.h` en `include/config.h`, mets le nom du Wi-Fi et recopie l'adresse MAC de la station dans `MAC_STATION`.
3. Flashe la sonde par l'USB-C (`pio run -t upload`). Si le téléversement échoue, maintiens le bouton BOOT en branchant la carte.
4. La sonde mesure et envoie tout de suite, puis toutes les 5 minutes. L'écran de la station passe de « Sonde : en attente » à la tension de la batterie, et la ligne « Ex » s'affiche.

Pour les essais, mets `INTERVALLE_MESURE_S` à 30 dans la sonde, puis remets 300 une fois dehors.

## Comment la sonde trouve la station

ESP-NOW doit émettre sur le même canal radio que la box, car la station y est connectée. Au premier réveil, la sonde cherche le Wi-Fi par son nom et note son canal ; elle ne s'y connecte pas, donc pas besoin du mot de passe. La station accuse réception de chaque message. Après trois envois sans réponse, la sonde recherche le canal, au cas où la box en aurait changé.

## Nouvelles alertes (`config.h` de la station)

| Alerte | Réglage | Repart quand |
| --- | --- | --- |
| Risque de gel (température dehors) | `ALERTE_GEL` (3 °C) | il fait 1 °C de plus |
| Sonde muette | `ALERTE_SONDE_MUETTE_MIN` (30 min) | la sonde parle de nouveau, avec une notification |
| Batterie de la sonde faible | `ALERTE_BATTERIE_V` (3,4 V) | la batterie est rechargée |

Les alertes de température de l'étape 2 portent maintenant sur l'intérieur.

## Points à vérifier ensemble

- **Mesure de la batterie** : compare avec le multimètre et règle `CORRECTION_BATTERIE` dans la sonde (tension réelle / tension affichée).
- **Portée** : ESP-NOW porte plus loin que le Wi-Fi classique. Si des messages se perdent, le moniteur série de la sonde affiche les échecs, et la station indique combien d'envois ont raté avant chaque message reçu.
- **Autonomie** : à mesurer au multimètre en sommeil. Avec le HT7333 et le pont diviseur (21 µA), on vise plusieurs mois sur une 18650. Ça fera un bon Short.
- **Froid** : une batterie Li-ion ne se recharge pas sous 0 °C. En hiver, rentre la sonde pour la recharger.

## Ce qu'affiche le moniteur série de la sonde

```
Mesure n.1 : 8.2 °C  87 %  998.4 hPa  batterie 4.12 V
Wi-Fi « MaBox » trouvé sur le canal 6 (-71 dBm)
Envoyé à la station (canal 6)
Dodo pendant 300 s
```
