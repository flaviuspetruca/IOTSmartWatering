#include "globals.h"

unsigned long lastMsg = 0;
WiFiManager wm;
WiFiClient espClient;

Servo myservo;
const int initialPositionServo = 90;

const int selPin = D7;
const int analogPin = A0;
const int flashBtn = D3;
const int modeBtn = D4;
const int modeLED = D1;
const int relayPin = D2;
const int waterContainerLed = D8;

int nrOfScreens = 3;

String currentMode = AUTOMATIC;
String lastMode = AUTOMATIC;

int currentWaterLeft;
bool wateringState = false;

int lastMoistureValue = -1;
int lastLightValue = 0;
float temperature = 0;
float pressure = 0;


unsigned long lastWateringTime = 0;
unsigned long betweenWatering = 60000;
