#ifndef CONFIG_H
#define CONFIG_H

// Configuration options for the water heating system

//=========================INCLUDE==================================//

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <PID_v1.h>

//=========================BROCHES==================================//

// Broche OneWire du DS18B20 (ne pas partager avec l'I2C)
#define ONE_WIRE_BUS 19

// Broches pour l'écran LCD (D22, D23 sur ESP 32 DEVKIT V1)
#define LCD_SDA 22
#define LCD_SCL 23


// Broches de l'encodeur rotatif (D33, D32, D35 sur ESP 32 DEVKIT V1)
#define CLK 35
#define DT 32
#define SW 33

// Broche Heater (D26 sur ESP 32 DEVKIT V1, pin de sortie PWM valide)
#define HEATER_PIN 26

// Broches des LED RED & GREEN (D25, D26 sur ESP 32 DEVKIT V1)
#define RED_LED_PIN 25
#define GREEN_LED_PIN 27

//=========================PARAMETRES==================================//


double setpoint = 100.0, input, output, setpoint_cache = 100.0, previousInput = 0.0; // Variables pour le PID

double Kp = 300.0, Ki = 0.05, Kd = 100.0; // Paramètres PID

const double temperatureThreshold = 1.5; // Seuil de température pour le contrôle du chauffage
const double COOLING_AMBIENT_TEMPERATURE = 30.0;
const double COOLING_AMBIENT_TOLERANCE = 0.5;
const double HEATING_TARGET_TOLERANCE = 0.5;
const double MINIMUM_HEATING_RISE = 2.0;

// ── Limites de température (°C) ─────────────────────────────────
#define TEMP_MIN      30.0
#define TEMP_MAX      100.0
#define TEMP_STEP     1.0     // Pas de réglage par clic encodeur
#define HISTERESIS     0.5     // Hystérésis pour éviter les oscillations du chauffage


// ---- Configuration PWM (HEATER) ----
const int PWM_FREQ       = 50;   // 50 Hz
const int PWM_RESOLUTION = 8;      // 8 bits -> valeurs 0-255
const int PWM_CHANNEL    = 0;
const int PWM_MAX_DUTY   = (1 << PWM_RESOLUTION) - 1;


// ── Encodeur rotatif ────────────────────────────────────────────
volatile int encoderPos  = 0;
volatile int lastCLKstate;

// ── Anti-rebond bouton ───────────────────────────────────────────
// SW utilise INPUT_PULLUP : repos = HIGH, appui = LOW
volatile bool buttonPressedEvent  = false;
volatile bool buttonReleasedEvent = false;
volatile unsigned long buttonPressStartMs = 0;
volatile unsigned long buttonReleaseMs    = 0;
volatile unsigned long lastButtonIrqMs    = 0;
bool          buttonIsPressed = false;
unsigned long btnPressTime    = 0;
bool          btnHandled      = false;
bool changeinput = true; // Variable pour indiquer si l'entrée a changé


// ── Timing ──────────────────────────────────────────────────────
unsigned long lastTempRead  = 0;
unsigned long lastDisplay   = 0;
const unsigned long TEMP_INTERVAL    = 1000;
const unsigned long DISPLAY_INTERVAL = 400;
const unsigned long BUTTON_DEBOUNCE  = 50; // Anti-rebond pour le bouton poussoir
const unsigned long BUTTON_LONG_PRESS = 5000; // Durée d'appui long pour réinitialiser
const unsigned long LED_BLINK_INTERVAL = 500; // Intervalle de clignotement des LED (en ms)
unsigned long lastLedBlink = 0;

const int TU_PERIODS_REQUIRED = 3; // Nombre de périodes à moyenner pour Tu
int countTu = 0; // Compteur pour déterminer le Tu de Ziegler-Nichols
unsigned long lastTuCrossingMs = 0;
unsigned long tuPeriodSumMs = 0;

// ── Caractère ° personnalisé pour LCD ───────────────────────────
byte degreeChar[8] = {
  0b00110,
  0b01001,
  0b01001,
  0b00110,
  0b00000,
  0b00000,
  0b00000,
  0b00000
};

bool enreglage = false; // Variable pour indiquer si l'enregistrement est en cours
bool heaterOn = false; // Variable pour indiquer si le chauffage est activé

// Acquisition envoyee au PC pour generer les graphes de l'experience.
unsigned long experimentStartMs = 0;
bool experimentLogging = false;
enum ExperimentPhase {
  EXPERIMENT_IDLE,
  EXPERIMENT_HEATING,
  EXPERIMENT_COOLING
};
ExperimentPhase experimentPhase = EXPERIMENT_IDLE;
double experimentStartTemperature = NAN;
double experimentMaximumTemperature = NAN;
bool heatingCompleted = false;
//=========================CONFIGURATION==================================//

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LiquidCrystal_I2C lcd(0x27, 16, 2); // Adresse I2C de l'écran LCD (0x27 ou 0x3F) et dimensions (16 colonnes, 2 lignes)
PID myPID(&input, &output, &setpoint, Kp, Ki, Kd, DIRECT);

