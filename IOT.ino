#include <SPI.h>
#include <Wire.h>
#include <Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP280.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
//-----------------------------------Network

const char* ssid = "GNXA986C1";
const char* password = "973HJY42M2VF";
const char* mqtt_server = "mqtt.uu.nl";
const char* mqtt_username = "student133";
const char* mqtt_password = "FDQrVK7m";

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;


Servo myservo;  // create servo object to control a servo

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

#define BMP_SCK  (13)
#define BMP_MISO (12)
#define BMP_MOSI (11)
#define BMP_CS   (10)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_BMP280 bmp; // I2C


const int initialPositionServo = 0;
const int wateringPosition = 90;

// PINS
const int selPin = D7;
const int analogPin = A0;
const int flashBtn = D3;

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
unsigned long lastWateringTime = 0;
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

#define AUTOMATIC "AUTOMATIC"
#define MANUAL "MANUAL"
String currentMode = AUTOMATIC;
String lastMode = AUTOMATIC;

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
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

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  String topicString = String(topic);
  
  if (topicString == "infob3it/133/water"){
    changeMode(MANUAL);
    StaticJsonDocument<256> doc;
    char value[32] = "";
    deserializeJson(doc, payload, length);
    strlcpy(value, doc["water"] | "default", sizeof(value));
    Serial.println(value);
    if (doc["water"] == "true") {
      water();
    } else if (doc["water"] == "false") {
      if (wateringState) {
        stopWatering();
      }
    }
    publishMode(MANUAL);
  }

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

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password, "infob3it/133/status", 0, true, "DISCONNECTED", true)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("infob3it/133/temperature", "reconnected");
      client.publish("infob3it/133/pressure", "reconnected");
      client.publish("infob3it/133/moisture", "reconnected");
      client.publish("infob3it/133/light", "reconnected");
      client.publish("infob3it/133/mode", "reconnected", false);
      client.publish("infob3it/133/water", "reconnected", false);
      client.publish("infob3it/133/refresh", "reconnected");
      // ... and resubscribe
      client.subscribe("infob3it/133/water");
      client.subscribe("infob3it/133/mode");
      client.subscribe("infob3it/133/refresh");
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
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
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
void setup() {
  Serial.begin(9600);
  Wire.begin(D2,D1);
  pinMode(analogPin, INPUT);
  pinMode(selPin, OUTPUT);
  pinMode(flashBtn, INPUT);
  pinMode(LED_BUILTIN, OUTPUT); 
  digitalWrite(LED_BUILTIN, HIGH);
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
  if (millis() - lastSoilMeasureTime >= soilInterval || lastSoilMeasureTime == 0 || refresh == true) {
    digitalWrite(selPin, HIGH);
    delay(100);
    soilMoisture = analogRead(analogPin);
    digitalWrite(analogPin, LOW);
    lastSoilMeasureTime = millis();
    lastMoistureValue = soilMoisture;
    checkPossibleWater(); //calling this function only when retrieving a new value. the function also checks if its time for watering
    return soilMoisture;
  }
  return -1;
}

//-------------------WATERING EVENTS
void water() {
  startedWatering = millis();
  wateringState = true;
  myservo.write(wateringPosition);
}

void stopWatering() {
  myservo.write(initialPositionServo);
  wateringState = false;
  lastWateringTime = millis();
}

//Function to stop watering, only in automatic mode
void stopWateringChecker() {
  if (millis() - startedWatering >= wateringInterval && wateringState && currentMode == AUTOMATIC) {
    stopWatering();
  }
}

//Function checks for the right time to water, only if in AUTOMATIC MODE
void checkPossibleWater(){
  if (currentMode == MANUAL) {
    return;
  }
  // check if moisture is low and time passed after last watering, for the sensor to get the data
  if (lastMoistureValue < 250 && lastMoistureValue != -1 && millis() - lastWateringTime >=  betweenWatering) { 
    if (!wateringState){
      water();
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
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Temperature: ");
  display.println(String(temperature , 3) + " C");
  display.println("Pressure");
  display.println(String(pressure , 3) + " Pa");
  display.display(); 
}

String milisToTime(unsigned long milliseconds) {
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
  String timeToDisplay = milisToTime(millis() - lastWateringTime);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Last watered: ");
  display.println(timeToDisplay + " ago");
  if (lastMoistureValue < aboutToWater && millis() - lastWateringTime >=  betweenWatering + 20000) {
    display.println("ABOUT TO WATER");
  }
  display.display();
}

void displayMoistureAndLight() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Moisture: ");
  display.println(lastMoistureValue);
  display.println("Light");
  display.println(lastLightValue);
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
void modeChecker(){
  if (currentMode == AUTOMATIC) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void changeMode(String modeValue) {
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
    if(client.publish("infob3it/133/moisture", buffer)) {
      Serial.println("Pubished #" + String(value));
    }
  }
}

//Function that confirms that the sensors have been updated
void publishHasRefreshed() {
  StaticJsonDocument<256> refresh;
  refresh["refresh"] = "false";
  char buffer[256];
  serializeJson(refresh, buffer);
  if(client.publish("infob3it/133/refresh", buffer)) {
    Serial.println("Confirmation sent!");
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

void loop() {
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
