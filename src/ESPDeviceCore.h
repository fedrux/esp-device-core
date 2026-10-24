#pragma once

#include <Arduino.h>
#include <EEPROM.h>

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
  uint16_t adminPort = 8080;
  bool useDhcp = true;
  uint8_t staticIp[4] = {0, 0, 0, 0};
  uint8_t gateway[4] = {0, 0, 0, 0};
  uint8_t subnet[4] = {255, 255, 255, 0};
  uint8_t dns[4] = {0, 0, 0, 0};
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
  static constexpr uint32_t kStorageMagic = 0x45535044;
  static constexpr size_t kStorageSize = 512;
  struct StoredConfig {
    uint32_t magic;
    char primarySsid[64];
    char primaryPassword[64];
    char secondarySsid[64];
    char secondaryPassword[64];
    uint8_t useDhcp;
    uint8_t staticIp[4];
    uint8_t gateway[4];
    uint8_t subnet[4];
    uint8_t dns[4];
  };
  DeviceCoreConfig config_;
  DeviceWebServer server_;
  String status_;
  String csrfToken_;
  StoredConfig storedConfig_{};
  bool setupMode_ = false;
  bool authorized();
  bool validCsrf();
  bool loadStoredConfig();
  bool saveStoredConfig();
  void clearStoredConfig();
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
