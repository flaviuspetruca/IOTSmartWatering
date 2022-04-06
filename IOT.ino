#include <Wire.h>
#include <Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP280.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <WiFiManager.h>  

//-----------------------------------Network

const char* mqtt_server = "mqtt.uu.nl";
WiFiClient espClient;
WiFiManager wm;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

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

#define EEPROM_SIZE 512

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_BMP280 bmp; // I2C

//Servo
Servo myservo;  // create servo object to control a servo
const int initialPositionServo = 90;
const int wateringPosition = 0;

// PINS
const int selPin = D7;
const int analogPin = A0;
const int flashBtn = D3;
const int waterContainerLed = D8;

// STATES
int lastButtonState = LOW;
bool wateringState = false;

// Times
unsigned long lastFlashBtnPress = 0;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
unsigned long lastSoilMeasureTime = 0;
unsigned long lastScreenChangeTime = 0;
unsigned long betweenWatering = 60000;
unsigned long soilInterval = 60000;
unsigned long startedWatering = 0;
unsigned long startedWateringUpdate = 0;
unsigned long lastWateringTime = 0;
unsigned long lastWaterCheck = 0;
const int waterCheckInterval = 1000;
const int dataInterval = 2000;
const int btnInterval = 500;
const int wateringInterval = 5000;
const int screenInterval = 4000;
const int aboutToWater = 350;
// Sensor values
int lastMoistureValue = -1;
int lastLightValue = 0;
float temperature = 0;
float pressure = 0;

// Screen Variables
int screen = 1;
int nrOfScreens = 3;


// Container and water left
int currentWaterLeft;
const int containerSize = 1500;
const int mlPerSec = 30;
bool waterLedState = false;
int waterLedInterval = 1500;
unsigned long lastHighLed = 0;

// MODES
#define AUTOMATIC "AUTOMATIC"
#define MANUAL "MANUAL"
String currentMode = AUTOMATIC;
String lastMode = AUTOMATIC;

void setupOTA() {
  ArduinoOTA.setPassword("strong_password");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  randomSeed(micros());
}

