#include <ESPDeviceCore.h>
#include "secrets.h"

DeviceCoreConfig config{
  "basic-device",
  WIFI_PRIMARY_SSID, WIFI_PRIMARY_PASSWORD,
  WIFI_SECONDARY_SSID, WIFI_SECONDARY_PASSWORD,
  ADMIN_USER, ADMIN_PASSWORD, SETUP_AP_PASSWORD,
  15000, 8080
};

ESPDeviceCore deviceCore(config);

String applicationStatus() {
  return "Applicazione pronta";
}

void setup() {
  deviceCore.setStatusProvider(applicationStatus);
  deviceCore.begin();
}

void loop() {
  deviceCore.loop();
}
