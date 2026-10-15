#pragma once

#include "TCS3448Types.h"

namespace TCS3448Manager {
bool Init(const TCS3448Config& config = TCS3448Config());
bool LEDEnable();
float GetIntegrationTime();
uint8_t GetGain();
bool ReadAllChannels(TCS3448Readings& readings);
const TCS3448Config& GetConfig();
const TCS3448Status& GetStatus();
bool StartAcquisition();
bool StopAcquisition();
// Poll from loop(); true only when a complete, new sample is available.
bool Update(TCS3448Sample& sample);
void PrintHeader();
void PrintReadings();
}
