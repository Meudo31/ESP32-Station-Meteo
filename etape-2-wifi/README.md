# Étape 2 : page web, courbes sur 24 h et alertes sur le téléphone

Même station qu'à l'étape 1, qui se connecte en plus au Wi-Fi. Elle garde 24 heures de mesures en mémoire (un point toutes les 10 minutes) et les affiche en courbes sur une page web, accessible depuis n'importe quel téléphone ou ordinateur de la maison. Elle envoie aussi une notification via l'appli **ntfy** quand la température sort de la plage choisie ou quand la pression chute vite.

Le code compile avec PlatformIO (ESP32 DevKit, framework Arduino) mais n'a pas encore été testé sur une vraie carte.

## Matériel et câblage

Rien de nouveau : même matériel et même câblage qu'à l'[étape 1](../etape-1-affichage).

## Mise en route

1. Installe l'appli **ntfy** et abonne-toi à un sujet au nom long et impossible à deviner.
2. Copie `include/config.example.h` en `include/config.h`, puis remplis le Wi-Fi, le sujet ntfy, l'altitude et les seuils d'alerte.
3. Lance `pio run -t upload`, puis `pio device monitor`.
4. Au démarrage, l'écran affiche l'adresse IP pendant 5 secondes et tu reçois « Station météo en service ».
5. Sur un téléphone connecté au même Wi-Fi, ouvre **http://meteo.local** (ou l'adresse IP si ton téléphone ne connaît pas les noms `.local`, ce qui arrive sur certains Android).

## La page web

- Valeurs actuelles : température, humidité, pression, tendance sur 3 h et prévision.
- Trois courbes sur 24 heures, tracées par la page elle-même : pas besoin d'Internet, ni de compte, ni de service en ligne.
- Elle se met à jour toute seule chaque minute et suit le thème clair ou sombre du téléphone.
- Les données brutes sont en JSON à l'adresse `/donnees`, pratique pour la suite.

L'historique est en mémoire vive : il repart de zéro si la carte redémarre. On pourra le garder sur une carte SD ou en mémoire flash plus tard.

## Alertes (`config.h`)

| Alerte | Réglage | Repart quand |
| --- | --- | --- |
| Température basse | `ALERTE_TEMP_MIN` (16 °C) | la température remonte de 0,5 °C |
| Température haute | `ALERTE_TEMP_MAX` (27 °C) | la température redescend de 0,5 °C |
| Risque de tempête | `ALERTE_CHUTE_PRESSION` (4 hPa en 3 h) | la chute ralentit de moitié |

Chaque alerte n'est envoyée qu'une fois, puis attend le retour à la normale : pas de rafale de notifications.

## Points à vérifier ensemble

- **Wi-Fi coupé** : la station continue d'afficher et de mesurer, et se reconnecte toute seule.
- **Heure** : elle vient d'Internet (NTP) et sert à dater les courbes. Sans Internet, les courbes s'affichent quand même, sans les heures.
- **Mémoire flash** : le programme occupe 78 % de la mémoire de l'ESP32, il reste de la marge.
