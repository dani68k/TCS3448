#include "DataStore.h"
#include "Csv.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <vector>

namespace {
using namespace DataStore;
Preferences prefs;
bool ready=false, transferring=false;
Error fault=STORAGE_UNAVAILABLE;
uint16_t rows=0;
uint32_t highest=0;
File download;
String single;
Transfer transferKind=Transfer::Csv;
uint32_t transferSize=0, projectedPosition=0;
std::string pendingSummary;
size_t pendingPosition=0;
constexpr const char* SummaryHeader="record_id,description,timestamp_ms,gain,integration_ms,led_current_ma";
struct Meta { uint16_t count=0; uint32_t highest=0, bytes=0, crc=0; };
bool line(File& f, std::string& out) {
  out.clear();
  while(f.available()) { int c=f.read(); if(c<0) return false; if(c=='\n') return true;
    if(c=='\r'||out.size()>=CaptureCSV::MaxLine) return false; out+=char(c); }
  return false; // Every line, including last, must be terminated.
}
bool summary(File& f, std::string& out) {
  std::string raw; std::vector<std::string> fields;
  if(!line(f,raw)||!CaptureCSV::split(raw,fields)||fields.size()!=36) return false;
  out=fields[0]+","+CaptureCSV::quote(fields[1]);
  for(unsigned i=2;i<6;++i) out+=","+fields[i];
  out+='\n'; return true;
}
bool resetSummary() {
  if(!download.seek(0)) return false;
  std::string header; if(!line(download,header)||header!=CaptureCSV::Header) return false;
  pendingSummary=std::string(SummaryHeader)+'\n'; pendingPosition=0; projectedPosition=0; return true;
}
bool scan(const char* path, Meta& m) {
  m=Meta(); File f=LittleFS.open(path,"r"); if(!f) return false;
  std::string s;
  if(!line(f,s)||s!=CaptureCSV::Header) return false;
  uint32_t requests[100]={};
  while(f.available()) {
    uint32_t id,req;
    if(!line(f,s)||!CaptureCSV::validate(s,id,req)||id<=m.highest||m.count>=100) return false;
    for(unsigned i=0;i<m.count;++i) if(requests[i]==req) return false;
    requests[m.count++]=req; m.highest=id;
  }
  m.bytes=f.size(); if(!f.seek(0)) return false;
  uint32_t crc=0xffffffff, read=0; uint8_t buffer[128];
  while(read<m.bytes) { size_t n=std::min(size_t(128),size_t(m.bytes-read)); if(f.read(buffer,n)!=n) return false;
    for(size_t i=0;i<n;++i) crc=CaptureCSV::crcByte(crc,buffer[i]); read+=n; yield(); }
  m.crc=~crc; return true;
}
Error check(bool validateFile=true) {
  if(!ready) return fault;
  if(transferring) return DataStore::BUSY;
  if(!validateFile) return DataStore::OK;
  Meta m; if(!scan("/data.csv",m)) return CSV_CORRUPT;
  rows=m.count; highest=m.highest; return DataStore::OK;
}
bool put(File& f,const std::string& s) { return f.write((const uint8_t*)s.data(),s.size())==s.size() && f.write('\n')==1; }
std::string serialize(const Snapshot& snap,const std::string& desc,uint32_t request,uint32_t id) {
  const auto& c=snap.config; const auto& a=snap.sample;
  const double gains[]={.5,1,2,4,8,16,32,64,128,256,512,1024,2048};
  String row=String(id)+","+CaptureCSV::quote(desc).c_str()+","+String(a.timestampMs)+","+String(gains[unsigned(a.gain)],1)+","+String(c.integrationMs,5)+","+String(c.ledCurrent)+","+(c.ledAuto?"AUTO":c.ledEnabled?"ON":"OFF")+","+String(unsigned(c.smux))+","+String(unsigned(c.autozero))+","+String(c.sampleIntervalMs)+","+String(snap.fullScale);
  const uint8_t order[]={12,6,0,7,8,15,1,2,9,13,14,3,4,5};
  for(uint8_t i:order) { row+=','; if(i<a.channelCount) row+=String(a.channels[i]); }
  row+=",1,unknown,unknown,"+String(a.sequence)+","+String(a.channelCount)+","+String(request)+","+String(a.saturated?1:0);
  for(uint8_t i:{10,11,16,17}) { row+=','; if(i<a.channelCount) row+=String(a.channels[i]); }
  return row.c_str();
}
Error replace(const std::string* added, const uint32_t* deleted, size_t count) {
  // Source has already been validated. Memory: one CSV line, not the whole file.
  File src=LittleFS.open("/data.csv","r"), dst=LittleFS.open("/data.tmp","w");
  if(!src||!dst) return WRITE_FAILED;
  uint32_t expectedCRC=0xffffffff, expectedSize=0;
  auto writeLine=[&](const std::string& value) {
    for(uint8_t b:value) expectedCRC=CaptureCSV::crcByte(expectedCRC,b);
    expectedCRC=CaptureCSV::crcByte(expectedCRC,'\n'); expectedSize+=value.size()+1;
    return put(dst,value);
  };
  std::string s; bool ok=line(src,s)&&writeLine(s); unsigned index=0;
  while(ok && src.available()) {
    uint32_t id,req; ok=line(src,s)&&CaptureCSV::validate(s,id,req); if(!ok) break;
    bool skip=added && rows==100 && index==0; ++index;
    for(size_t i=0;i<count;++i) if(deleted[i]==id) skip=true;
    if(!skip) ok=writeLine(s);
    yield();
  }
  if(ok&&added) ok=writeLine(*added);
  dst.flush(); dst.close(); src.close();
  Meta m;
  if(!ok||!scan("/data.tmp",m)||m.bytes!=expectedSize||m.crc!=~expectedCRC||m.count!=(added ? (rows<100?rows+1:100) : rows-count)) return WRITE_FAILED;
  // No dependency on overwrite-rename: destination is absent at both renames.
  if(LittleFS.exists("/data.bak")&&!LittleFS.remove("/data.bak")) return WRITE_FAILED;
  if(!LittleFS.rename("/data.csv","/data.bak")) return WRITE_FAILED;
  if(!LittleFS.rename("/data.tmp","/data.csv")) {
    if(!LittleFS.rename("/data.bak","/data.csv")) { ready=false; fault=WRITE_FAILED; }
    return WRITE_FAILED;
  }
  Meta verified;
  if(!scan("/data.csv",verified)||verified.bytes!=m.bytes||verified.crc!=m.crc) {
    ready=false; fault=CSV_CORRUPT; return WRITE_FAILED;
  }
  rows=m.count; highest=m.highest;
  // Valid new file is authoritative after reset; stale backup is harmless.
  LittleFS.remove("/data.bak"); return DataStore::OK;
}
}
namespace DataStore {
void Init() {
  Close(); prefs.end(); ready=false; fault=STORAGE_UNAVAILABLE; rows=0;
  if(!LittleFS.begin(false)) return;
  if(!prefs.begin("captures",false)) return;
  Meta m;
  if(LittleFS.exists("/data.csv")) {
    if(!scan("/data.csv",m)) { fault=CSV_CORRUPT; return; }
  } else if(LittleFS.exists("/data.bak")) {
    // Interrupted commit: restore the verified original. Never choose partial tmp.
    if(!scan("/data.bak",m)||!LittleFS.rename("/data.bak","/data.csv")) { fault=CSV_CORRUPT; return; }
  } else {
    File f=LittleFS.open("/data.csv","w");
    bool ok=f&&put(f,CaptureCSV::Header); f.flush(); f.close();
    if(!ok||!scan("/data.csv",m)) { fault=WRITE_FAILED; return; }
  }
  rows=m.count; highest=m.highest;
  uint32_t counter=prefs.getUInt("last",0);
  if(counter<highest && prefs.putUInt("last",highest)!=sizeof(uint32_t)) return;
  ready=true; fault=DataStore::OK;
}
uint16_t Count() { return rows; }
Error Insert(const Snapshot& snap,const std::string& desc,uint32_t request,uint32_t& id) {
  Error e=check(); if(e!=DataStore::OK) return e;
  if(!CaptureCSV::description(desc)) return INVALID_DESCRIPTION;
  if(!request) return INVALID_PAYLOAD;
  if(!snap.fullScale||!snap.sample.sequence||unsigned(snap.sample.gain)>12) return NO_SAMPLE;
  // Persistent deduplication includes reconnect/reboot, as long as row is retained.
  File f=LittleFS.open("/data.csv","r"); std::string s; line(f,s);
  while(f.available()) {
    uint32_t req;
    if(!line(f,s)||!CaptureCSV::validate(s,id,req)) return CSV_CORRUPT;
    if(req==request) {
      // Reject collisions unless the entire historical snapshot is identical.
      if(s!=serialize(snap,desc,request,id)) return INVALID_PAYLOAD;
      return DataStore::OK;
    }
  }
  f.close();
  uint32_t last=prefs.getUInt("last",0); if(last==UINT32_MAX) return STORAGE_FULL;
  id=last+1;
  // Reserve before touching CSV: a reset can leave a gap, never reuse an ID.
  if(prefs.putUInt("last",id)!=sizeof(uint32_t)) return WRITE_FAILED;
  s=serialize(snap,desc,request,id); uint32_t checked,req;
  if(!CaptureCSV::validate(s,checked,req)) return NO_SAMPLE;
  if(LittleFS.totalBytes()-LittleFS.usedBytes()<uint32_t(100*CaptureCSV::MaxLine+8192)) return STORAGE_FULL;
  return replace(&s,nullptr,0);
}
Error Delete(const uint32_t* ids,size_t count) {
  Error e=check(); if(e!=DataStore::OK) return e;
  if(!ids||!count||count>100) return INVALID_RECORD_IDS;
  bool found[100]={};
  for(size_t i=0;i<count;++i) { if(!ids[i]) return INVALID_RECORD_IDS; for(size_t j=0;j<i;++j) if(ids[j]==ids[i]) return INVALID_RECORD_IDS; }
  File f=LittleFS.open("/data.csv","r"); std::string s; line(f,s);
  while(f.available()) { uint32_t id,req; if(!line(f,s)||!CaptureCSV::validate(s,id,req)) return CSV_CORRUPT; for(size_t i=0;i<count;++i) if(ids[i]==id) found[i]=true; }
  f.close(); for(size_t i=0;i<count;++i) if(!found[i]) return RECORD_NOT_FOUND;
  return replace(nullptr,ids,count);
}
Error Open(uint32_t id,uint32_t& size,uint32_t& crc,Transfer kind) {
  // GET only traverses to the requested ID; mutations still validate the whole file.
  Error e=check(false); if(e!=DataStore::OK) return e;
  if((kind==Transfer::Record&&!id)||(kind==Transfer::Summaries&&id)) return INVALID_PAYLOAD;
  Meta m;
  if(!id) { if(!scan("/data.csv",m)) return CSV_CORRUPT; rows=m.count; }
  transferKind=kind;
  single="";
  if(id) {
    File f=LittleFS.open("/data.csv","r"); std::string s; bool found=false;
    if(!f||!line(f,s)||s!=CaptureCSV::Header) return CSV_CORRUPT;
    while(f.available()) { uint32_t n,r; if(!line(f,s)||!CaptureCSV::validate(s,n,r)) return CSV_CORRUPT; if(n==id) { found=true; break; } }
    if(!found) return RECORD_NOT_FOUND;
    single=kind==Transfer::Record ? String(s.c_str())+"\n" : String(CaptureCSV::Header)+"\n"+s.c_str()+"\n"; size=single.length(); uint32_t c=0xffffffff;
    for(unsigned i=0;i<size;++i) c=CaptureCSV::crcByte(c,single[i]); crc=~c;
  } else {
    download=LittleFS.open("/data.csv","r"); if(!download) return TRANSFER_FAILED; size=m.bytes; crc=m.crc;
    if(kind==Transfer::Summaries) {
      if(!resetSummary()) { Close(); return TRANSFER_FAILED; }
      uint32_t c=0xffffffff; size=0;
      do {
        for(uint8_t b:pendingSummary) c=CaptureCSV::crcByte(c,b);
        size+=pendingSummary.size();
        if(!download.available()) break;
        if(!summary(download,pendingSummary)) { Close(); return CSV_CORRUPT; }
      } while(true);
      crc=~c;
      if(!resetSummary()) { Close(); return TRANSFER_FAILED; }
    }
  }
  transferSize=size;
  transferring=true; return DataStore::OK;
}
Error Read(uint32_t offset,uint8_t* out,size_t& length,size_t capacity) {
  if(!transferring) return TRANSFER_FAILED;
  if((capacity!=12&&capacity!=504)||offset>=transferSize||offset%capacity) return INVALID_PAYLOAD;
  length=std::min(capacity,size_t(transferSize-offset));
  if(transferKind==Transfer::Summaries) {
    // Sequential virtual CSV, bounded to one projected row. Retry seeks rebuild it.
    if(offset!=projectedPosition&&!resetSummary()) return TRANSFER_FAILED;
    size_t copied=0;
    while(copied<length) {
      if(pendingPosition==pendingSummary.size()) {
        if(!summary(download,pendingSummary)) return TRANSFER_FAILED;
        pendingPosition=0;
      }
      if(projectedPosition<offset) {
        size_t skip=std::min(size_t(offset-projectedPosition),pendingSummary.size()-pendingPosition);
        projectedPosition+=skip; pendingPosition+=skip; continue;
      }
      size_t n=std::min(length-copied,pendingSummary.size()-pendingPosition);
      memcpy(out+copied,pendingSummary.data()+pendingPosition,n); copied+=n; pendingPosition+=n; projectedPosition+=n;
    }
    return DataStore::OK;
  }
  if(single.length()) memcpy(out,single.c_str()+offset,length);
  else if(!download.seek(offset)||download.read(out,length)!=length) return TRANSFER_FAILED;
  return DataStore::OK;
}
void Close() { download.close(); single=""; pendingSummary.clear(); transferring=false; transferSize=0; }
}
