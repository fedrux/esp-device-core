#pragma once
#include "Arduino.h"
struct FakeUpdate {
  unsigned starts=0,activations=0;bool running=false;String md5;
  bool begin(size_t) { ++starts;running=true;return true; }
  bool setMD5(const char* value) { md5=value;return true; }
  bool end(bool) { bool ok=running&&md5!="00000000000000000000000000000000";if(ok)++activations;running=false;return ok; }
  size_t write(uint8_t*,size_t n) { return n; }
  String getErrorString() { return "Test updater"; }
};
extern FakeUpdate Update;
