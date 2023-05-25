Adafruit_BMP280 bmp;

int getLight() {
  if (millis() - lastSoilMeasureTime < soilInterval) {
    digitalWrite(selPin, LOW);
    int light = analogRead(analogPin);
    lastLightValue = light;
    return light / 10;
  }
  return -1;
}

//Function that gets moisture data. Only once a minute and setting the pin back to Low to avoid corrosion.
int getSoilMoisture(bool refresh) {
  int soilMoisture;
  int avg = 0;
  if (millis() - lastSoilMeasureTime >= soilInterval || lastSoilMeasureTime == 0 || refresh == true) {
    digitalWrite(selPin, HIGH);
    delay(100);
    for(int i = 0 ; i < 3 ; i++) {
      soilMoisture = analogRead(analogPin);
      avg += soilMoisture;  
    }
    digitalWrite(analogPin, LOW);
    avg /= 3;
    lastSoilMeasureTime = millis();
    lastMoistureValue = avg;
    checkPossibleWater();
    return avg / 10;
  }
  return -1;
}

int getTemperature() {
  return bmp.readTemperature();
}

int getPressure() {
  return bmp.readPressure();
}
