#pragma once
#include <string>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <type_traits>
class String {
  std::string s;
public:
  String()=default;
  String(const char* p):s(p?p:""){}
  String(const std::string& p):s(p){}
  template<class T, typename std::enable_if<std::is_integral<T>::value,int>::type=0> String(T n):s(std::to_string(n)){}
  String(double n,int decimals) { char b[100]; std::snprintf(b,sizeof(b),"%.*f",decimals,n); s=b; }
  const char* c_str() const { return s.c_str(); }
  size_t length() const { return s.size(); }
  char operator[](size_t i) const { return s[i]; }
  String& operator+=(const String& b) { s+=b.s; return *this; }
  String& operator+=(char c) { s+=c; return *this; }
  friend String operator+(const String& a,const String& b) { return String(a.s+b.s); }
};
inline void yield() {}
inline uint32_t fakeMillis=0;
inline uint32_t millis() { return fakeMillis; }
struct FakeSerial { template<class... T> void printf(const char*,T...) {} };
inline FakeSerial Serial;
