#include "config.h"



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); // Initialisation de la communication série
  sensors.begin(); // Initialisation du capteur de température DS18B20
  lcd.begin(16, 2); // Initialisation de l'écran LCD
  lcd.createChar(0, degreeChar); // Création du caractère °
  encoder.attachSingleEdge(CLK, DT); // Initialisation de l'encodeur rotatif
  encoder.setCount(0); // Réinitialisation du compteur de l'encodeur
  myPID.SetMode(AUTOMATIC); // Activation du PID
  myPID.SetOutputLimits(0, 255); // Limitation de la sortie du PID entre 0 et 255 (pour PWM)
  pinMode(RED_LED_PIN, OUTPUT); // Configuration de la broche de la LED rouge en sortie
  pinMode(GREEN_LED_PIN, OUTPUT); // Configuration de la broche de la LED verte en sortie
  digitalWrite(RED_LED_PIN, HIGH); // Éteindre la LED rouge au démarrage
  digitalWrite(GREEN_LED_PIN, LOW); // Éteindre la LED verte au démarrage
  lastCLKstate = digitalRead(CLK); // Lecture de l'état initial du signal CLK de l'encodeur
  attachInterrupt(digitalPinToInterrupt(CLK), handleEncoderInterrupt, CHANGE); // Attachement de l'interruption pour l'encodeur rotatif sur le signal CLK
}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long currentMillis = millis();

  //___1. Lecture de la température toutes les 2000 ms _________________________________
  if (currentMillis - lastTempRead >= TEMP_INTERVAL) {
    input = (double)readTemperatureC();
    lastTempRead = currentMillis;
  }

  //___2. Gestion de l'encodeur rotatif pour le réglage de la température consignée ______
  if (encoderPos != 0) {
    heaterOn = false;
    enreglage = true;
    setpoint_cache += (double)encoderPos * 0.5; // Incrémenter ou décrémenter la consigne par pas de 0.5°C
    if (setpoint_cache < TEMP_MIN) {
      setpoint_cache = TEMP_MIN; // Limiter la consigne à 0°C minimum
    } else if (setpoint_cache > TEMP_MAX) {
      setpoint_cache = TEMP_MAX; // Limiter la consigne à 100°C maximum
    }
    setpoint = setpoint_cache; // Mettre à jour la consigne pour le PID
    encoderPos = 0; // Réinitialiser le compteur de l'encodeur
  }

  //___3. Gestion du Bouton poussoir pour activer le heater après la définition de la température consigne et arrêt d'urgence si le temps de pression est supérieur à 800 ms ______________________
  bool btnState = digitalRead(SW);
  if (btnState == HIGH && lastBtnState == LOW) {
    btnPressTime = currentMillis; // Enregistrer le temps de pression du bouton
    btnHandled = false; // Réinitialiser l'état de traitement du bouton
  } else if (btnState == LOW && lastBtnState == HIGH) {
    if (!btnHandled && (30 < currentMillis - btnPressTime < 800)) {
      heaterOn = !heaterOn; // Inverser l'état du chauffage si le bouton est relâché avant 800 ms
      btnHandled = true; // Marquer le bouton comme traité
    }
  } else if (btnState == HIGH && (currentMillis - btnPressTime >= 800)) {
    heaterOn = false; // Arrêt d'urgence du chauffage si le bouton est maintenu enfoncé pendant plus de 800 ms
    btnHandled = true; // Marquer le bouton comme traité
  }
  lastBtnState = btnState; // Mettre à jour l'état précédent du bouton
  
  
  //___4. Contrôle du chauffage TOR/PID ____________________________
  if (heaterOn) {
    if (0 < setpoint - input <= HISTERESIS) {
      myPID.Compute(); // Calculer la sortie du PID
      analogWrite(HEATER_PIN, (int)output); // Appliquer la sortie du PID au chauffage
    } else if (setpoint - input > HISTERESIS) {
      analogWrite(HEATER_PIN, 255); // Allumer le chauffage à pleine puissance si la température est inférieure à la consigne moins l'hystérésis
    } else {
      analogWrite(HEATER_PIN, 0); // Éteindre le chauffage si la température est supérieure à la consigne
      heaterOn = false; // Désactiver le chauffage si la température est supérieure à la consigne
    }
  } else {
    analogWrite(HEATER_PIN, 0); // Éteindre le chauffage si désactivé
  }

  //___5. Affichage sur l'écran LCD toutes les 400 ms ____________________________
  if (currentMillis - lastDisplay >= DISPLAY_INTERVAL) {
    afficheurLCD(); // Mettre à jour l'affichage LCD
    lastDisplay = currentMillis;
  }
} 