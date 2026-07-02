#ifndef CONFIG_H
#define CONFIG_H

// Configuration options for the water heating system

//=========================INCLUDE==================================//

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal.h>
#include <ESP32Encoder.h>
#include <PID_v1.h>

//=========================BROCHES==================================//

// Broche OneWire du DS18B20 (D23 sur ESP 32 DEVKIT V1)
#define ONE_WIRE_BUS 23

// Broches pour l'écran LCD (D15, D2, D4, D5, D18, D19 sur ESP 32 DEVKIT V1)
#define LCD_RS 15
#define LCD_EN 2
#define LCD_D4 4
#define LCD_D5 5
#define LCD_D6 18
#define LCD_D7 19

// Broches de l'encodeur rotatif (D33, D32, D35 sur ESP 32 DEVKIT V1)
#define CLK 33
#define DT 32
#define SW 35

// Broches Heater (D34 sur ESP 32 DEVKIT V1)
#define HEATER_PIN 34

// Broches des LED RED & GREEN (D25, D26 sur ESP 32 DEVKIT V1)
#define RED_LED_PIN 25
#define GREEN_LED_PIN 26

//=========================PARAMETRES==================================//   


double setpoint = 60.0, input, output, setpoint_cache = 60.0; // Variables pour le PID

double Kp = 2.0, Ki = 5.0, Kd = 1.0; // Paramètres PID

const double temperatureThreshold = 1.5; // Seuil de température pour le contrôle du chauffage

// ── Limites de température (°C) ─────────────────────────────────
#define TEMP_MIN      30.0
#define TEMP_MAX      100.0
#define TEMP_STEP     1.0     // Pas de réglage par clic encodeur
#define HISTERESIS     0.5     // Hystérésis pour éviter les oscillations du chauffage
// ── Encodeur rotatif ────────────────────────────────────────────
volatile int encoderPos  = 0;
volatile int lastCLKstate;

// ── Anti-rebond bouton ───────────────────────────────────────────
// D35 est relié à GND, donc repos = LOW
bool          lastBtnState  = LOW;    // repos = LOW sur D35
unsigned long btnPressTime  = 0;
bool          btnHandled    = false;

// ── Timing ──────────────────────────────────────────────────────
unsigned long lastTempRead  = 0;
unsigned long lastDisplay   = 0;
const unsigned long TEMP_INTERVAL    = 2000;
const unsigned long DISPLAY_INTERVAL = 400;

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
//=========================CONFIGURATION==================================//

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
ESP32Encoder encoder;
PID myPID(&input, &output, &setpoint, Kp, Ki, Kd, DIRECT);

//=========================FONCTIONS==================================//

// Lecture brute DS18B20
float readTemperatureC() {
  sensors.requestTemperatures();
  const float temp = sensors.getTempCByIndex(0);
  if (temp == DEVICE_DISCONNECTED_C) {
    heaterOn = false; // Désactiver le chauffage si le capteur est déconnecté
    return NAN;
  }
  input = (double)temp; // Mettre à jour la variable d'entrée pour le PID
  return temp;
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


void afficheurLCD() {
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
}

#endif // CONFIG_H