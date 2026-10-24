#pragma once

#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
using DeviceWebServer = ESP8266WebServer;
#else
#include <WebServer.h>
#include <WiFi.h>
using DeviceWebServer = WebServer;
#endif

#if defined(ESP8266)
#include <Updater.h>
#else
#include <Update.h>
#endif

struct DeviceCoreConfig {
  const char* deviceName;
  const char* primarySsid;
  const char* primaryPassword;
  const char* secondarySsid;
  const char* secondaryPassword;
  const char* adminUser;
  const char* adminPassword;
  const char* apPassword;
  uint32_t wifiTimeoutMs = 15000;
};

class ESPDeviceCore {
 public:
  explicit ESPDeviceCore(const DeviceCoreConfig& config);
  void begin();
  void loop();
  void setStatusProvider(String (*provider)());
  String statusText() const;
  bool restart();
  bool setupMode() const;
  DeviceWebServer& server();

 private:
  DeviceCoreConfig config_;
  DeviceWebServer server_{80};
  String status_;
  bool setupMode_ = false;
  bool authorized();
  void connectWifi();
  void startSetupAp();
  void registerRoutes();
  void sendAdminPage();
  void sendStatus();
  void handleConfig();
  void handleUpdate();
  void handleUpload();
  static String (*statusProvider_)();
};
