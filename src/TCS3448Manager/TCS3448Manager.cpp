#include "TCS3448Manager.h"
#include "TCS3448ConfigMath.h"

#include <Adafruit_TCS3448.h>
#include <Arduino.h>
#include <Wire.h>
#include <cmath>

namespace {
Adafruit_TCS3448 sensor;
bool initialized = false;
TCS3448Config currentConfig;
TCS3448Timing activeTiming;
TCS3448Status status;
bool measurementPending = false;
uint32_t measurementStartedMs = 0;
uint32_t lastStartedMs = 0;

uint8_t smuxCode(TCS3448SmuxMode mode) {
  return mode == TCS3448SmuxMode::Channels6 ? 0 : mode == TCS3448SmuxMode::Channels12 ? 2 : 3;
}
uint8_t zeroCode(TCS3448AutoZeroMode mode) {
  return mode == TCS3448AutoZeroMode::Disabled ? 0 : mode == TCS3448AutoZeroMode::FirstMeasurement ? 255 : 1;
}
tcs3448_settings_t settingsFor(const TCS3448Config& config, const TCS3448Timing& timing, bool acquiring = status.acquiring) {
  return {static_cast<uint8_t>(config.gain), timing.atime, timing.astep,
          smuxCode(config.smux), zeroCode(config.autozero), config.ledCurrent, config.ledAuto ? acquiring : config.ledEnabled};
}
bool sameSettings(const tcs3448_settings_t& a, const tcs3448_settings_t& b) {
  return a.gain == b.gain && a.atime == b.atime && a.astep == b.astep &&
      a.smux == b.smux && a.autozero == b.autozero && a.ledCurrent == b.ledCurrent && a.ledEnabled == b.ledEnabled;
}
bool verify(const tcs3448_settings_t& expected) {
  tcs3448_settings_t actual;
  return sensor.readConfiguration(&actual) && sameSettings(expected, actual);
}
void logApplied() {
  Serial.printf("[TCS3448] applied: gain_code=%u integration=%.5fms LED=%umA/%s SMUX=%u AZ=%u period=%lums ADC=%u\n",
      static_cast<unsigned>(currentConfig.gain), currentConfig.integrationMs,
      currentConfig.ledCurrent, currentConfig.ledAuto ? "AUTO" : currentConfig.ledEnabled ? "ON" : "OFF",
      static_cast<unsigned>(currentConfig.smux), static_cast<unsigned>(currentConfig.autozero),
      static_cast<unsigned long>(currentConfig.sampleIntervalMs), activeTiming.adcFullScale);
}
void fail(TCS3448Error error) {
  TCS3448Manager::StopAcquisition(true);
  status.error = error;
  Serial.printf("[TCS3448] acquisition stopped: %s\n", TCS3448Manager::ErrorName(error));
}
}

