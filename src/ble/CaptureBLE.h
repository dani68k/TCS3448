#pragma once
#include <BLEService.h>
#include "DataStore/DataStore.h"
namespace CaptureBLE {
bool Init(BLEService*);
void Reset();
void Update();
void Remember(const TCS3448Sample&);
}
