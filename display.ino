#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

int screen = 1;
const int aboutToWater = 350;
unsigned long lastScreenChangeTime = 0;
const int screenInterval = 4000;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String millisToTime(unsigned long milliseconds) {
  unsigned long allSeconds = milliseconds/1000;
  int runHours = allSeconds/3600;
  int secsRemaining = allSeconds%3600;
  int runMinutes = secsRemaining/60;
  int runSeconds = secsRemaining%60;
  String minutes = runMinutes < 10 ? "0" + String(runMinutes) : String(runMinutes);
  String seconds = runSeconds < 10 ? "0" + String(runSeconds) : String(runSeconds);
  String hours = runHours < 10 ? "0" + String(runHours) : String(runHours);
  return (hours + ":" + minutes + ":" + seconds  );
}

void displayScreen() {
  if (wateringState == true) {
    displayWatering();
    return;
  }
  switch(screen) {
    case 1:
      displayTempPress();
      break;
    case 2:
      displayMoistureAndLight();
      break;
    case 3:
      displayLastWateringTime();
      break;
    case 4:
      displayLowWater();
      break;
    default:
      displayTempPress();
      break;
  }
}

void displayWatering() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Watering..");
  display.display(); 
}

void displayTempPress() {
  display.clearDisplay();
 
  // display temperature
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Temperature: ");
  display.setTextSize(2);
  display.setCursor(0,10);
  display.print(String(temperature, 3));
  display.print(" ");
  display.setTextSize(1);
  display.cp437(true);
  display.write(167);
  display.setTextSize(2);
  display.print("C");
  
  // display pressure
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("Pressure: ");
  display.setTextSize(2);
  display.setCursor(0, 45);
  display.print(String(pressure, 2));
  display.setTextSize(1);
  display.print("Pa"); 
  
  display.display();
}

void displayLastWateringTime() {
  String timeToDisplay = millisToTime(millis() - lastWateringTime);
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Last watered: ");
  display.setTextSize(2);
  display.setCursor(0,10);
  display.print(timeToDisplay);
  display.setTextSize(1);
  display.print(" ago");
  display.setCursor(0, 45);
  if (lastMoistureValue < aboutToWater && millis() - lastWateringTime >=  betweenWatering + 20000 && currentMode == AUTOMATIC) {
    display.println("ABOUT TO WATER");
  }
  display.display();
}

void displayMoistureAndLight() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Moisture: ");
  display.setTextSize(2);
  display.setCursor(0,10);
  display.print(lastMoistureValue);
  
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("Light: ");
  display.setTextSize(2);
  display.setCursor(0, 45);
  display.print(lastLightValue);
  
  display.display();
}

void displayLowWater() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,0);
  if (currentWaterLeft == 0) {
    display.print("EMPTY");
  } else {
    display.print("LOW WATER");
  }
  display.display();
}

// Function creates a carousel of the different screens with different sensor values and data
void screenSwapper() {
  displayScreen();
  if (millis() - lastScreenChangeTime >= screenInterval || wateringState) {
    if (screen == nrOfScreens) {
      screen = 1;
    } else {
      screen++;
    }
    lastScreenChangeTime = millis();
  }
}
