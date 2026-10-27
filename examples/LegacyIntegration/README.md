# Integrating an Existing Application

Integrate the core into the same web server as an existing application. Each
device has its own server on port 80; the core and routes are not shared
between devices over the network. An application may already have:

- application web pages on port 80;
- a messaging integration with application-specific commands;
- sensor or actuator logic.

Do not create a second `WiFiServer` or `WebServer` inside the same firmware.
Use one `ESP8266WebServer` or `WebServer` and register application routes on
the server returned by `deviceCore.server()`. The `/admin/*` routes are reserved
for the core:

```cpp
#include <ESPDeviceCore.h>

ESP_DEVICE_FIRMWARE(firmwareIdentity, "my-application", "1.1.0");

DeviceCoreConfig makeCoreConfig() {
  DeviceCoreConfig coreConfig{
  "my-device",
  WIFI_PRIMARY_SSID, WIFI_PRIMARY_PASSWORD,
  WIFI_SECONDARY_SSID, WIFI_SECONDARY_PASSWORD,
  ADMIN_USER, ADMIN_PASSWORD, SETUP_AP_PASSWORD,
  15000
  };
  coreConfig.firmware = &firmwareIdentity;
  return coreConfig;
}

ESPDeviceCore deviceCore(makeCoreConfig());

String applicationStatus() {
  return String("active_outputs=") + activeOutputCount();
}

void setup() {
  // Initialize application logic and messaging integrations here.
  // Register application routes on deviceCore.server() before begin().
  deviceCore.setStatusProvider(applicationStatus);
  deviceCore.begin(); // Returns before Wi-Fi is connected.
}

void loop() {
  // Keep the application loop; the core services the same server.
  deviceCore.loop();
  processApplicationMessages();
  updateApplicationOutputs();
}
```

After upload:

- application pages and administration: `http://DEVICE-IP/`;
- administration, Wi-Fi configuration, and OTA:
  `http://DEVICE-IP/admin`;
- recovery mode: connect to `ESP-Setup-XXXX` and open
  `http://192.168.4.1/admin`.

The core never registers `/` or a not-found handler. Keep the application's
homepage and include a link to `/admin`. Registering core routes does not replace
those application handlers. Only the core should call `handleClient()` on this
shared server during normal operation.

`begin()` and Wi-Fi recovery are nonblocking. Application network requests must
wait for `WiFi.status() == WL_CONNECTED`; sensors and local outputs can keep
running while connection attempts continue. The core does not initialize Serial,
so an existing UART protocol is preserved. It reserves EEPROM bytes 0-1023;
resolve any application EEPROM conflict before migration.

Use `.edc` packages for OTA and keep the application ID fixed between versions.
Change the firmware version for every release. OTA is a maintenance operation:
the synchronous upload handler can delay the application's loop. Ensure outputs
and peripherals tolerate maintenance before allowing an update.

Keep messaging tokens in a local `secrets.h` excluded from Git. Do not commit
application credentials to a public repository.

Migrate and test one board at a time. Compile the target firmware and verify
the application routes, administration routes, OTA update, and device logic
before deployment.
