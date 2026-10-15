#include "TCS3448ConfigMath.h"

#include <cmath>
#include <limits>

namespace TCS3448ConfigMath {
bool ResolveIntegration(double milliseconds, TCS3448Timing& timing) {
  constexpr double stepMs = 0.00278;
  // ATIME=ASTEP=0 is forbidden; ASTEP=65535 is reserved.
  if (!std::isfinite(milliseconds) || milliseconds < 2 * stepMs ||
      milliseconds > 256.0 * 65535 * stepMs + 0.0005) return false;
  const double desired = milliseconds / stepMs;
  double bestError = std::numeric_limits<double>::max();
  uint32_t bestTicks = 0;
  for (uint16_t multiplier = 1; multiplier <= 256; ++multiplier) {
    long steps = std::lround(desired / multiplier);
    if (steps < 1) steps = 1;
    if (steps > 65535) steps = 65535;
    const uint32_t ticks = multiplier * static_cast<uint32_t>(steps);
    if (ticks < 2) continue;
    const double error = std::fabs(ticks - desired);
    if (error < bestError) {
      bestError = error;
      bestTicks = ticks;
      timing.atime = multiplier - 1;
      timing.astep = steps - 1;
    }
  }
  if (!bestTicks) return false;
  timing.integrationMs = bestTicks * stepMs;
  timing.adcFullScale = bestTicks > 65535 ? 65535 : bestTicks;
  return true;
}

uint32_t MinimumPeriod(double integrationMs, TCS3448SmuxMode smux, TCS3448AutoZeroMode az) {
  const uint8_t cycles = static_cast<uint8_t>(smux) / 6;
  // AZ is typically 15 ms; reserve 20 ms per relevant cycle, plus 10 ms I2C/scheduling.
  const uint8_t zeroCycles = az == TCS3448AutoZeroMode::Disabled ? 0 :
      az == TCS3448AutoZeroMode::EveryMeasurement ? cycles : 1;
  return static_cast<uint32_t>(std::ceil(integrationMs * cycles + zeroCycles * 20.0 + 10.0));
}

TCS3448Error Validate(const TCS3448Config& config, TCS3448Timing& timing) {
  if (static_cast<uint8_t>(config.gain) > 12) return TCS3448Error::InvalidGain;
  if (!ResolveIntegration(config.integrationMs, timing)) return TCS3448Error::InvalidIntegration;
  if (config.ledCurrent < 4 || config.ledCurrent > 258 || config.ledCurrent % 2)
    return TCS3448Error::InvalidLedCurrent;
  const auto count = static_cast<uint8_t>(config.smux);
  if (count != 6 && count != 12 && count != 18) return TCS3448Error::InvalidSmuxMode;
  if (static_cast<uint8_t>(config.autozero) > 2) return TCS3448Error::InvalidAutozero;
  // Unsigned millis() comparisons remain unambiguous inside the half-range.
  if (!config.sampleIntervalMs || config.sampleIntervalMs > 0x7fffffffUL)
    return TCS3448Error::InvalidSamplePeriod;
  timing.minimumPeriodMs = MinimumPeriod(timing.integrationMs, config.smux, config.autozero);
  if (config.sampleIntervalMs < timing.minimumPeriodMs) return TCS3448Error::SamplePeriodTooShort;
  return TCS3448Error::None;
}
}
