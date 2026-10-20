#pragma once
#include "Arduino.h"
#include <memory>
#include <map>
struct SimulatedReset {};
struct FakeFS;
extern FakeFS LittleFS;
class File {
  std::shared_ptr<std::string> data;
  size_t position=0;
  bool writable=false;
public:
  File()=default;
  File(std::shared_ptr<std::string> value,bool write):data(value),writable(write){}
  explicit operator bool() const { return bool(data); }
  bool available() { return data&&position<data->size(); }
  int read();
  size_t read(uint8_t* out,size_t n) { size_t i=0; while(i<n&&available()) {int c=read();if(c<0)break;out[i++]=uint8_t(c);} return i; }
  size_t write(const uint8_t*,size_t);
  size_t write(uint8_t b) { return write(&b,1); }
  void flush() {}
  void close() { data.reset(); }
  size_t size() { return data?data->size():0; }
  bool seek(size_t p) { if(!data||p>data->size()) return false; position=p; return true; }
};
struct FakeFS {
  std::map<std::string,std::shared_ptr<std::string>> files;
  bool mounted=true;
  int renameStep=0, resetAt=0, failRenameAt=0, writesLeft=-1, failReadAt=-1;
  size_t capacity=0xe0000;
  bool begin(bool format) { if(format) throw std::runtime_error("autoformat forbidden"); return mounted; }
  bool exists(const char* p) { return files.count(p)>0; }
  File open(const char* p,const char* mode) {
    if(!mounted) return {};
    if(mode[0]=='w') { files[p]=std::make_shared<std::string>(); return File(files[p],true); }
    auto it=files.find(p); return it==files.end()?File():File(it->second,false);
  }
  bool remove(const char* p) { return files.erase(p)!=0; }
  bool rename(const char* a,const char* b) {
    ++renameStep;
    if(resetAt==renameStep) throw SimulatedReset();
    if(failRenameAt==renameStep||!exists(a)||exists(b)) return false;
    files[b]=files[a]; files.erase(a); return true;
  }
  size_t totalBytes() { return capacity; }
  size_t usedBytes() { size_t n=0; for(const auto& f:files) n+=f.second->size(); return n; }
  std::string content(const char* p) { return exists(p)?*files[p]:""; }
};
inline int File::read() {
  if(LittleFS.failReadAt>=0&&position>=size_t(LittleFS.failReadAt)) return -1;
  return available()?uint8_t((*data)[position++]):-1;
}
inline size_t File::write(const uint8_t* bytes,size_t n) {
  if(!data||!writable||LittleFS.writesLeft==0) return 0;
  if(LittleFS.writesLeft>0) --LittleFS.writesLeft;
  data->append((const char*)bytes,n); position+=n; return n;
}
