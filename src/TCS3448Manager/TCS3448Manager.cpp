#include "TCS3448Manager.h"

#include <Arduino.h>
#include <Wire.h>

namespace {
Adafruit_TCS3448 sensor;
bool initialized = false;
uint8_t ledCurrent = 10;

const tcs3448_channel_t outputChannels[] = {
    TCS3448_CHANNEL_F1, TCS3448_CHANNEL_F2, TCS3448_CHANNEL_FZ,
    TCS3448_CHANNEL_F3, TCS3448_CHANNEL_F4, TCS3448_CHANNEL_F5,
    TCS3448_CHANNEL_FY, TCS3448_CHANNEL_FXL, TCS3448_CHANNEL_F6,
    TCS3448_CHANNEL_F7, TCS3448_CHANNEL_F8, TCS3448_CHANNEL_NIR,
    TCS3448_CHANNEL_VIS_TL_0};
}

namespace TCS3448Manager {
bool Init(const TCS3448Config& config) {
  initialized = false;
  Wire.setPins(config.sdaPin, config.sclPin);
  Wire.setClock(config.i2cFrequency);
  if (!sensor.begin()) {
    Serial.println(F("Could not find a TCS3448 sensor!"));
    return false;
  }

  // Integration time = (ATIME + 1) * (ASTEP + 1) * 2.78 us.
  if (!sensor.setGain(config.gain) || !sensor.setATIME(config.atime) ||
      !sensor.setASTEP(config.astep) ||
      !sensor.setSMUXMode(TCS3448_SMUX_18CH)) {
    Serial.println(F("Could not configure the sensor!"));
    return false;
  }
  ledCurrent = config.ledCurrent;
  initialized = true;
  return true;
}

bool LEDEnable() {
  if (!initialized) return false;
  if (!sensor.setLEDCurrent(ledCurrent)) {
    Serial.println(F("Could not set the LED current!"));
    return false;
  }
  if (!sensor.enableLED(true)) {
    Serial.println(F("Could not turn on the LED!"));
    return false;
  }
  return true;
}

float GetIntegrationTime() {
  return initialized ? sensor.getIntegrationTime() : 0.0f;
}

uint8_t GetGain() {
  return initialized ? static_cast<uint8_t>(sensor.getGain()) : 0xFF;
}

bool ReadAllChannels(TCS3448Readings& readings) {
  return initialized && sensor.readAllChannels(readings);
}

void PrintHeader() {
  Serial.print(F("Integration time: "));
  Serial.print(GetIntegrationTime());
  Serial.println(F(" ms"));
  Serial.println();
  Serial.println(F("F1\tF2\tFZ\tF3\tF4\tF5\tFY\tFXL\tF6\tF7\tF8\tNIR\tVIS"));
}

void PrintReadings() {
  TCS3448Readings readings;
  if (!ReadAllChannels(readings)) {
    Serial.println(F("Could not read sensor channels."));
    return;
  }
  bool first = true;
  for (const auto channel : outputChannels) {
    if (!first) Serial.print('\t');
    Serial.print(readings[channel]);
    first = false;
  }
  Serial.println();
}
}
