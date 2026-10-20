// Compile the production DataStore against a fault-injectable filesystem/NVS.
#include <stdexcept>
#include <iostream>
#include <cassert>
#include "host/FS.h"
#include "host/Preferences.h"
FakeFS LittleFS;
uint32_t Preferences::counter=0;
bool Preferences::fail=false;
#include "../src/DataStore/DataStore.cpp"
using namespace DataStore;
Snapshot sample() {
  Snapshot s; s.fullScale=18000; s.sample.sequence=1; s.sample.timestampMs=1234;
  for(unsigned i=0;i<18;++i) s.sample.channels[i]=uint16_t(i*100);
  return s;
}
void fresh() { LittleFS=FakeFS(); Preferences::counter=0; Preferences::fail=false; Init(); assert(Count()==0); }
uint32_t add(uint32_t request,const std::string& desc="capture") { uint32_t id=0; assert(Insert(sample(),desc,request,id)==OK); return id; }
std::vector<uint32_t> ids() {
  std::vector<uint32_t> result; File f=LittleFS.open("/data.csv","r"); std::string s; assert(line(f,s));
  while(f.available()) { uint32_t id,r; assert(line(f,s)&&CaptureCSV::validate(s,id,r)); result.push_back(id); }
  return result;
}
int main() {
  fresh();
  Snapshot frozen=sample(); frozen.config.integrationMs=25.12345; frozen.config.ledAuto=true;
  frozen.config.ledCurrent=258; frozen.config.autozero=TCS3448AutoZeroMode::EveryMeasurement;
  frozen.config.sampleIntervalMs=999; frozen.fullScale=12345;
  uint32_t historical=0; assert(Insert(frozen,"historical",77,historical)==OK);
  std::vector<std::string> saved; std::string raw=LittleFS.content("/data.csv");
  auto start=raw.find('\n')+1; assert(CaptureCSV::split(raw.substr(start,raw.size()-start-1),saved));
  assert(saved[4]=="25.12345"&&saved[5]=="258"&&saved[6]=="AUTO"&&saved[8]=="2"&&saved[9]=="999"&&saved[10]=="12345");
  assert(saved[11]=="1200"&&saved[13]=="0"&&saved[26]=="unknown"&&saved[27]=="unknown");
  fresh(); auto partial=sample(); partial.sample.channelCount=6; partial.config.smux=TCS3448SmuxMode::Channels6;
  assert(Insert(partial,"six",1,historical)==OK);
  raw=LittleFS.content("/data.csv"); start=raw.find('\n')+1; CaptureCSV::split(raw.substr(start,raw.size()-start-1),saved);
  assert(saved[11].empty()&&saved[13]=="0"&&saved[23]=="400"&&saved[32].empty());
  fresh(); assert(add(1,"quote \" and , comma; caf\xc3\xa9")==1);
  auto original=LittleFS.content("/data.csv"); uint32_t id=0;
  assert(Insert(sample(),"quote \" and , comma; caf\xc3\xa9",1,id)==OK&&id==1&&LittleFS.content("/data.csv")==original);
  assert(Insert(sample(),"different",1,id)==INVALID_PAYLOAD);
  Snapshot collision=sample(); collision.sample.channels[0]++;
  assert(Insert(collision,"quote \" and , comma; caf\xc3\xa9",1,id)==INVALID_PAYLOAD);
  assert(Insert(sample(),"new\nline",2,id)==INVALID_DESCRIPTION);
  assert(Insert(sample(),std::string(81,'a'),2,id)==INVALID_DESCRIPTION);
  assert(Insert(sample(),std::string("\xc0\x80"),2,id)==INVALID_DESCRIPTION);
  for(uint32_t i=2;i<=99;++i) add(i);
  assert(Count()==99&&ids().front()==1);
  add(100); assert(Count()==100&&ids().front()==1);
  // VIEW stops at its row, without a second full-file CRC scan.
  std::string beforeView=LittleFS.content("/data.csv");
  LittleFS.failReadAt=int(beforeView.find('\n',strlen(CaptureCSV::Header)+1)+1);
  uint32_t earlySize=0,earlyCRC=0; assert(Open(1,earlySize,earlyCRC,Transfer::Record)==OK); Close();
  LittleFS.failReadAt=-1;
  // Compact table projection streams across row boundaries, with exact CRC and retry.
  uint32_t fastSize=0,fastCRC=0; assert(Open(0,fastSize,fastCRC,Transfer::Summaries)==OK);
  assert(fastSize<LittleFS.content("/data.csv").size());
  std::string summaries; uint32_t sumCRC=0xffffffff;
  for(uint32_t off=0;off<fastSize;off+=504) {
    uint8_t b[504],retry[504]; size_t n=0,again=0; assert(Read(off,b,n,504)==OK);
    if(off==504) {assert(Read(off,retry,again,504)==OK&&again==n&&memcmp(b,retry,n)==0);}
    summaries.append((char*)b,n); for(size_t j=0;j<n;++j)sumCRC=CaptureCSV::crcByte(sumCRC,b[j]);
  }
  assert(~sumCRC==fastCRC&&std::count(summaries.begin(),summaries.end(),'\n')==101);
  assert(summaries.find("unknown")==std::string::npos&&summaries.find("quote \"\" and , comma")!=std::string::npos); Close();
  assert(Open(50,fastSize,fastCRC,Transfer::Record)==OK);
  uint8_t rowBytes[504]; size_t rowLength=0; assert(Read(0,rowBytes,rowLength,504)==OK);
  assert(rowLength==fastSize&&std::string((char*)rowBytes,rowLength).find("50,\"capture\"")==0);
  assert(std::string((char*)rowBytes,rowLength).find("record_id")==std::string::npos); Close();
  add(101); assert(Count()==100&&ids().front()==2&&ids().back()==101);
  uint32_t remove[]={2,50,101}; assert(Delete(remove,3)==OK); assert(Count()==97);
  auto current=ids(); assert(std::find(current.begin(),current.end(),50)==current.end());
  uint32_t bad[]={3,999}; original=LittleFS.content("/data.csv"); assert(Delete(bad,2)==RECORD_NOT_FOUND&&LittleFS.content("/data.csv")==original);
  uint32_t repeated[]={3,3}; assert(Delete(repeated,2)==INVALID_RECORD_IDS);
  assert(Delete(nullptr,0)==INVALID_RECORD_IDS);
  assert(add(102)==102&&Count()==98); Init(); assert(Count()==98);
  current=ids(); assert(Delete(current.data(),current.size())==OK&&Count()==0); Init(); assert(add(103)==103);
  uint32_t size=0,crc=0; assert(Open(0,size,crc)==OK);
  assert(Insert(sample(),"busy",104,id)==BUSY); assert(Delete(remove,1)==BUSY);
  std::string downloaded; uint32_t c=0xffffffff;
  for(uint32_t off=0;off<size;off+=12) { uint8_t b[12]; size_t n=0; assert(Read(off,b,n)==OK); downloaded.append((char*)b,n); for(size_t j=0;j<n;++j)c=CaptureCSV::crcByte(c,b[j]); }
  assert(downloaded==LittleFS.content("/data.csv")&&~c==crc); Close();
  assert(Open(103,size,crc)==OK); Close(); assert(Open(999,size,crc)==RECORD_NOT_FOUND);
  current=ids(); assert(Delete(current.data(),current.size())==OK); assert(Open(0,size,crc)==OK&&size==strlen(CaptureCSV::Header)+1); Close();
  assert(Open(0,size,crc,Transfer::Summaries)==OK);
  uint8_t emptyList[504]; size_t emptyLength=0; assert(Read(0,emptyList,emptyLength,504)==OK);
  assert(emptyLength==strlen(SummaryHeader)+1&&memcmp(emptyList,SummaryHeader,emptyLength-1)==0); Close();
  // Both rename failures keep original; IDs reserved before failure remain gaps.
  for(int failure=1;failure<=2;++failure) {
    fresh(); add(1); original=LittleFS.content("/data.csv"); LittleFS.renameStep=0; LittleFS.failRenameAt=failure;
    assert(Insert(sample(),"failed",2,id)==WRITE_FAILED); assert(LittleFS.content("/data.csv")==original);
    LittleFS.failRenameAt=0; Init(); assert(add(3)==3);
  }
  // Reset just before first/second rename; startup restores original, not tmp.
  for(int reset=1;reset<=2;++reset) {
    fresh(); add(1); original=LittleFS.content("/data.csv"); LittleFS.renameStep=0; LittleFS.resetAt=reset;
    try { Insert(sample(),"reset",2,id); assert(false); } catch(const SimulatedReset&) {}
    LittleFS.resetAt=0; Init(); assert(LittleFS.content("/data.csv")==original&&Count()==1); assert(add(3)==3);
  }
  fresh(); add(1); original=LittleFS.content("/data.csv"); LittleFS.writesLeft=1;
  assert(Insert(sample(),"full",2,id)==WRITE_FAILED); assert(LittleFS.content("/data.csv")==original);
  LittleFS.writesLeft=-1; Init(); assert(add(3)==3);
  // Valid committed CSV wins over stale backup after interrupted cleanup.
  LittleFS.files["/data.bak"]=std::make_shared<std::string>(original); Init(); assert(Count()==2);
  // Corruption is preserved and reported, including empty existing file.
  for(const auto& invalid:{std::string(),std::string("bad header\n"),original+"bad row\n"}) {
    fresh(); LittleFS.files["/data.csv"]=std::make_shared<std::string>(invalid); Init();
    assert(Insert(sample(),"x",1,id)==CSV_CORRUPT&&LittleFS.content("/data.csv")==invalid);
  }
  fresh(); LittleFS.mounted=false; Init(); assert(Insert(sample(),"x",1,id)==STORAGE_UNAVAILABLE);
  fresh(); Preferences::fail=true; assert(Insert(sample(),"x",1,id)==WRITE_FAILED&&Count()==0);
  fresh(); add(1); original=LittleFS.content("/data.csv"); LittleFS.capacity=1024;
  assert(Insert(sample(),"full",2,id)==STORAGE_FULL&&LittleFS.content("/data.csv")==original);
  fresh(); add(1); original=LittleFS.content("/data.csv"); LittleFS.failReadAt=int(strlen(CaptureCSV::Header)+5);
  assert(Insert(sample(),"read error",2,id)==CSV_CORRUPT&&LittleFS.content("/data.csv")==original);
  std::cout<<"Storage: FIFO 1/99/100/101, CSV, IDs, dedup, CRUD, CRC transfer, reset recovery and injected failures passed\n";
}
