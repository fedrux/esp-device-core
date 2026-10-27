#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
using std::min;
using std::max;
#define PROGMEM
class String {
 public:
  std::string value;
  String() = default;
  String(const char* s) : value(s ? s : "") {}
  String(const std::string& s) : value(s) {}
  String(char c) : value(1, c) {}
  template<class T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0>
  String(T n) : value(std::to_string(n)) {}
  const char* c_str() const { return value.c_str(); }
  size_t length() const { return value.size(); }
  bool isEmpty() const { return value.empty(); }
  char operator[](size_t i) const { return value[i]; }
  void reserve(size_t n) { value.reserve(n); }
  String substring(size_t start) const { return value.substr(start); }
  bool endsWith(const char* end) const { std::string suffix(end); return value.size() >= suffix.size() && value.compare(value.size()-suffix.size(),suffix.size(),suffix)==0; }
  void replace(const char* oldValue, const char* replacement) { size_t p=0; while((p=value.find(oldValue,p))!=std::string::npos) { value.replace(p,strlen(oldValue),replacement);p+=strlen(replacement); } }
  String& operator+=(const String& other) { value += other.value; return *this; }
  friend String operator+(String a, const String& b) { a += b; return a; }
  friend bool operator==(const String& a,const String& b) { return a.value==b.value; }
  friend bool operator!=(const String& a,const String& b) { return !(a==b); }
};
inline size_t strlcpy(char* target, const char* source, size_t size) { const size_t n=strlen(source);if(size){memcpy(target,source,std::min(n,size-1));target[std::min(n,size-1)]=0;}return n; }
extern uint32_t fakeMillis;
inline uint32_t millis() { return fakeMillis; }
struct FakeESP {
  bool restarted=false;
  void restart() { restarted=true; }
  String getResetReason() { return "Test boot"; }
  uint32_t getFreeHeap() { return 40000; }
};
extern FakeESP ESP;
class IPAddress {
 public:
  uint8_t octets[4]{};
  IPAddress() = default;
  explicit IPAddress(uint32_t) {}
  explicit IPAddress(const uint8_t* ip) { memcpy(octets,ip,4); }
  IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d) : octets{a,b,c,d} {}
  String toString() const { return String(octets[0])+"."+String(octets[1])+"."+String(octets[2])+"."+String(octets[3]); }
  friend bool operator==(const IPAddress& a,const IPAddress& b) { return !memcmp(a.octets,b.octets,4); }
  friend bool operator!=(const IPAddress& a,const IPAddress& b) { return !(a==b); }
};
