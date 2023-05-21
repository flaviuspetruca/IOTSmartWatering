#ifndef GLOBALS_H
#define GLOBALS_H
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <Servo.h>

#define MSG_BUFFER_SIZE  (50)

// OLED
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

// BMP 
#define BMP_SCK  (13)
#define BMP_MISO (12)
#define BMP_MOSI (11)
#define BMP_CS   (10)

// MODES
#define AUTOMATIC "AUTOMATIC"
#define MANUAL "MANUAL"

extern WiFiClient espClient;
extern WiFiManager wm;
extern unsigned long lastMsg;
extern char msg[MSG_BUFFER_SIZE];

extern int nrOfScreens;

extern Servo myservo;
extern const int initialPositionServo;

extern String currentMode;
extern String lastMode;

// PINS
extern const int selPin;
extern const int analogPin;
extern const int flashBtn;
extern const int modeBtn;
extern const int modeLED;
extern const int relayPin;
extern const int waterContainerLed;

extern int currentWaterLeft;
extern bool wateringState;

extern int lastMoistureValue;
extern int lastLightValue;
extern float temperature;
extern float pressure;

extern unsigned long lastWateringTime;
extern unsigned long betweenWatering;

#endif
