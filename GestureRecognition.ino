#include "I2Cdev.h"
#include "MPU6050.h"

// Arduino Wire library is required if I2Cdev I2CDEV_ARDUINO_WIRE implementation
// is used in I2Cdev.h
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif
MPU6050 accelgyro;

int16_t ax, ay, az;
int16_t gx, gy, gz;


int zArray[6] = {};
int zArrayLength = 6;
int yArray[6] = {};
int yArrayLength = 6;
int initializationValue = 0;
int aboveZPoint = 2500;
int stopWateringPoint = -15000;
bool isWatering = false;
unsigned long lastDataRequest = 0;
int dataRequestInterval = 1000;


#define OUTPUT_READABLE_ACCELGYRO


#define LED_PIN 13
bool blinkState = false;

void setup() { 
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif

    Serial.begin(38400);
    accelgyro.initialize();
    initializeZArray();
    initializeYArray();
}

void initializeZArray() {
  initializationValue = 0;
  while(initializationValue < zArrayLength) {
    accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    zArray[initializationValue++] = gz;
  }
}

void initializeYArray() {
  initializationValue = 0;
  while(initializationValue < yArrayLength) {
    accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    yArray[initializationValue++] = gy;
  }
}

void checkZArrayValues() {
  if (isWatering == true) {
    return;
  }
  int avg = 0;
  for (int i = 1 ; i < zArrayLength ; i++) {
    if (zArray[i] < zArray[i-1]) {
      return;
    }
  }
  if(zArray[0] > -5000 && zArray[0] < 5000 && zArray[zArrayLength - 1] > 8000){
     Serial.println("water");
     isWatering = true;
  }
}

void addZNewValue() {
  for (int i = 0 ; i < zArrayLength - 1 ; i++) {
    zArray[i] = zArray[i+1];
  }
  zArray[zArrayLength-1] = gz;
}

void checkStopWatering() {
  if (isWatering == true) {
    for (int i = 1 ; i < zArrayLength ; i++) {
      if (zArray[i] > zArray[i-1]) {
        return;
      }
    }
    if(zArray[0] > -5000 && zArray[0] < 5000 && zArray[zArrayLength - 1] < -8000){
      isWatering = false;
      Serial.println("stopWater");
    }
  }
}

void checkYArrayValues() {
  if (isWatering == true) {
    return;
  }
  for (int i = 1 ; i < yArrayLength ; i++) {
    if (yArray[i] < yArray[i-1]) {
      return;
    }
  }
  if(yArray[0] > -5000 && yArray[0] < 5000 && yArray[yArrayLength - 1] > 18000 && (millis() - lastDataRequest) >= dataRequestInterval){
     Serial.println("data");
     lastDataRequest = millis();
  }
}

void addYNewValue() {
  for (int i = 0 ; i < yArrayLength - 1 ; i++) {
    yArray[i] = yArray[i+1];
  }
  yArray[yArrayLength-1] = gy;
}


void loop() {
    accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    checkZArrayValues();
    addZNewValue();
    checkStopWatering();
    checkYArrayValues();
    addYNewValue();
    delay(100);
    
}
