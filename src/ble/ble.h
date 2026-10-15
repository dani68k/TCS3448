#pragma once

namespace TCS3448BLE {
// Initialize once after the sensor. BLE remains available if the sensor fails.
bool Init();
// Process queued commands, sensor polling and paced notifications on loop().
void Update();
}
