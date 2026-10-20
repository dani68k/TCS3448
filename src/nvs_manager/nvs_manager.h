#pragma once

#include "TCS3448Manager/TCS3448Types.h"

namespace NVSManager {
enum class Result { Ok, NotFound, InvalidData, Unavailable, WriteFailed };

// Load only editable sensor settings; preserve the caller's boot-only I2C setup.
// On any error, config remains unchanged.
Result Load(TCS3448Config& config);
// Call only with configuration confirmed by the sensor after APPLY.
Result Save(const TCS3448Config& config);
const char* ResultName(Result result);
}
