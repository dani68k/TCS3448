#pragma once

#include <stddef.h>
#include <stdint.h>
#include "TCS3448Manager/TCS3448Types.h"

namespace TCS3448Protocol {
constexpr char DeviceName[] = "TCS3448";
constexpr char ServiceUUID[] = "34480001-7f2a-4f6c-9e31-8dbe0c2f5a70";
constexpr char StatusUUID[] = "34480002-7f2a-4f6c-9e31-8dbe0c2f5a70";
constexpr char CommandUUID[] = "34480003-7f2a-4f6c-9e31-8dbe0c2f5a70";
constexpr char SampleUUID[] = "34480004-7f2a-4f6c-9e31-8dbe0c2f5a70";
constexpr char ConfigUUID[] = "34480005-7f2a-4f6c-9e31-8dbe0c2f5a70";
constexpr char CaptureUUID[] = "34480006-7f2a-4f6c-9e31-8dbe0c2f5a70";
constexpr uint8_t Version = 1;
constexpr uint8_t ConfigVersion = 3;
constexpr uint8_t ApplyTag = 0xa4;
constexpr size_t PacketSize = 20;
constexpr size_t SampleSize = 48;
constexpr uint8_t FragmentCount = 4;

enum class Command : uint8_t { None = 0, Start = 1, Stop = 2, GetStatus = 3, GetConfig = 4, ApplyConfig = 5, Invalid = 255 };
enum class Result : uint8_t { Ok = 0, UnknownCommand = 1, SensorError = 2, QueueFull = 3, InvalidConfig = 4, ConfigApplyFailed = 5, ConfigSaveFailed = 6 };

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
  out[3] = sample.channelCount;
  Put32(out + 4, sample.sequence);
  Put32(out + 8, sample.timestampMs);
  for (uint8_t i = 0; i < TCS3448MaxChannels; ++i) Put16(out + 12 + i * 2, sample.channels[i]);
}
}

namespace TCS3448Protocol {
inline uint16_t Get16(const uint8_t* in) { return in[0] | (uint16_t(in[1]) << 8); }
inline uint32_t Get32(const uint8_t* in) {
  return uint32_t(in[0]) | (uint32_t(in[1]) << 8) | (uint32_t(in[2]) << 16) | (uint32_t(in[3]) << 24);
}
inline bool DecodeApply(const uint8_t* packet, size_t size, TCS3448Config& config) {
  if (size != PacketSize || packet[0] != ApplyTag || (packet[1] != 2 && packet[1] != ConfigVersion) ||
      packet[5] > (packet[1] == 2 ? 1 : 2) || Get32(packet + 16) != 0) return false;
  config.gain = static_cast<TCS3448Gain>(packet[2]);
  config.smux = static_cast<TCS3448SmuxMode>(packet[3]);
  config.autozero = static_cast<TCS3448AutoZeroMode>(packet[4]);
  config.ledEnabled = packet[5] == 1;
  config.ledAuto = packet[5] == 2;
  config.ledCurrent = Get16(packet + 6);
  config.integrationMs = Get32(packet + 8) / 1000.0;
  config.sampleIntervalMs = Get32(packet + 12);
  return true;
}
}
