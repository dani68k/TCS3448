#pragma once

#include "TCS3448Types.h"

namespace TCS3448Manager {
bool Init(const TCS3448Config& config = TCS3448Config());
bool LEDEnable();
float GetIntegrationTime();
uint8_t GetGain();
bool ReadAllChannels(TCS3448Readings& readings);
void PrintHeader();
void PrintReadings();
}
