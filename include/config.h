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

typedef enum {
    SLEEP_MODE,
    HEATING_MODE,
    ERROR_MODE
} SystemState; // Enumération pour les différents états du système

double setpoint, input, output; // Variables pour le PID

double Kp = 2.0, Ki = 5.0, Kd = 1.0; // Paramètres PID

const double temperatureThreshold = 1.5; // Seuil de température pour le contrôle du chauffage

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
    return NAN;
  }
  return temp;
}


#endif // CONFIG_H