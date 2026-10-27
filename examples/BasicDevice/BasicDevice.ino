#include <ESPDeviceCore.h>
#include "secrets.h"

ESP_DEVICE_FIRMWARE(firmwareIdentity, "basic-device", "1.1.0");

DeviceCoreConfig createConfig() {
  DeviceCoreConfig config{
  "basic-device",
  WIFI_PRIMARY_SSID, WIFI_PRIMARY_PASSWORD,
  WIFI_SECONDARY_SSID, WIFI_SECONDARY_PASSWORD,
  ADMIN_USER, ADMIN_PASSWORD, SETUP_AP_PASSWORD
  };
  config.firmware = &firmwareIdentity;
  return config;
}

ESPDeviceCore deviceCore(createConfig());

String applicationStatus() {
  return "Applicazione pronta";
}

void setup() {
  // The application owns /; the core only registers /admin and /admin/*.
  deviceCore.server().on("/", HTTP_GET, [] {
    deviceCore.server().sendHeader("Location", "/admin");
    deviceCore.server().send(302, "text/plain", "Amministrazione: /admin");
  });
  deviceCore.setStatusProvider(applicationStatus);
  deviceCore.begin();
}

void loop() {
  deviceCore.loop();
}
