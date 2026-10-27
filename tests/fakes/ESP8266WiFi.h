#pragma once
#include "Arduino.h"
enum { WIFI_STA, WIFI_AP_STA, WL_DISCONNECTED=0, WL_CONNECTED=3 };
struct FakeWiFi {
  int connection=WL_DISCONNECTED;
  String lastSsid,lastPassword; unsigned begins=0;bool ap=false;
  void persistent(bool) {} void mode(int) {} void setAutoReconnect(bool) {}
  void disconnect() { connection=WL_DISCONNECTED; }
  bool config(IPAddress,IPAddress,IPAddress) { return true; }
  bool config(IPAddress,IPAddress,IPAddress,IPAddress) { return true; }
  void begin(const char* ssid,const char* password) { lastSsid=ssid;lastPassword=password;++begins; }
  int status() { return connection; }
  String SSID() { return lastSsid; }
  String SSID(int) { return "Detected"; }
  int RSSI() { return -52; } int RSSI(int) { return -50; }
  String macAddress() { return "00:11:22:33:44:55"; }
  bool softAP(const char*,const char*) { ap=true;return true; }
  void softAPdisconnect(bool) { ap=false; }
  IPAddress localIP() { return IPAddress(192,168,10,20); }
  IPAddress softAPIP() { return IPAddress(192,168,4,1); }
  int scanComplete() { return 0; } void scanNetworks(bool) {} void scanDelete() {}
};
extern FakeWiFi WiFi;
