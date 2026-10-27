#pragma once
#include "Arduino.h"
struct FakeEEPROM {
  std::vector<uint8_t> bytes = std::vector<uint8_t>(1024,255);
  unsigned commits=0; bool commitOk=true;
  void begin(size_t) {}
  size_t length() { return bytes.size(); }
  template<class T> void get(size_t offset,T& value) { memcpy(&value,bytes.data()+offset,sizeof(T)); }
  template<class T> void put(size_t offset,const T& value) { memcpy(bytes.data()+offset,&value,sizeof(T)); }
  bool commit() { ++commits;return commitOk; }
};
extern FakeEEPROM EEPROM;
