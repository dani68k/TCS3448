#pragma once

#include <stdint.h>

constexpr uint8_t TCS3448MaxChannels = 18;

// Stable application codes. Mapping to the vendor API belongs to the manager.
enum class TCS3448Gain : uint8_t {
  X0_5 = 0, X1, X2, X4, X8, X16, X32, X64, X128, X256, X512, X1024, X2048
};
enum class TCS3448SmuxMode : uint8_t { Channels6 = 6, Channels12 = 12, Channels18 = 18 };
enum class TCS3448AutoZeroMode : uint8_t { Disabled = 0, FirstMeasurement = 1, EveryMeasurement = 2 };

struct TCS3448Config {
  int sdaPin = 4;
  int sclPin = 5;
  uint32_t i2cFrequency = 400000;
  TCS3448Gain gain = TCS3448Gain::X64;
  double integrationMs = 50.04;
  uint16_t ledCurrent = 10;
  bool ledEnabled = false;
  bool ledAuto = false;
  TCS3448SmuxMode smux = TCS3448SmuxMode::Channels18;
  TCS3448AutoZeroMode autozero = TCS3448AutoZeroMode::FirstMeasurement;
  uint32_t sampleIntervalMs = 500;
};

using TCS3448Readings = uint16_t[TCS3448MaxChannels];
struct TCS3448Sample {
  uint32_t sequence = 0;
  uint32_t timestampMs = 0;
  TCS3448Readings channels = {};
  TCS3448Gain gain = TCS3448Gain::X64;
  bool saturated = false;
  uint8_t channelCount = TCS3448MaxChannels;
};

enum class TCS3448Error : uint8_t {
  None = 0, Initialization = 1, Configuration = 2, Led = 3,
  Communication = 4, Timeout = 5, ConfigApplyFailed = 6,
  InvalidGain = 16, InvalidIntegration = 17, InvalidLedCurrent = 18,
  InvalidSmuxMode = 19, InvalidAutozero = 20, InvalidSamplePeriod = 21,
  SamplePeriodTooShort = 22, InvalidPayload = 23,
  ConfigSaveFailed = 24,  // Settings applied, but not confirmed persistent in NVS.
};

struct TCS3448Status {
  bool ready = false;  // True only while the hardware configuration is known.
  bool acquiring = false;
  TCS3448Error error = TCS3448Error::None;
  uint32_t sampleCount = 0;
  uint16_t configRevision = 0;
};
