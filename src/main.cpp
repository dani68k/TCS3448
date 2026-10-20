#include <Arduino.h>

#include "SerialCommand/SerialCommand.h"
#include "TCS3448Manager/TCS3448Manager.h"
#include "ble/ble.h"
#include "nvs_manager/nvs_manager.h"

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println(F("TCS3448 - BLE Web"));
  TCS3448Config config;
  const auto restored = NVSManager::Load(config);
  Serial.printf("[NVS] setup restore: %s%s\n", NVSManager::ResultName(restored),
      restored == NVSManager::Result::Ok ? "" : " (using defaults)");
  TCS3448Manager::Init(config);
  if (!TCS3448BLE::Init()) Serial.println(F("Could not initialize BLE."));
}

void loop() {
  TCS3448BLE::Update();
  processSerialCommands();
  delay(1);
}
