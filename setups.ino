#include <Wire.h>

const char* password = "strong_password";

// Over the air updates
void setupOTA() {
  ArduinoOTA.setPassword(password);
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
  if(wm.autoConnect("AutoConnectAP", password)) {
    Serial.println("connected");
  }
  
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

void setupBmp() {
  unsigned status;
  Serial.println(BMP280_ADDRESS_ALT, HEX);
  Serial.println("ChipId" + String(BMP280_CHIPID));
  status = bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID);
  //status = bmp.begin();
  if (!status) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
    Serial.print("SensorID was: 0x"); Serial.println(bmp.sensorID(),16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
    while (1) delay(10);
  }

  /* Default settings from datasheet. */
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
}

void setupScreen() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  Serial.println(F("displaying"));
  displayScreen();
}

void setup() {
  Serial.begin(9600);
  Wire.begin(D5,D6);
  pinMode(analogPin, INPUT);
  pinMode(selPin, OUTPUT);
  pinMode(flashBtn, INPUT);
  pinMode(modeBtn, INPUT);
  pinMode(modeLED, OUTPUT); 
  pinMode(waterContainerLed, OUTPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);
   
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(modeLED, HIGH);
  setupBmp();
  setup_wifi();
  setupOTA();
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  while ( !Serial ) delay(100);   // wait for native usb
  setupScreen();
}
