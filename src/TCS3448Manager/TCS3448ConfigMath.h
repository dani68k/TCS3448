#pragma once

#include "TCS3448Types.h"

// Hardware timing stays private to the sensor layer, never on the BLE wire.
struct TCS3448Timing {
  uint8_t atime = 0;
  uint16_t astep = 0;
  double integrationMs = 0;
  uint16_t adcFullScale = 0;
  uint32_t minimumPeriodMs = 0;
};

namespace TCS3448ConfigMath {
bool ResolveIntegration(double milliseconds, TCS3448Timing& timing);
uint32_t MinimumPeriod(double integrationMs, TCS3448SmuxMode smux, TCS3448AutoZeroMode az);
TCS3448Error Validate(const TCS3448Config& config, TCS3448Timing& timing);
}
