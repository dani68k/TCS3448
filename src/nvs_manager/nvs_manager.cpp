#include "nvs_manager.h"
#include "TCS3448Manager/TCS3448ConfigMath.h"

#include <Preferences.h>
#include <cstring>
#include <limits>

namespace {
constexpr const char* Namespace = "tcs3448_setup";
constexpr const char* Key = "config";
constexpr size_t Size = 32;
constexpr uint8_t Version = 1;
static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559,
              "Setup format requires IEEE754 binary64");

uint32_t crc32(const uint8_t* bytes, size_t size) {
  uint32_t crc=0xffffffff;
  for(size_t i=0;i<size;++i) {
    crc^=bytes[i];
    for(unsigned bit=0;bit<8;++bit) crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));
  }
  return ~crc;
}
void put32(uint8_t* bytes,uint32_t n) {
  for(unsigned i=0;i<4;++i) bytes[i]=uint8_t(n>>(8*i));
}
uint32_t get32(const uint8_t* bytes) {
  uint32_t n=0; for(unsigned i=0;i<4;++i) n|=uint32_t(bytes[i])<<(8*i); return n;
}
void encode(const TCS3448Config& c,uint8_t* bytes) {
  memset(bytes,0,Size); memcpy(bytes,"TCS8",4);
  bytes[4]=Version; bytes[5]=uint8_t(c.gain); bytes[6]=uint8_t(c.smux); bytes[7]=uint8_t(c.autozero);
  bytes[8]=c.ledAuto?2:uint8_t(c.ledEnabled);
  bytes[10]=uint8_t(c.ledCurrent); bytes[11]=uint8_t(c.ledCurrent>>8);
  uint64_t bits=0; memcpy(&bits,&c.integrationMs,sizeof(bits));
  for(unsigned i=0;i<8;++i) bytes[12+i]=uint8_t(bits>>(8*i));
  put32(bytes+20,c.sampleIntervalMs); put32(bytes+28,crc32(bytes,28));
}
bool decode(const uint8_t* bytes,TCS3448Config& config) {
  if(memcmp(bytes,"TCS8",4)||bytes[4]!=Version||bytes[8]>2||bytes[9]||get32(bytes+24)||
      get32(bytes+28)!=crc32(bytes,28)) return false;
  auto candidate=config;
  candidate.gain=static_cast<TCS3448Gain>(bytes[5]);
  candidate.smux=static_cast<TCS3448SmuxMode>(bytes[6]);
  candidate.autozero=static_cast<TCS3448AutoZeroMode>(bytes[7]);
  candidate.ledEnabled=bytes[8]==1; candidate.ledAuto=bytes[8]==2;
  candidate.ledCurrent=uint16_t(bytes[10])|(uint16_t(bytes[11])<<8);
  uint64_t bits=0; for(unsigned i=0;i<8;++i) bits|=uint64_t(bytes[12+i])<<(8*i);
  memcpy(&candidate.integrationMs,&bits,sizeof(bits));
  candidate.sampleIntervalMs=get32(bytes+20);
  TCS3448Timing timing;
  if(TCS3448ConfigMath::Validate(candidate,timing)!=TCS3448Error::None) return false;
  config=candidate; return true;
}
}

namespace NVSManager {
Result Load(TCS3448Config& config) {
  Preferences preferences;
  if(!preferences.begin(Namespace,false)) return Result::Unavailable;
  if(!preferences.isKey(Key)) { preferences.end(); return Result::NotFound; }
  uint8_t bytes[Size];
  bool read=preferences.getBytesLength(Key)==Size && preferences.getBytes(Key,bytes,Size)==Size;
  preferences.end();
  if(!read||!decode(bytes,config)) return Result::InvalidData;
  return Result::Ok;
}
Result Save(const TCS3448Config& config) {
  TCS3448Timing timing;
  if(TCS3448ConfigMath::Validate(config,timing)!=TCS3448Error::None) return Result::InvalidData;
  uint8_t bytes[Size], verified[Size]; encode(config,bytes);
  Preferences preferences;
  if(!preferences.begin(Namespace,false)) return Result::Unavailable;
  // Avoid flash wear when the user reapplies the same confirmed setup.
  bool same=preferences.getBytesLength(Key)==Size && preferences.getBytes(Key,verified,Size)==Size &&
            memcmp(bytes,verified,Size)==0;
  bool saved=same || preferences.putBytes(Key,bytes,Size)==Size;
  saved=saved && preferences.getBytesLength(Key)==Size && preferences.getBytes(Key,verified,Size)==Size &&
        memcmp(bytes,verified,Size)==0;
  preferences.end();
  return saved?Result::Ok:Result::WriteFailed;
}
const char* ResultName(Result result) {
  switch(result) {
    case Result::Ok: return "OK";
    case Result::NotFound: return "NOT_FOUND";
    case Result::InvalidData: return "INVALID_DATA";
    case Result::Unavailable: return "NVS_UNAVAILABLE";
    default: return "WRITE_FAILED";
  }
}
}
