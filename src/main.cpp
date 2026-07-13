#include "config.h"



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); // Initialisation de la communication série
  sensors.begin(); // Initialisation du capteur de température DS18B20
  Wire.begin(LCD_SDA, LCD_SCL); // Initialisation du bus I2C pour l'écran LCD
  lcd.begin(16, 2); // Initialisation de l'écran LCD
  lcd.createChar(0, degreeChar); // Création du caractère °
  lcd.backlight(); // Activation du rétroéclairage de l'écran LCD
  lcd.clear(); // Effacement de l'écran LCD
  myPID.SetMode(AUTOMATIC); // Activation du PID
  myPID.SetOutputLimits(0, 255); // Limitation de la sortie du PID entre 0 et 255 (pour PWM)
  pinMode(SW, INPUT_PULLUP); // SW au repos = HIGH, appuyé = LOW
  pinMode(CLK, INPUT); // Configuration de la broche CLK de l'encodeur en entrée
  pinMode(DT, INPUT); // Configuration de la broche DT de l'encode
  ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcAttachPin(HEATER_PIN, PWM_CHANNEL);
  pinMode(RED_LED_PIN, OUTPUT); // Configuration de la broche de la LED rouge en sortie
  pinMode(GREEN_LED_PIN, OUTPUT); // Configuration de la broche de la LED verte en sortie
  digitalWrite(RED_LED_PIN, LOW); // LED rouge éteinte au démarrage
  digitalWrite(GREEN_LED_PIN, LOW); // Éteindre la LED verte au démarrage
  lastCLKstate = digitalRead(CLK); // Lecture de l'état initial du signal CLK de l'encodeur
  attachInterrupt(digitalPinToInterrupt(CLK), handleEncoderInterrupt, CHANGE); // Attachement de l'interruption pour l'encodeur rotatif sur le signal CLK
  attachInterrupt(digitalPinToInterrupt(SW), handleButtonInterrupt, CHANGE); // Appui et relâchement du bouton poussoir
}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long currentMillis = millis();

  //___0. Gestion du bouton poussoir de l'encodeur _________________________________
  handleButton(currentMillis);

  //___1. Lecture de la température toutes les 2000 ms _________________________________
  if (currentMillis - lastTempRead >= TEMP_INTERVAL) {
    readTemperatureC();
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

  //___4. Contrôle du chauffage TOR/PID ____________________________
  if (heaterOn) {
    double error = setpoint - input;
    if (error > 0 && error <= HISTERESIS || error < 0 && error >= -HISTERESIS) {
      myPID.Compute(); // Calculer la sortie du PID
      ledcWrite(PWM_CHANNEL, (int)output); // Appliquer la sortie du PID au chauffage
    } else if (error > HISTERESIS) {
      ledcWrite(PWM_CHANNEL, 255); // Allumer le chauffage à pleine puissance si la température est inférieure à la consigne moins l'hystérésis
    } else {
      ledcWrite(PWM_CHANNEL, 0); // Éteindre le chauffage si la température est supérieure à la consigne
      heaterOn = false; // Désactiver le chauffage si la température est supérieure à la consigne
    }
  } else {
    ledcWrite(PWM_CHANNEL, 0); // Éteindre le chauffage si désactivé
  }

  //___5. Affichage sur l'écran LCD toutes les 400 ms ____________________________
  if (currentMillis - lastDisplay >= DISPLAY_INTERVAL) {
    afficheurLCD(); // Mettre à jour l'affichage LCD
    lastDisplay = currentMillis;
  }

  //___6. Gestion des LED pour indiquer l'état du chauffage ____________________________
  if (enreglage) {
    if (currentMillis - lastLedBlink >= LED_BLINK_INTERVAL) {
      digitalWrite(RED_LED_PIN, !digitalRead(RED_LED_PIN)); // Faire clignoter la LED rouge si l'enregistrement est en cours
      digitalWrite(GREEN_LED_PIN, LOW);
      lastLedBlink = currentMillis;
    }
  } else if (heaterOn) {
    digitalWrite(RED_LED_PIN, HIGH); // Allumer la LED rouge si le chauffage est activé
    digitalWrite(GREEN_LED_PIN, LOW); // Éteindre la LED verte si le chauffage est activé
  } else {
    digitalWrite(RED_LED_PIN, LOW); // Éteindre la LED rouge si le chauffage est désactivé
    digitalWrite(GREEN_LED_PIN, HIGH); // Allumer la LED verte si le chauffage est désactivé
  }
} 
