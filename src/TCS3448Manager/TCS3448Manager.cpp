#include "TCS3448Manager.h"

#include <Arduino.h>
#include <Wire.h>

namespace {
Adafruit_TCS3448 sensor;
bool initialized = false;
TCS3448Config currentConfig;
TCS3448Status status;
bool measurementPending = false;
uint32_t measurementStartedMs = 0;
uint32_t lastStartedMs = 0;

void fail(TCS3448Error error) {
  // All sensor access, including failure cleanup, runs on the Arduino loop task.
  sensor.stopMeasurement();
  sensor.enableLED(false);
  measurementPending = false;
  status.acquiring = false;
  status.error = error;
}

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
  status = TCS3448Status();
  measurementPending = false;
  currentConfig = config;
  if (config.sampleIntervalMs == 0) {
    status.error = TCS3448Error::Configuration;
    return false;
  }
  Wire.setPins(config.sdaPin, config.sclPin);
  Wire.setClock(config.i2cFrequency);
  if (!sensor.begin()) {
    status.error = TCS3448Error::Initialization;
    Serial.println(F("Could not find a TCS3448 sensor!"));
    return false;
  }

  // Integration time = (ATIME + 1) * (ASTEP + 1) * 2.78 us.
  if (!sensor.setGain(config.gain) || !sensor.setATIME(config.atime) ||
      !sensor.setASTEP(config.astep) ||
      !sensor.setSMUXMode(TCS3448_SMUX_18CH) ||
      !sensor.setLEDCurrent(config.ledCurrent)) {
    status.error = TCS3448Error::Configuration;
    Serial.println(F("Could not configure the sensor!"));
    return false;
  }
  initialized = true;
  if (!sensor.stopMeasurement() || !sensor.enableLED(false)) {
    status.error = TCS3448Error::Communication;
    initialized = false;
    return false;
  }
  status.ready = true;
  return true;
}

bool LEDEnable() {
  if (!initialized) return false;
  if (!sensor.setLEDCurrent(currentConfig.ledCurrent)) {
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

const TCS3448Config& GetConfig() { return currentConfig; }
const TCS3448Status& GetStatus() { return status; }

bool StartAcquisition() {
  if (!status.ready) return false;
  if (status.acquiring) return true;
  if (!sensor.stopMeasurement()) {
    fail(TCS3448Error::Communication);
    return false;
  }
  // Latch/discard the previous result before starting a new acquisition.
  tcs3448_data_t discarded;
  if (!sensor.readData(&discarded)) {
    fail(TCS3448Error::Communication);
    return false;
  }
  if (!LEDEnable()) {
    fail(TCS3448Error::Led);
    return false;
  }
  status.error = TCS3448Error::None;
  status.acquiring = true;
  measurementPending = false;
  lastStartedMs = millis() - currentConfig.sampleIntervalMs;
  return true;
}

bool StopAcquisition() {
  status.acquiring = false;
  measurementPending = false;
  if (!initialized) return false;
  const bool stopped = sensor.stopMeasurement();
  const bool ledOff = sensor.enableLED(false);
  if (!stopped || !ledOff) {
    status.error = TCS3448Error::Communication;
    return false;
  }
  return true;
}

bool Update(TCS3448Sample& sample) {
  if (!status.acquiring) return false;
  const uint32_t now = millis();
  if (!measurementPending) {
    if (now - lastStartedMs < currentConfig.sampleIntervalMs) return false;
    if (!sensor.startMeasurement()) {
      fail(TCS3448Error::Communication);
      return false;
    }
    measurementPending = true;
    measurementStartedMs = lastStartedMs = now;
    return false;
  }

  bool ready = false;
  if (!sensor.getDataReady(&ready)) {
    fail(TCS3448Error::Communication);
    return false;
  }
  if (!ready) {
    // Allow all three automatic SMUX cycles plus margin, including long exposures.
    const uint32_t timeoutMs = static_cast<uint32_t>(
        (currentConfig.atime + 1.0f) * (currentConfig.astep + 1.0f) * 0.00278f * 3) + 1000;
    if (now - measurementStartedMs > timeoutMs) fail(TCS3448Error::Timeout);
    return false;
  }

  tcs3448_data_t data;
  const bool read = sensor.readData(&data);
  const bool stopped = sensor.stopMeasurement();
  measurementPending = false;
  if (!read || !stopped) {
    fail(TCS3448Error::Communication);
    return false;
  }
  sample.sequence = ++status.sampleCount;
  sample.timestampMs = now;
  sample.gain = data.gain;
  sample.saturated = data.saturated;
  for (uint8_t i = 0; i < TCS3448_CHANNEL_COUNT; ++i) sample.channels[i] = data.channels[i];
  return true;
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
