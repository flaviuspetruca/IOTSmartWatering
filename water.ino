const int wateringPosition = 0;

void water() {
  if (currentWaterLeft == 0) {
    startedWatering = millis();
    stopWatering();
  } else {
      startedWatering = startedWateringUpdate = millis();
      wateringState = true;
      digitalWrite(relayPin, HIGH);
      //myservo.write(wateringPosition);
  }
}

void stopWatering() {
  digitalWrite(relayPin, LOW);
  wateringState = false;
  lastWateringTime = millis();
}
