#include <Arduino.h>

#include "SerialCommand/SerialCommand.h"
#include "TCS3448Manager/TCS3448Manager.h"

namespace {
bool sensorReady = false;
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println(F("Adafruit TCS3448 basic readings"));

  sensorReady = TCS3448Manager::Init();
  if (sensorReady) {
    TCS3448Manager::LEDEnable();
    TCS3448Manager::PrintHeader();
  }
}

void loop() {
  if (sensorReady) TCS3448Manager::PrintReadings();
  processSerialCommands();
  delay(500);
}
