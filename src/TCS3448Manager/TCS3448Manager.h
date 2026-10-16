#pragma once

#include "TCS3448Types.h"

namespace TCS3448Manager {
bool Init(const TCS3448Config& config = TCS3448Config());
const TCS3448Config& GetConfig();
const TCS3448Status& GetStatus();
uint16_t GetADCFullScale();
uint32_t GetMinimumPeriodMs();
bool RefreshConfig();
TCS3448Error ApplyConfig(const TCS3448Config& config);
const char* ErrorName(TCS3448Error error);
bool StartAcquisition();
// STOP turns AUTO off and preserves manual LED. Disconnect/fault always turns it off.
bool StopAcquisition(bool turnOffLED = false);
bool Update(TCS3448Sample& sample);
// Diagnostic compatibility API; not used by the BLE acquisition path.
bool LEDEnable();
float GetIntegrationTime();
uint8_t GetGain();
bool ReadAllChannels(TCS3448Readings& readings);
void PrintHeader();
void PrintReadings();
}