void setup_wifi() {
  //eeprom auth in case WIFIManger does not work
  /*EEPROM.begin(EEPROM_SIZE);
  String eeprom_ssid;
  String eeprom_password;
  for (int i = 0 ; i < 32 ; i++) {
    eeprom_ssid += char(EEPROM.read(i));
  }
  for (int i = 32 ; i < 96 ; i++) {
    eeprom_password += char(EEPROM.read(i));
  }
  EEPROM.end();
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(eeprom_ssid);*/
  if(wm.autoConnect("AutoConnectAP", "strong_password")) {
    Serial.println("connected");
  }
  //WiFi.begin(eeprom_ssid.c_str(), eeprom_password.c_str());

  setupOTA();
  
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(5000);
    Serial.print(".");
    ESP.restart();
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


// Incoming messages
void incomingWaterMessage(String topicString, byte* payload, unsigned int length) {
  if (topicString == "infob3it/133/water"){
    StaticJsonDocument<256> doc;
    char value[32] = "";
    deserializeJson(doc, payload, length);
    strlcpy(value, doc["water"] | "default", sizeof(value));
    Serial.println(value);
    if (doc["water"] == "true") {
      changeMode(MANUAL);
      water();
    } else if (doc["water"] == "false") {
      if (wateringState) {
        stopWatering();
      }
    }
    publishMode(MANUAL);
  }
}

void incomingModeMessage(String topicString, byte* payload, unsigned int length) {
  if (topicString == "infob3it/133/mode"){
    StaticJsonDocument<256> doc;
    char value[32] = "";
    deserializeJson(doc, payload, length);
    strlcpy(value, doc["mode"] | "default", sizeof(value));
    Serial.println(value);
    if (doc["mode"] == "AUTOMATIC") {
      changeMode(AUTOMATIC);
    } else if (doc["mode"] == "MANUAL") {
      changeMode(MANUAL);
    }
  }
}

void incomingRefreshMessage(String topicString, byte* payload, unsigned int length) {
  if (topicString == "infob3it/133/refresh"){
    StaticJsonDocument<256> doc;
    char value[32] = "";
    deserializeJson(doc, payload, length);
    strlcpy(value, doc["refresh"] | "default", sizeof(value));
    Serial.println(value);
    if (doc["refresh"] == "true") {
      refreshData();
    } 
  }
}

void incomingWaterLeftMessage(String topicString, byte* payload, unsigned int length) {
  if (topicString == "infob3it/133/waterLeft"){
    StaticJsonDocument<256> doc;
    char value[32] = "";
    deserializeJson(doc, payload, length);
    strlcpy(value, doc["water"] | "default", sizeof(value));
    currentWaterLeft = atoi(value);
    Serial.println(currentWaterLeft);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  String topicString = String(topic);
  
  incomingWaterMessage(topicString, payload, length);
  incomingModeMessage(topicString, payload, length);
  incomingRefreshMessage(topicString, payload, length);
  incomingWaterLeftMessage(topicString, payload, length);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    StaticJsonDocument<256> reconnected;
    reconnected["reconnected"] = "false";
    char buffer[256];
    serializeJson(reconnected, buffer);

    //get mqtt credentials
    EEPROM.begin(EEPROM_SIZE);
    String mqtt_username;
    String mqtt_password;
    for (int i = 96 ; i < 112 ; i++) {
      mqtt_username += char(EEPROM.read(i));
    }
    for (int i = 112 ; i < 136 ; i++) {
      mqtt_password += char(EEPROM.read(i));
    }
    EEPROM.end();

    //connect to mqtt broker, last Will and Testament to status, QOS0, Retain message true, using clean sessions
    if (client.connect(clientId.c_str(), mqtt_username.c_str(), mqtt_password.c_str(), "infob3it/133/status", 0, true, buffer, true)) {
      Serial.println("connected");
      reconnected["reconnected"] = "true";
      serializeJson(reconnected, buffer);
      client.publish("infob3it/133/pressure", buffer);
      client.publish("infob3it/133/temperature", buffer);
      client.publish("infob3it/133/moisture", buffer, true);
      client.publish("infob3it/133/light", buffer);
      client.publish("infob3it/133/mode", buffer, false);
      client.publish("infob3it/133/water", buffer, false);
      client.publish("infob3it/133/refresh", buffer);
      client.publish("infob3it/133/status", buffer, true);
   
      client.subscribe("infob3it/133/water");
      client.subscribe("infob3it/133/mode");
      client.subscribe("infob3it/133/refresh");
      client.subscribe("infob3it/133/waterLeft");
      publishMode(currentMode);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


//----------------SETUP

void setupBmp() {
  unsigned status;
  status = bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID);
  if (!status) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
    Serial.print("SensorID was: 0x"); Serial.println(bmp.sensorID(),16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
    
  }
}

void setupScreen() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  displayScreen();
}

void setupServo() {
  myservo.attach(D6);
  myservo.write(initialPositionServo); // reset servo to the original position
}

void setupEEPROM() {
  String ssid = "GNXA986C1";
  String password = "973HJY42M2VF";
  String mqtt_username = "student133";
  String mqtt_password = "FDQrVK7m";

  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 96; i++) {
    EEPROM.put(i, 0);
  }
  for (int i = 0 ; i < 32 ; i++) {
    EEPROM.put(i, ssid[i]);
  }
  for (int i = 32 ; i < 96 ; i++) {
    EEPROM.put(i, password[i - 32]);
  }
  
  for (int i = 96 ; i < 112 ; i++) {
    EEPROM.put(i, mqtt_username[i - 96]);
  }

  for (int i = 112 ; i < 136 ; i++) {
    EEPROM.put(i, mqtt_password[i - 112]);
  }
  EEPROM.commit();
  EEPROM.end();
}

void setup() {
  Serial.begin(9600);
  Wire.begin(D2,D1);
  pinMode(analogPin, INPUT);
  pinMode(selPin, OUTPUT);
  pinMode(flashBtn, INPUT);
  pinMode(LED_BUILTIN, OUTPUT); 
  pinMode(D8, OUTPUT); 
  digitalWrite(LED_BUILTIN, HIGH);
  //setupEEPROM();
  setupServo();
  setupBmp();
  
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  while ( !Serial ) delay(100);   // wait for native usb
  setupScreen();
}

//-----------------------SENSORS
int getLight() {
  if (millis() - lastSoilMeasureTime < soilInterval) {
    digitalWrite(selPin, LOW);
    int light = analogRead(analogPin);
    lastLightValue = light;
    return light;
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
    checkPossibleWater(); //calling this function only when retrieving a new value. the function also checks if its time for watering
    return avg;
  }
  return -1;
}

//-------------------WATERING EVENTS
void water() {
  if (currentWaterLeft == 0) {
    startedWatering = millis();
    stopWatering();
  } else {
      startedWatering = startedWateringUpdate = millis();
      wateringState = true;
      myservo.write(wateringPosition);
  }
}

void stopWatering() {
  myservo.write(initialPositionServo);
  wateringState = false;
  lastWateringTime = millis();
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

//Function checks for the right time to water, only if in AUTOMATIC MODE
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


//-------------------DISPLAY
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

//Function creates a carousel of the different screens with different sensor values and data
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

//------------LOOP EVENTS
//Function to check mode in order to change built in led state
void modeChecker() {
  if (currentMode == AUTOMATIC) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
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

//Function checks for flash btn press in order to change use mode.
void btnPressChecker() {
  int flashBtnState = digitalRead(flashBtn);
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


// --------Publish Data
void publishTemperature() {
  StaticJsonDocument<256> temp;
  temp["temperature"] = String(temperature, 2);
  temp["id"] = value;
  char buffer[256];
  serializeJson(temp, buffer);
  if (client.publish("infob3it/133/temperature", buffer)) {
    Serial.println("Pubished #" + String(value));
  }
}

void publishPressure() {
  StaticJsonDocument<256> pres;
  pres["pressure"] = String(pressure,2);
  pres["id"] = value;
  char buffer[256];
  serializeJson(pres, buffer);
  if (client.publish("infob3it/133/pressure", buffer)) {
    Serial.println("Pubished #" + String(value));
  }
}

void publishLight(int currentLight) {
  StaticJsonDocument<256> light;
  light["light"] = currentLight;
  light["id"] = value;
  char buffer[256];
  serializeJson(light, buffer);
  if (currentLight != -1) {
    if (client.publish("infob3it/133/light", buffer)) {
      Serial.println("Pubished #" + String(value));
    }
  }
}

void publishMoisture(int currentMoisture) {
  StaticJsonDocument<256> soil;
  soil["moisture"] = currentMoisture;
  soil["id"] = value;
  char buffer[256];
  serializeJson(soil, buffer);
  if (currentMoisture != -1) {
    if(client.publish("infob3it/133/moisture", buffer, true)) {
      Serial.println("Pubished #" + String(value));
    }
  }
}

//Function that confirms that the sensors have been updated by sending back a message
void publishHasRefreshed() {
  StaticJsonDocument<256> refresh;
  refresh["refresh"] = "false";
  char buffer[256];
  serializeJson(refresh, buffer);
  if(client.publish("infob3it/133/refresh", buffer)) {
    Serial.println("Confirmation sent!");
  }
}

void publishMode(String modeValue) {
  StaticJsonDocument<256> modeJSON;
  modeJSON["mode"] = modeValue;
 
  char buffer[256];
  serializeJson(modeJSON, buffer);
  if (client.publish("infob3it/133/mode", buffer)) {
    Serial.println("Pubished " + modeValue);
  }
}

void publishWaterLeft() {
  StaticJsonDocument<256> water;
  water["water"] = String(currentWaterLeft);
  char buffer[256];
  serializeJson(water, buffer);
  if(client.publish("infob3it/133/waterLeft", buffer, true)) {
    Serial.println("Water updated!");
  }
}

//Function that refreshed data bases on incoming message from topic refresh
void refreshData() {
    unsigned long now = millis();
    const int currentLight = getLight();
    const int currentMoisture = getSoilMoisture(true);
    temperature = bmp.readTemperature();
    pressure = bmp.readPressure();
    lastMsg = now;
    ++value;
    publishTemperature();
    publishPressure();
    publishLight(currentLight);
    publishMoisture(currentMoisture);
    publishHasRefreshed();
}

//Function retrieves data every 2 seconds and sends it to the MQTT broker. Sending data with JSON.
void readAndSendData() {
  unsigned long now = millis();
  if (now - lastMsg > dataInterval) {
    const int currentLight = getLight();
    const int currentMoisture = getSoilMoisture(false);
    temperature = bmp.readTemperature();
    pressure = bmp.readPressure();
    lastMsg = now;
    ++value;
    publishTemperature();
    publishPressure();
    publishLight(currentLight);
    publishMoisture(currentMoisture);
  }
}

// Water container low on water make led blink
void blinkWaterLed() {
  if (waterLedState && millis() - lastHighLed >= waterLedInterval) {
    digitalWrite(D8, HIGH);
    lastHighLed = millis();
  } else {
    digitalWrite(D8, LOW);
  }
}

void loop() {
  wm.process();
  ArduinoOTA.handle();
  checkDecreasingWaterLevel();
  checkLowWater();
  blinkWaterLed();
  modeChecker();
  stopWateringChecker();
  btnPressChecker();
  screenSwapper();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  readAndSendData();
}
