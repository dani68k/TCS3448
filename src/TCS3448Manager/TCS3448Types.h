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
};

using TCS3448Readings = uint16_t[TCS3448_CHANNEL_COUNT];
