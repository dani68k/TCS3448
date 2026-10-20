#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <map>
#include <string>
#include <vector>
class Preferences {
  std::string name;
  std::string path(const char* key) const { return name+"/"+key; }
public:
  static uint32_t counter;
  static bool fail;
  inline static std::map<std::string,std::vector<uint8_t>> blobs;
  inline static bool failRead=false, failWrite=false, corruptWrite=false;
  inline static unsigned blobWrites=0;
  bool begin(const char* ns,bool) { name=ns; return !fail; }
  void end() {}
  uint32_t getUInt(const char*,uint32_t) { return counter; }
  size_t putUInt(const char*,uint32_t n) { if(fail) return 0; counter=n; return sizeof(n); }
  bool isKey(const char* key) { return blobs.count(path(key))!=0; }
  size_t getBytesLength(const char* key) { auto i=blobs.find(path(key)); return i==blobs.end()?0:i->second.size(); }
  size_t getBytes(const char* key,void* out,size_t capacity) {
    auto i=blobs.find(path(key));
    if(failRead||i==blobs.end()||i->second.size()>capacity) return 0;
    memcpy(out,i->second.data(),i->second.size()); return i->second.size();
  }
  size_t putBytes(const char* key,const void* data,size_t size) {
    if(failWrite) return 0;
    ++blobWrites; blobs[path(key)].assign((const uint8_t*)data,(const uint8_t*)data+size);
    if(corruptWrite&&size) blobs[path(key)][0]^=1;
    return size;
  }
};