namespace TCS3448Manager {
const char* ErrorName(TCS3448Error error) {
  switch (error) {
    case TCS3448Error::None: return "OK";
    case TCS3448Error::Initialization: return "INITIALIZATION_FAILED";
    case TCS3448Error::Configuration: return "CONFIGURATION_INVALID";
    case TCS3448Error::Led: return "LED_FAILED";
    case TCS3448Error::Communication: return "I2C_COMMUNICATION_FAILED";
    case TCS3448Error::Timeout: return "ACQUISITION_TIMEOUT";
    case TCS3448Error::InvalidGain: return "INVALID_GAIN";
    case TCS3448Error::InvalidIntegration: return "INVALID_INTEGRATION";
    case TCS3448Error::InvalidLedCurrent: return "INVALID_LED_CURRENT";
    case TCS3448Error::InvalidSmuxMode: return "INVALID_SMUX_MODE";
    case TCS3448Error::InvalidAutozero: return "INVALID_AUTOZERO";
    case TCS3448Error::InvalidSamplePeriod: return "INVALID_SAMPLE_PERIOD";
    case TCS3448Error::SamplePeriodTooShort: return "SAMPLE_PERIOD_TOO_SHORT";
    case TCS3448Error::InvalidPayload: return "INVALID_CONFIG_PAYLOAD";
    default: return "CONFIG_APPLY_FAILED";
  }
}

bool Init(const TCS3448Config& config) {
  initialized = false;
  status = TCS3448Status();
  currentConfig = config;
  measurementPending = false;
  const auto error = TCS3448ConfigMath::Validate(config, activeTiming);
  if (error != TCS3448Error::None) {
    status.error = error;
    Serial.printf("[TCS3448] init: %s\n", ErrorName(error));
    return false;
  }
  Wire.setPins(config.sdaPin, config.sclPin);
  Wire.setClock(config.i2cFrequency);
  if (!sensor.begin()) {
    status.error = TCS3448Error::Initialization;
    Serial.println("[TCS3448] sensor initialization failed");
    return false;
  }
  initialized = true;
  const auto expected = settingsFor(config, activeTiming);
  if (!sensor.stopMeasurement() || !sensor.writeConfiguration(expected) || !verify(expected)) {
    sensor.enableLED(false);
    status.error = TCS3448Error::ConfigApplyFailed;
    return false;
  }
  currentConfig.integrationMs = activeTiming.integrationMs;
  status.ready = true;
  status.configRevision = 1;
  logApplied();
  return true;
}

const TCS3448Config& GetConfig() { return currentConfig; }
const TCS3448Status& GetStatus() { return status; }
uint16_t GetADCFullScale() { return status.ready ? activeTiming.adcFullScale : 0; }
uint32_t GetMinimumPeriodMs() { return activeTiming.minimumPeriodMs; }

bool RefreshConfig() {
  if (!initialized || !status.ready) return false;
  // A checked read of every setting confirms the cached high-level configuration.
  if (verify(settingsFor(currentConfig, activeTiming))) return true;
  fail(TCS3448Error::Communication);
  status.ready = false;
  return false;
}

TCS3448Error ApplyConfig(const TCS3448Config& requested) {
  Serial.println("[TCS3448] validating configuration");
  TCS3448Timing proposedTiming;
  const auto error = TCS3448ConfigMath::Validate(requested, proposedTiming);
  if (error != TCS3448Error::None) {
    Serial.printf("[TCS3448] rejected: %s (LED=%u mA, period=%lu ms, min=%lu ms)\n",
        ErrorName(error), requested.ledCurrent, static_cast<unsigned long>(requested.sampleIntervalMs),
        static_cast<unsigned long>(proposedTiming.minimumPeriodMs));
    return error;  // Absolutely no I2C writes or state changes before validation.
  }
  // Pins and bus speed are boot-only settings; not part of the remote setup.
  if (requested.sdaPin != currentConfig.sdaPin || requested.sclPin != currentConfig.sclPin ||
      requested.i2cFrequency != currentConfig.i2cFrequency) return TCS3448Error::InvalidPayload;
  if (!status.ready || !RefreshConfig()) return TCS3448Error::ConfigApplyFailed;
  const auto previous = settingsFor(currentConfig, activeTiming, false);
  const auto proposed = settingsFor(requested, proposedTiming);
  const bool wasRunning = status.acquiring;
  status.acquiring = false;
  measurementPending = false;
  if (!sensor.stopMeasurement() || !sensor.writeConfiguration(proposed) || !verify(proposed)) {
    // Restore all old registers and verify them, never publish a partial candidate.
    const bool stopped = sensor.stopMeasurement();
    const bool restored = stopped && sensor.writeConfiguration(previous) && verify(previous);
    status.ready = restored;
    status.error = TCS3448Error::ConfigApplyFailed;
    if (!restored) sensor.enableLED(false);
    Serial.printf("[TCS3448] CONFIG_APPLY_FAILED; rollback=%s; acquisition stopped\n", restored ? "verified" : "unconfirmed");
    return TCS3448Error::ConfigApplyFailed;
  }
  currentConfig = requested;
  currentConfig.integrationMs = proposedTiming.integrationMs;
  activeTiming = proposedTiming;
  ++status.configRevision;
  status.error = TCS3448Error::None;
  status.acquiring = wasRunning;
  lastStartedMs = millis() - currentConfig.sampleIntervalMs;
  logApplied();
  return TCS3448Error::None;
}

bool StartAcquisition() {
  if (!status.ready) return false;
  if (status.acquiring) return true;
  if (!RefreshConfig() || !sensor.stopMeasurement()) {
    fail(TCS3448Error::Communication);
    return false;
  }
  tcs3448_data_t discarded;
  if (!sensor.readData(&discarded, static_cast<uint8_t>(currentConfig.smux))) {
    fail(TCS3448Error::Communication);
    return false;
  }
  if (currentConfig.ledAuto && (!sensor.enableLED(true) || !verify(settingsFor(currentConfig, activeTiming, true)))) {
    fail(TCS3448Error::Led);
    return false;
  }
  status.error = TCS3448Error::None;
  status.acquiring = true;
  measurementPending = false;
  lastStartedMs = millis() - currentConfig.sampleIntervalMs;
  return true;
}

bool StopAcquisition(bool turnOffLED) {
  status.acquiring = false;
  measurementPending = false;
  if (!initialized) return false;
  const bool stopped = sensor.stopMeasurement();
  bool ledOff = true;
  if (turnOffLED || currentConfig.ledAuto) {
    ledOff = sensor.enableLED(false);
    auto expected = settingsFor(currentConfig, activeTiming);
    expected.ledEnabled = false;
    ledOff = ledOff && verify(expected);
    if (ledOff && currentConfig.ledEnabled && !currentConfig.ledAuto) {
      currentConfig.ledEnabled = false;
      ++status.configRevision;
    }
  }
  if (!stopped || !ledOff) {
    status.ready = false;
    status.error = TCS3448Error::Communication;
    return false;
  }
  return true;
}

bool Update(TCS3448Sample& sample) {
  if (!status.acquiring || !status.ready) return false;
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
    if (now - measurementStartedMs > activeTiming.minimumPeriodMs + 1000) fail(TCS3448Error::Timeout);
    return false;
  }
  tcs3448_data_t data;
  const uint8_t count = static_cast<uint8_t>(currentConfig.smux);
  const bool read = sensor.readData(&data, count);
  const bool stopped = sensor.stopMeasurement();
  measurementPending = false;
  if (!read || !stopped) {
    fail(TCS3448Error::Communication);
    return false;
  }
  sample.sequence = ++status.sampleCount;
  sample.timestampMs = now;
  sample.gain = static_cast<TCS3448Gain>(data.gain);
  sample.saturated = data.saturated;
  sample.channelCount = count;
  for (uint8_t i = 0; i < TCS3448MaxChannels; ++i) sample.channels[i] = i < count ? data.channels[i] : 0;
  return true;
}

bool LEDEnable() {
  auto config = currentConfig;
  config.ledEnabled = true;
  config.ledAuto = false;
  return ApplyConfig(config) == TCS3448Error::None;
}
float GetIntegrationTime() { return currentConfig.integrationMs; }
uint8_t GetGain() { return static_cast<uint8_t>(currentConfig.gain); }
bool ReadAllChannels(TCS3448Readings& readings) {
  if (!status.ready || status.acquiring) return false;
  for (auto& channel : readings) channel = 0;
  return sensor.readAllChannels(readings);
}
void PrintHeader() { Serial.println("Raw channels in automatic SMUX order"); }
void PrintReadings() {
  TCS3448Readings readings;
  if (!ReadAllChannels(readings)) return;
  for (uint8_t i = 0; i < static_cast<uint8_t>(currentConfig.smux); ++i) {
    if (i) Serial.print('\t');
    Serial.print(readings[i]);
  }
  Serial.println();
}
}
