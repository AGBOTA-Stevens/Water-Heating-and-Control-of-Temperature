# Water-Heating-and-Control-of-Temperature

## Generation des graphes de l'experience

Le firmware envoie les mesures de temperature au format CSV sur le port serie
pendant chaque cycle. Une phase `heating` n'est validee qu'apres une montee
d'au moins 2 °C et l'atteinte de la cible. Si le chauffage est ensuite arrete
par le bouton, la phase `cooling` est enregistree jusqu'au retour a 30 °C.
Un arret avant la cible, ou un changement de consigne avec l'encodeur, ne
declenche donc pas de faux refroidissement.

Le script Python genere ensuite quatre images PNG adaptees a un memoire :

- `results/temperature_vs_time.png` : temperature mesuree, cible et temps
  d'atteinte dans une bande de ±0,5 °C ;
- `results/pid_precision.png` : erreur de regulation et commande PWM, avec
  l'erreur absolue moyenne et maximale par rapport a la consigne effectivement
  utilisee par le PID. Le CSV conserve aussi l'erreur par rapport a la cible
  affichee.
- `results/cooling_vs_time.png` : chute de temperature apres chauffage jusqu'a
  la temperature ambiante ;
- `results/performance_summary.png` : durees de chauffage, refroidissement,
  cycle total et atteinte de la cible.

Les axes temporels utilisent la date locale de l'ordinateur qui capture les
donnees, au format `JJ/MM/AAAA HH:MM:SS`. Cette date est aussi conservee dans
la colonne `timestamp` du CSV.

Installer les dependances puis lancer la capture avant d'appuyer sur le bouton
de l'encodeur pour demarrer le chauffage :

```bash
python3 -m pip install matplotlib pyserial
python3 scripts/generate_graphs.py --port /dev/ttyUSB0
```

Appuyer a nouveau sur le bouton pour arreter le chauffage. Les deux PNG et le
fichier `results/experiment_data.csv` sont alors crees. Le port peut etre
remplace par celui de la carte (`/dev/ttyACM0`, par exemple).