#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include <cstdlib>
#include <cmath>

namespace CaptureCSV {
constexpr unsigned Limit = 100;
constexpr unsigned MaxLine = 700;
constexpr const char* Header = "record_id,description,timestamp_ms,gain,integration_ms,led_current_ma,led_mode,auto_smux,autozero,sample_period_ms,adc_full_scale,f1,f2,fz,f3,f4,f5,fy,fxl,f6,f7,f8,nir,vis_tl,vis_br,valid,analog_saturation,digital_saturation,sequence,channel_count,request_id,saturated,vis_tl_1,vis_br_1,vis_tl_2,vis_br_2";
inline uint32_t crcByte(uint32_t crc, uint8_t b) {
  crc ^= b;
  for (int i = 0; i < 8; ++i) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
  return crc;
}
inline bool description(const std::string& s) {
  // Limit UTF-8 bytes, reject controls, overlong encodings and invalid scalars.
  if (s.size() > 80) return false;
  for (size_t i = 0; i < s.size();) {
    uint8_t c = s[i++];
    if (c < 32 || c == 127) return false;
    if (c < 128) continue;
    unsigned n; uint32_t cp, min;
    if (c >= 0xc2 && c <= 0xdf) { n=1; cp=c&31; min=128; }
    else if (c >= 0xe0 && c <= 0xef) { n=2; cp=c&15; min=2048; }
    else if (c >= 0xf0 && c <= 0xf4) { n=3; cp=c&7; min=65536; }
    else return false;
    if (i+n > s.size()) return false;
    while (n--) { uint8_t b=s[i++]; if ((b&0xc0)!=0x80) return false; cp=(cp<<6)|(b&63); }
    if (cp<min || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return false;
  }
  return true;
}
inline std::string quote(const std::string& s) {
  std::string out="\"";
  for (char c:s) { out+=c; if(c=='"') out+='"'; }
  return out+'"';
}
inline bool split(const std::string& s, std::vector<std::string>& fields) {
  fields.clear(); size_t i=0;
  do {
    std::string f;
    if (i<s.size() && s[i]=='"') {
      ++i; bool closed=false;
      while (i<s.size()) { char c=s[i++]; if(c=='"') {
        if(i<s.size() && s[i]=='"') { ++i; f+='"'; } else { closed=true; break; }
      } else f+=c; }
      if(!closed || (i<s.size() && s[i]!=',')) return false;
    } else { while(i<s.size() && s[i]!=',') { if(s[i]=='"') return false; f+=s[i++]; } }
    fields.push_back(f);
    if(i==s.size()) break;
    ++i; if(i==s.size()) { fields.push_back(""); break; }
  } while(i<=s.size());
  return true;
}
inline bool integer(const std::string& s, uint32_t& n) {
  if(s.empty() || s.size()>10) return false;
  uint64_t x=0; for(char c:s) { if(c<'0'||c>'9') return false; x=x*10+c-'0'; }
  if(x>0xffffffffu) return false; n=uint32_t(x); return true;
}
inline bool validate(const std::string& line, uint32_t& id, uint32_t& request) {
  if(line.size()>MaxLine) return false;
  std::vector<std::string> f;
  if(!split(line,f) || f.size()!=36 || !description(f[1])) return false;
  if(!integer(f[0],id)||!id||!integer(f[30],request)||!request) return false;
  for(unsigned i=2;i<f.size();++i) {
    if(i==6) { if(f[i]!="OFF"&&f[i]!="ON"&&f[i]!="AUTO") return false; continue; }
    if(i==26||i==27) { if(f[i]!="unknown") return false; continue; }
    if(i==3||i==4) {
      char* end=nullptr; double v=std::strtod(f[i].c_str(),&end);
      if(f[i].empty()||!end||*end||!std::isfinite(v)||v<=0) return false; continue;
    }
    if((i>=11&&i<=24)||(i>=32)) { if(f[i].empty()) continue; }
    uint32_t v; if(!integer(f[i],v)) return false;
    if((i==10||(i>=11&&i<=24)||i>=32)&&v>65535) return false;
    if((i==25||i==31)&&v>1) return false;
    if(i==29&&v!=6&&v!=12&&v!=18) return false;
  }
  const char* gains[]={"0.5","1.0","2.0","4.0","8.0","16.0","32.0","64.0","128.0","256.0","512.0","1024.0","2048.0"};
  bool knownGain=false; for(auto gain:gains) if(f[3]==gain) knownGain=true;
  if(!knownGain || f[25]!="1" || f[10]=="0" || f[7]!=f[29] || (f[8]!="0"&&f[8]!="1"&&f[8]!="2")) return false;
  uint32_t led,period,sequence; integer(f[5],led); integer(f[9],period); integer(f[28],sequence);
  if(led<4||led>258||led%2||!period||period>0x7fffffff||!sequence) return false;
  const unsigned order[]={12,6,0,7,8,15,1,2,9,13,14,3,4,5,10,11,16,17};
  unsigned count=unsigned(std::strtoul(f[29].c_str(),nullptr,10));
  for(unsigned i=0;i<18;++i) {
    unsigned col=i<14?11+i:32+i-14;
    if(f[col].empty() != (order[i]>=count)) return false;
  }
  return true;
}
}
