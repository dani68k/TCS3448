#include <stdexcept>
#include <iostream>
#include <cassert>
#include "host/FS.h"
#include "host/Preferences.h"
FakeFS LittleFS;
uint32_t Preferences::counter=0;
bool Preferences::fail=false;
#include "../src/DataStore/DataStore.cpp"
#include "../src/ble/CaptureBLE.cpp"
namespace TCS3448Manager {
TCS3448Config fakeConfig;
const TCS3448Config& GetConfig() { return fakeConfig; }
uint16_t GetADCFullScale() { return 18000; }
}
using namespace TCS3448Protocol;
BLEService service;
uint32_t req=100;
uint8_t command[20]={};
void prepare(uint8_t op) { memset(command,0,20); command[0]=op; Put32(command+2,++req); }
const uint8_t* execute() {
  service.characteristic.clientWrite(command,20);
  const auto* before=service.characteristic.clientRead();
  assert(Get32(before+2)!=req); // Must not ACK the unprocessed characteristic write echo.
  CaptureBLE::Update();
  const auto* after=service.characteristic.clientRead(); assert(Get32(after+2)==req); return after;
}
int main() {
  assert(CaptureBLE::Init(&service)); CaptureBLE::Reset();
  prepare(1); Put32(command+8,1); assert(execute()[1]==DataStore::NO_SAMPLE);
  TCS3448Sample s; s.sequence=1; s.timestampMs=123; s.channels[0]=42;
  CaptureBLE::Remember(s);
  prepare(1); Put32(command+8,1); uint32_t pin=req; assert(execute()[1]==0);
  // Acquisition and setup continue after pin. Store must retain historical values.
  TCS3448Manager::fakeConfig.integrationMs=99.0; s.sequence=2; s.channels[0]=999; CaptureBLE::Remember(s);
  prepare(2); Put16(command+8,0); command[10]=3; memcpy(command+11,"old",3); assert(execute()[1]==0);
  prepare(3); Put32(command+8,pin); Put16(command+12,3); assert(execute()[1]==0&&DataStore::Count()==1);
  std::string raw=LittleFS.content("/data.csv"); auto start=raw.find('\n')+1;
  // Fast long-read blocks carry actual CSV bytes; VIEW has no repeated header.
  prepare(11); command[8]=0; uint32_t fastToken=req; auto fast=execute();
  assert(fast[1]==0&&Get16(fast+18)==504&&Get32(fast+8)==raw.size());
  prepare(12); Put32(command+12,fastToken); fast=execute();
  size_t expected=std::min(size_t(504),raw.size());
  assert(fast[1]==0&&service.characteristic.getLength()==8+expected&&memcmp(fast+8,raw.data(),expected)==0);
  prepare(6); Put32(command+8,fastToken); assert(execute()[1]==0);
  prepare(11); command[8]=2; Put32(command+12,1); fastToken=req; fast=execute(); assert(fast[1]==0&&Get32(fast+8)==raw.size()-start);
  prepare(12); Put32(command+12,fastToken); fast=execute(); assert(fast[1]==0&&memcmp(fast+8,raw.data()+start,raw.size()-start)==0);
  prepare(6); Put32(command+8,fastToken); assert(execute()[1]==0);
  prepare(11); command[8]=1; fastToken=req; fast=execute(); assert(fast[1]==0&&Get32(fast+8)<raw.size());
  prepare(12); Put32(command+12,fastToken); fast=execute(); assert(fast[1]==0&&memcmp(fast+8,"record_id,description",21)==0);
  prepare(6); Put32(command+8,fastToken); assert(execute()[1]==0);
  std::vector<std::string> f;
  assert(CaptureCSV::split(raw.substr(start,raw.size()-start-1),f));
  assert(f[4]=="50.04000"&&f[13]=="42"&&f[28]=="1");
  prepare(3); Put32(command+8,pin); Put16(command+12,3); assert(execute()[1]==0&&DataStore::Count()==1);
  prepare(4); uint32_t token=req; auto result=execute(); assert(result[1]==0); uint32_t size=Get32(result+8); assert(size==raw.size());
  prepare(3); Put32(command+8,pin); Put16(command+12,3); assert(execute()[1]==DataStore::BUSY);
  prepare(5); Put32(command+8,0); Put32(command+12,token+1); assert(execute()[1]==DataStore::TRANSFER_FAILED);
  prepare(5); Put32(command+8,0); Put32(command+12,token); result=execute(); assert(result[1]==0&&memcmp(result+8,raw.data(),12)==0);
  fakeMillis=16000; CaptureBLE::Update(); // abandoned pull releases storage lock
  prepare(9); assert(execute()[1]==0);
  prepare(2); command[10]=4; Put32(command+11,1); assert(execute()[1]==0);
  prepare(7); Put16(command+8,1); uint32_t receipt=req; result=execute(); assert(result[1]==0&&Get16(result+8)==0&&Get16(result+10)==1);
  prepare(10); Put32(command+8,0); Put32(command+12,receipt); result=execute(); assert(result[1]==0&&Get32(result+8)==1);
  prepare(1); Put32(command+8,2); service.characteristic.clientWrite(command,20);
  CaptureBLE::Reset(); CaptureBLE::Update(); assert(Get32(service.characteristic.clientRead()+2)==0); // old-session work discarded
  prepare(3); Put32(command+8,pin); Put16(command+12,3); assert(execute()[1]==DataStore::NO_SAMPLE);
  std::cout<<"Capture BLE: processed ACK cache, historical pin, idempotency, BUSY, token checks, timeout, delete receipt and session isolation passed\n";
}
