#pragma once

#include <Adafruit_TCS3448.h>

struct TCS3448Config {
  int sdaPin = 4;
  int sclPin = 5;
  uint32_t i2cFrequency = 400000;
  tcs3448_gain_t gain = TCS3448_GAIN_64X;
  uint8_t atime = 29;
  uint16_t astep = 599;
  uint8_t ledCurrent = 10;
  uint16_t sampleIntervalMs = 500;
};

using TCS3448Readings = uint16_t[TCS3448_CHANNEL_COUNT];

// Raw ADC values in the driver's 18-channel SMUX order (not calibrated units).
struct TCS3448Sample {
  uint32_t sequence = 0;
  uint32_t timestampMs = 0;
  TCS3448Readings channels = {};
  tcs3448_gain_t gain = TCS3448_GAIN_64X;
  bool saturated = false;
};

enum class TCS3448Error : uint8_t {
  None = 0,
  Initialization = 1,
  Configuration = 2,
  Led = 3,
  Communication = 4,
  Timeout = 5,
};

struct TCS3448Status {
  bool ready = false;
  bool acquiring = false;
  TCS3448Error error = TCS3448Error::None;
  uint32_t sampleCount = 0;
};
