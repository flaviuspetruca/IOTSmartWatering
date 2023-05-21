#include <Adafruit_BMP280.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

const char* mqtt_server = "public.mqtthq.com";
PubSubClient client(espClient);
char* topic = "proiect/smp/";
const int dataInterval = 100000;
int sentDataIndex = 0;

char* concat(char* str1, char* str2) {
  int len1 = strlen(str1);
  int len2 = strlen(str2);
  char* result = new char[len1 + len2 + 1];
  strcpy(result, str1);
  strcat(result, str2);
  return result;
}

// Incoming messages
void incomingWaterMessage(String topicString, byte* payload, unsigned int length) {
  if (topicString == String(topic) + "/water"){
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
  if (topicString == String(topic) + "/mode"){
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
  if (topicString == String(topic) + "/refresh"){
    StaticJsonDocument<256> doc;
    char value[32] = "";
    deserializeJson(doc, payload, length);
    strlcpy(value, doc["refresh"] | "default", sizeof(value));
    Serial.println(value);
    if (doc["refresh"] == "true") {
      readAndSendData(true);
    } 
  }
}

void incomingWaterLeftMessage(String topicString, byte* payload, unsigned int length) {
  if (topicString == String(topic) + "/waterLeft"){
    StaticJsonDocument<256> doc;
    char value[32] = "";
    deserializeJson(doc, payload, length);
    strlcpy(value, doc["water"] | "default", sizeof(value));
    currentWaterLeft = atoi(value);
    Serial.println(currentWaterLeft);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String topicString = String(topic);
  Serial.print("Message arrived [" + topicString + "]");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

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
    
    //connect to mqtt broker, last Will and Testament sent to status, QOS0, Retain message true, using clean sessions
    if (client.connect(clientId.c_str(), NULL, NULL, concat(topic, "/status"), 0, true, buffer, true)) {
      Serial.println("connected");
      reconnected["reconnected"] = "true";
      serializeJson(reconnected, buffer);
      client.publish(concat(topic, "/pressure"), buffer);
      client.publish(concat(topic, "/temperature"), buffer);
      client.publish(concat(topic, "/moisture"), buffer, true);
      client.publish(concat(topic, "/light"), buffer);
      client.publish(concat(topic, "/mode"), buffer, false);
      client.publish(concat(topic, "/water"), buffer, false);
      client.publish(concat(topic, "/refresh"), buffer);
      client.publish(concat(topic, "/status"), buffer, true);
   
      client.subscribe(concat(topic, "/water"));
      client.subscribe(concat(topic, "/mode"));
      client.subscribe(concat(topic, "/refresh"));
      client.subscribe(concat(topic, "/waterLeft"));
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


// Publishers
void publishTemperature() {
  StaticJsonDocument<256> temp;
  temp["temperature"] = String(temperature, 2);
  temp["id"] = sentDataIndex;
  char buffer[256];
  serializeJson(temp, buffer);
  if (client.publish(concat(topic, "/temperature"), buffer)) {
    Serial.println("Pubished #" + String(sentDataIndex));
  }
}

void publishPressure() {
  StaticJsonDocument<256> pres;
  pres["pressure"] = String(pressure,2);
  pres["id"] = sentDataIndex;
  char buffer[256];
  serializeJson(pres, buffer);
  if (client.publish(concat(topic, "/pressure"), buffer)) {
    Serial.println("Pubished #" + String(sentDataIndex));
  }
}

void publishLight() {
  const int currentLight = getLight();
  StaticJsonDocument<256> light;
  light["light"] = currentLight;
  light["id"] = sentDataIndex;
  char buffer[256];
  serializeJson(light, buffer);
  if (currentLight != -1) {
    if (client.publish(concat(topic, "/light"), buffer)) {
      Serial.println("Pubished #" + String(sentDataIndex));
    }
  }
}

void publishMoisture(int currentMoisture) {
  StaticJsonDocument<256> soil;
  soil["moisture"] = currentMoisture;
  soil["id"] = sentDataIndex;
  char buffer[256];
  serializeJson(soil, buffer);
  Serial.println("moisture" + String(currentMoisture));
  if (currentMoisture != -1) {
    if(client.publish(concat(topic, "/moisture"), buffer, true)) {
      Serial.println("Pubished #" + String(sentDataIndex));
    }
  }
}

//Function that confirms that the sensors have been updated by sending back a message
void publishHasRefreshed() {
  StaticJsonDocument<256> refresh;
  refresh["refresh"] = "false";
  char buffer[256];
  serializeJson(refresh, buffer);
  if(client.publish(concat(topic, "/refresh"), buffer)) {
    Serial.println("Confirmation sent!");
  }
}

void publishMode(String modeValue) {
  StaticJsonDocument<256> modeJSON;
  modeJSON["mode"] = modeValue;
 
  char buffer[256];
  serializeJson(modeJSON, buffer);
  if (client.publish(concat(topic, "/mode"), buffer)) {
    Serial.println("Pubished " + modeValue);
  }
}

void publishWaterLeft() {
  StaticJsonDocument<256> water;
  water["water"] = String(currentWaterLeft);
  char buffer[256];
  serializeJson(water, buffer);
  if(client.publish(concat(topic, "/waterLeft"), buffer, true)) {
    Serial.println("Water updated!");
  }
}

void clientConnection() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

//Function retrieves data every 2 seconds and sends it to the MQTT broker. Sending data with JSON.
void readAndSendData(bool refresh) {
  unsigned long now = millis();
  if (now - lastMsg > dataInterval || refresh) {
    const int currentMoisture = getSoilMoisture(refresh);
    temperature = getTemperature();
    pressure = getPressure();
    lastMsg = now;
    ++sentDataIndex;
    publishTemperature();
    publishPressure();
    publishLight();
    publishMoisture(currentMoisture);
    publishHasRefreshed();
  }
}
