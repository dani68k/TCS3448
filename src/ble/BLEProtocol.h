#pragma once

#include <stddef.h>
#include <stdint.h>
#include "TCS3448Manager/TCS3448Types.h"

namespace AtlantesProtocol {
constexpr char DeviceName[] = "ATLANTES-TCS3448";
constexpr char ServiceUUID[] = "a71a0001-3448-4c53-8a7e-41544c414e54";
constexpr char StatusUUID[] = "a71a0002-3448-4c53-8a7e-41544c414e54";
constexpr char CommandUUID[] = "a71a0003-3448-4c53-8a7e-41544c414e54";
constexpr char SampleUUID[] = "a71a0004-3448-4c53-8a7e-41544c414e54";
constexpr char ConfigUUID[] = "a71a0005-3448-4c53-8a7e-41544c414e54";
constexpr uint8_t Version = 1;
constexpr size_t PacketSize = 20;
constexpr size_t SampleSize = 48;
constexpr uint8_t FragmentCount = 4;

enum class Command : uint8_t { None = 0, Start = 1, Stop = 2, GetStatus = 3, GetConfig = 4, Invalid = 255 };
enum class Result : uint8_t { Ok = 0, UnknownCommand = 1, SensorError = 2, QueueFull = 3 };

// Explicit little-endian encoding; never send a C++ struct's memory layout.
inline void Put16(uint8_t* out, uint16_t value) {
  out[0] = value & 0xff;
  out[1] = value >> 8;
}
inline void Put32(uint8_t* out, uint32_t value) {
  for (uint8_t i = 0; i < 4; ++i) out[i] = (value >> (8 * i)) & 0xff;
}
inline void EncodeSample(const TCS3448Sample& sample, uint8_t* out) {
  out[0] = Version;
  out[1] = static_cast<uint8_t>(sample.gain);
  out[2] = sample.saturated ? 1 : 0;
  out[3] = TCS3448_CHANNEL_COUNT;
  Put32(out + 4, sample.sequence);
  Put32(out + 8, sample.timestampMs);
  for (uint8_t i = 0; i < TCS3448_CHANNEL_COUNT; ++i) Put16(out + 12 + i * 2, sample.channels[i]);
}
}
