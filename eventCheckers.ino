bool waterLedState = false;
unsigned long lastHighLed = 0;

const int btnInterval = 500;
int lastButtonState = LOW;

const int waterCheckInterval = 1000;
unsigned long lastWaterCheck = 0;
const int wateringInterval = 5000;
const int waterLedInterval = 1500;
const int mlPerSec = 30;

// Times
unsigned long lastFlashBtnPress = 0;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
unsigned long lastSoilMeasureTime = 0;
unsigned long soilInterval = 600000;
unsigned long startedWatering = 0;
unsigned long startedWateringUpdate = 0;


// Function checks for the right time to water, only if in AUTOMATIC MODE
// Calling this function only when retrieving a new value for the moisture sensor. The function also checks if its time for watering
void checkPossibleWater() {
  if (currentMode == MANUAL) {
    return;
  }
  // check if moisture is low and time passed after last watering, for the sensor to get the data
  if (lastMoistureValue < 250 && lastMoistureValue != -1 && millis() - lastWateringTime >=  betweenWatering && lastLightValue < 400) { 
    if (!wateringState){
      water();
    } 
  }
}

void checkDecreasingWaterLevel() {
  unsigned long now = millis();
  if (wateringState && now - lastWaterCheck >= waterCheckInterval) {
    Serial.println(currentWaterLeft - ((now - startedWateringUpdate) / 1000) * mlPerSec);
    if (currentWaterLeft >= ((now - startedWateringUpdate) / 1000) * mlPerSec) {
      currentWaterLeft -= ((now - startedWateringUpdate) / 1000) * mlPerSec;
      startedWateringUpdate = lastWaterCheck = now;
      publishWaterLeft(); 
    } else {
      currentWaterLeft = 0;
      startedWateringUpdate = lastWaterCheck = now;
      publishWaterLeft(); 
    }
  }
}
void checkLowWater() {
   if (currentWaterLeft < 200) {
      waterLedState = true;
      if (nrOfScreens <= 3) {
        nrOfScreens++;
      }
   } else {
      waterLedState = false;
      if (nrOfScreens > 3) {
        nrOfScreens--;
      }
   }
}

//Function to stop watering, only in automatic mode
void stopWateringChecker() {
  int temperatureDelay = 0;
  if (temperature <= 20) {
    temperatureDelay = 1;
  }
  if (millis() - startedWatering >= wateringInterval - temperatureDelay && wateringState && currentMode == AUTOMATIC) {
    stopWatering();
  }
}

//Function to check mode in order to change built in led state
void modeChecker() {
  if (currentMode == AUTOMATIC) {
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(modeLED, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(modeLED, HIGH);
  }
}

// Water container low on water make led blink
void blinkWaterLed() {
  if (waterLedState && millis() - lastHighLed >= waterLedInterval) {
    digitalWrite(waterContainerLed, HIGH);
    lastHighLed = millis();
  } else {
    digitalWrite(waterContainerLed, LOW);
  }
}

//Function checks for flash btn press in order to change use mode.
void btnPressChecker() {
  int flashBtnState = digitalRead(flashBtn) && digitalRead(modeBtn);
  if (flashBtnState != lastButtonState) {
    lastDebounceTime = millis();
  }
  if (millis() - lastDebounceTime > debounceDelay) {
    if (millis() - lastFlashBtnPress >= btnInterval || lastFlashBtnPress == 0) {
      lastFlashBtnPress = millis();
      changeMode("");
    }
  }
}

void changeMode(String modeValue) {
  Serial.println(modeValue);
  lastMode = currentMode;
  if (modeValue == "") {
    if (currentMode == AUTOMATIC) {
      currentMode = MANUAL;
    } else {
      currentMode = AUTOMATIC;
    }
    publishMode(currentMode);
  } else {
    currentMode = modeValue;
  }
}

void checkers() {
  checkDecreasingWaterLevel();
  checkLowWater();
  blinkWaterLed();
  modeChecker();
  stopWateringChecker();
  btnPressChecker();
}
