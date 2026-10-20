#pragma once
#include <Arduino.h>
#include <FS.h>
#include "TCS3448Manager/TCS3448Types.h"

namespace DataStore {
enum Error : uint8_t { OK, NO_SAMPLE, INVALID_DESCRIPTION, STORAGE_UNAVAILABLE, STORAGE_FULL,
  RECORD_NOT_FOUND, INVALID_RECORD_IDS, BUSY, CSV_CORRUPT, WRITE_FAILED, TRANSFER_FAILED, INVALID_PAYLOAD };
struct Snapshot { TCS3448Sample sample; TCS3448Config config; uint16_t fullScale=0; };
void Init();
Error Insert(const Snapshot&, const std::string& description, uint32_t request, uint32_t& id);
Error Delete(const uint32_t* ids, size_t count);
enum class Transfer : uint8_t { Csv = 0, Summaries = 1, Record = 2 };
Error Open(uint32_t id, uint32_t& size, uint32_t& crc, Transfer kind=Transfer::Csv);
Error Read(uint32_t offset, uint8_t* out, size_t& length, size_t capacity=12);
void Close();
uint16_t Count();
}