//=========================FONCTIONS==================================//

void startExperimentLogging(unsigned long currentMillis) {
  experimentStartMs = currentMillis;
  experimentLogging = true;
  experimentPhase = EXPERIMENT_HEATING;
  experimentStartTemperature = input;
  experimentMaximumTemperature = input;
  heatingCompleted = false;
  Serial.println("EXPERIMENT_BEGIN");
  Serial.println("DATA,elapsed_s,phase,temperature_c,target_c,pid_setpoint_c,output,target_error_c,pid_error_c");
}

void stopExperimentLogging() {
  if (!experimentLogging) {
    return;
  }
  experimentLogging = false;
  experimentPhase = EXPERIMENT_IDLE;
  heatingCompleted = false;
  Serial.println("EXPERIMENT_END");
}

void markHeatingCompleted() {
  if (isnan(input)) {
    return;
  }
  if (isnan(experimentStartTemperature)) {
    experimentStartTemperature = input;
  }

  if (isnan(experimentMaximumTemperature) || input > experimentMaximumTemperature) {
    experimentMaximumTemperature = input;
  }

  const bool reachedTarget = input >= setpoint_cache - HEATING_TARGET_TOLERANCE;
  const bool actuallyHeated =
      experimentMaximumTemperature >= experimentStartTemperature + MINIMUM_HEATING_RISE;
  heatingCompleted = heatingCompleted || (reachedTarget && actuallyHeated);
}

void startCoolingLogging() {
  if (!experimentLogging || experimentPhase != EXPERIMENT_HEATING || !heatingCompleted) {
    return;
  }
  experimentPhase = EXPERIMENT_COOLING;
  Serial.println("COOLING_BEGIN");
}

void logExperimentSample(unsigned long currentMillis) {
  if (!experimentLogging || isnan(input)) {
    return;
  }

  const double targetError = setpoint_cache - input;
  const double pidError = setpoint - input;
  Serial.print("DATA,");
  Serial.print((currentMillis - experimentStartMs) / 1000.0, 3);
  Serial.print(",");
  Serial.print(experimentPhase == EXPERIMENT_COOLING ? "cooling" : "heating");
  Serial.print(",");
  Serial.print(input, 3);
  Serial.print(",");
  Serial.print(setpoint_cache, 3);
  Serial.print(",");
  Serial.print(setpoint, 3);
  Serial.print(",");
  Serial.print(output, 3);
  Serial.print(",");
  Serial.print(targetError, 3);
  Serial.print(",");
  Serial.println(pidError, 3);
}

bool coolingReachedAmbient() {
  return experimentPhase == EXPERIMENT_COOLING &&
         !isnan(input) &&
         input <= COOLING_AMBIENT_TEMPERATURE + COOLING_AMBIENT_TOLERANCE;
}

// Lecture brute DS18B20
float readTemperatureC() {
  sensors.requestTemperatures();
  const float temp = sensors.getTempCByIndex(0);
  if (temp == DEVICE_DISCONNECTED_C) {
    input = NAN; // Mettre à jour la variable d'entrée pour le PID avec une valeur NaN
    return NAN;
  }
  input = (double)temp; // Mettre à jour la variable d'entrée pour le PID
  return temp;
}
// Détermine le Tu de Ziegler-Nichols
void resetTuDetection() {
  countTu = 0;
  lastTuCrossingMs = 0;
  tuPeriodSumMs = 0;
}

