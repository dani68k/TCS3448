#include "SerialCommand.h"

#include <Arduino.h>

void processSerialCommands() {
  // No active commands. Discard input with a bounded budget per loop.
  for (uint8_t budget = 0; budget < 32 && Serial.available() > 0; ++budget) {
    if (Serial.read() < 0) return;
  }
}
