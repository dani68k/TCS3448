#include <cassert>
#include <iostream>
#include <limits>
#include "host/Preferences.h"
uint32_t Preferences::counter=777;
bool Preferences::fail=false;
#include "../src/TCS3448Manager/TCS3448ConfigMath.cpp"
#include "../src/nevs_manager/nvs_manager.cpp"
using NVSManager::Result;

void refreshCRC(std::vector<uint8_t>& b) { put32(b.data()+28,crc32(b.data(),28)); }
int main() {
  TCS3448Config defaults, loaded;
  loaded.gain=TCS3448Gain::X2;
  assert(NVSManager::Load(loaded)==Result::NotFound&&loaded.gain==TCS3448Gain::X2);
  TCS3448Config confirmed;
  confirmed.gain=TCS3448Gain::X2048; confirmed.integrationMs=123.45678;
  confirmed.ledCurrent=258; confirmed.smux=TCS3448SmuxMode::Channels12;
  confirmed.autozero=TCS3448AutoZeroMode::EveryMeasurement; confirmed.sampleIntervalMs=999;
  for(unsigned mode=0;mode<3;++mode) {
    confirmed.ledAuto=mode==2; confirmed.ledEnabled=mode==1;
    assert(NVSManager::Save(confirmed)==Result::Ok);
    loaded=defaults; loaded.sdaPin=21; loaded.sclPin=22; loaded.i2cFrequency=100000;
    assert(NVSManager::Load(loaded)==Result::Ok);
    assert(loaded.gain==confirmed.gain&&loaded.integrationMs==confirmed.integrationMs&&loaded.ledCurrent==258);
    assert(loaded.smux==confirmed.smux&&loaded.autozero==confirmed.autozero&&loaded.sampleIntervalMs==999);
    assert(loaded.ledAuto==(mode==2)&&loaded.ledEnabled==(mode==1));
    assert(loaded.sdaPin==21&&loaded.sclPin==22&&loaded.i2cFrequency==100000);
    unsigned writes=Preferences::blobWrites;
    assert(NVSManager::Save(confirmed)==Result::Ok&&Preferences::blobWrites==writes);
  }
  assert(Preferences::counter==777); // Capture IDs use a different namespace/key.
  const auto original=Preferences::blobs["tcs3448_setup/config"];
  assert(original.size()==32);
  confirmed.sampleIntervalMs=1001;
  Preferences::failWrite=true;
  assert(NVSManager::Save(confirmed)==Result::WriteFailed);
  assert(Preferences::blobs["tcs3448_setup/config"]==original);
  Preferences::failWrite=false;
  assert(NVSManager::Save(confirmed)==Result::Ok);
  Preferences::fail=true;
  assert(NVSManager::Save(confirmed)==Result::Unavailable&&NVSManager::Load(loaded)==Result::Unavailable);
  Preferences::fail=false; Preferences::failRead=true;
  loaded=defaults;
  assert(NVSManager::Load(loaded)==Result::InvalidData&&loaded.gain==defaults.gain);
  Preferences::failRead=false;
  for(unsigned failure=0;failure<6;++failure) {
    auto damaged=original;
    if(failure==0) damaged[0]^=1; // CRC/magic
    if(failure==1) {damaged[4]=2;refreshCRC(damaged);} // unsupported version
    if(failure==2) {damaged[8]=3;refreshCRC(damaged);} // invalid LED mode
    if(failure==3) {damaged[5]=255;refreshCRC(damaged);} // invalid config despite valid CRC
    if(failure==4) {put32(damaged.data()+20,1);refreshCRC(damaged);} // insufficient period
    if(failure==5) damaged.resize(16);
    Preferences::blobs["tcs3448_setup/config"]=damaged;
    loaded=defaults;
    assert(NVSManager::Load(loaded)==Result::InvalidData&&loaded.integrationMs==defaults.integrationMs);
    assert(Preferences::blobs["tcs3448_setup/config"]==damaged); // no repair/erase on load
  }
  unsigned writes=Preferences::blobWrites;
  confirmed.integrationMs=std::numeric_limits<double>::quiet_NaN();
  assert(NVSManager::Save(confirmed)==Result::InvalidData&&Preferences::blobWrites==writes);
  confirmed=defaults; Preferences::corruptWrite=true;
  assert(NVSManager::Save(confirmed)==Result::WriteFailed); // readback verification
  std::cout<<"NVS setup: roundtrip OFF/ON/AUTO, precision, bus preserved, dedup, namespace isolation, validation and read/write failures passed\n";
}
