#include "globals.h"

void loop() {
  wm.process();
  ArduinoOTA.handle();
  checkers();
  screenSwapper();
  clientConnection();
  readAndSendData(false);
}
