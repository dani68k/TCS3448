#include <Arduino.h>

#include "SerialCommand/SerialCommand.h"
#include "TCS3448Manager/TCS3448Manager.h"
#include "ble/ble.h"

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println(F("ATLANTES / TCS3448 - BLE Web"));
  TCS3448Manager::Init();
  if (!AtlantesBLE::Init()) Serial.println(F("Could not initialize BLE."));
}

void loop() {
  AtlantesBLE::Update();
  processSerialCommands();
  delay(1);
}