void determineTu(unsigned long currentMillis) {
  if (Ki != 0.0 || Kd != 0.0 || isnan(input) || isnan(previousInput)) {
    return;
  }

  bool crossedSetpointUp = previousInput < setpoint && input >= setpoint;
  if (!crossedSetpointUp) {
    return;
  }

  if (lastTuCrossingMs == 0) {
    lastTuCrossingMs = currentMillis;
    Serial.println("Tu: premier passage montant du setpoint detecte");
    return;
  }

  unsigned long periodMs = currentMillis - lastTuCrossingMs;
  lastTuCrossingMs = currentMillis;

  if (periodMs == 0) {
    return;
  }

  tuPeriodSumMs += periodMs;
  countTu++;

  Serial.print("Tu periode ");
  Serial.print(countTu);
  Serial.print(" : ");
  Serial.print(periodMs / 1000.0, 2);
  Serial.println(" s");

  if (countTu >= TU_PERIODS_REQUIRED) {
    double Tu = (tuPeriodSumMs / (double)countTu) / 1000.0; // Convertir en secondes
    double Ku = Kp; // Gain critique utilise pendant l'essai

    if (Tu <= 0.0) {
      resetTuDetection();
      return;
    }

    Serial.print("Tu de Ziegler-Nichols : ");
    Serial.print(Tu, 2);
    Serial.println(" secondes");

    // Reglage Ziegler-Nichols PID classique : Kp=0.6Ku, Ti=Tu/2, Td=Tu/8
    Kp = 0.6 * Ku;
    Ki = 1.2 * Ku / Tu;
    Kd = 0.0;
    myPID.SetTunings(Kp, Ki, Kd);

    Serial.print("Nouveaux gains PID => Kp: ");
    Serial.print(Kp, 2);
    Serial.print(", Ki: ");
    Serial.print(Ki, 2);
    Serial.print(", Kd: ");
    Serial.println(Kd, 2);
  }
}
// Une Interruption pour l'encodeur rotatif
void IRAM_ATTR handleEncoderInterrupt() {
  int CLKstate = digitalRead(CLK);
  int DTstate = digitalRead(DT);
  if (CLKstate != lastCLKstate) {
    encoderPos += (CLKstate != DTstate) ? 1 : -1;

    lastCLKstate = CLKstate;
  }
}

void IRAM_ATTR handleButtonInterrupt() {
  unsigned long now = millis();
  if (now - lastButtonIrqMs < BUTTON_DEBOUNCE) {
    return;
  }
  lastButtonIrqMs = now;

  if (digitalRead(SW) == LOW) {
    buttonPressStartMs = now;
    buttonPressedEvent = true;
  } else {
    buttonReleaseMs = now;
    buttonReleasedEvent = true;
  }
}

void handleShortButtonPress() {
  heaterOn = !heaterOn; // Inverser l'état du chauffage
  if (heaterOn) {
    stopExperimentLogging();
    resetTuDetection(); // Nouvelle mesure Tu à chaque démarrage du chauffage
    startExperimentLogging(millis());
  } else {
    if (heatingCompleted) {
      startCoolingLogging();
    } else {
      stopExperimentLogging();
    }
  }
  enreglage = false; // Sortir du mode d'enregistrement si le bouton est pressé
}

void handleLongButtonPress() {
  setpoint_cache = 60.0;
  setpoint = setpoint_cache; // Mettre à jour la consigne pour le PID
  heaterOn = false; // Désactiver le chauffage lors de la réinitialisation
  stopExperimentLogging();
  enreglage = false;
  resetTuDetection();
}

void handleButton(unsigned long currentMillis) {
  noInterrupts();
  bool pressedEvent = buttonPressedEvent;
  bool releasedEvent = buttonReleasedEvent;
  unsigned long pressedAt = buttonPressStartMs;
  unsigned long releasedAt = buttonReleaseMs;
  buttonPressedEvent = false;
  buttonReleasedEvent = false;
  interrupts();

  if (pressedEvent) {
    buttonIsPressed = true;
    btnPressTime = pressedAt;
    btnHandled = false;
  }

  if (buttonIsPressed && !btnHandled && currentMillis - btnPressTime >= BUTTON_LONG_PRESS) {
    handleLongButtonPress();
    btnHandled = true;
  }

  if (releasedEvent) {
    unsigned long heldTime = releasedAt - btnPressTime;
    if (buttonIsPressed && !btnHandled && heldTime >= BUTTON_DEBOUNCE) {
      handleShortButtonPress();
    }
    buttonIsPressed = false;
    btnHandled = false;
  }
}

void afficheurLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  if (isnan(input)) {
    lcd.print("Err");
  } else {
    lcd.print(input, 1);
    lcd.write(byte(0)); // Affiche le caractère °
    lcd.print("C");
  }
  lcd.setCursor(0, 1);
  lcd.print("Set: ");
  lcd.print(setpoint_cache, 1);
  lcd.write(byte(0)); // Affiche le caractère °
  lcd.print("C");

  // Réglage de la température consignée avec l'encodeur rotatif
  if (enreglage) {
    lcd.setCursor(0, 1);
    lcd.print("Reg> ");
    lcd.print(setpoint_cache, 1);
    lcd.write(byte(0)); // Affiche le caractère °
    lcd.print("C");
  }
  if (heaterOn) {
    lcd.print(" H:N");
  } else {
    lcd.print(" H:F");
  }
}
// Fonction pour régler la puissance du chauffage avec une bande morte
void setHeaterPower(int power) {

  // Deux NPN → logique directe, pas d'inversion
  ledcWrite(PWM_CHANNEL, constrain(power, 0, PWM_MAX_DUTY)); // Appliquer la sortie du PID à la broche de chauffage via PWM

}
#endif // CONFIG_H
