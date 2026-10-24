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

DeviceCoreConfig coreConfig{
  "my-device",
  WIFI_PRIMARY_SSID, WIFI_PRIMARY_PASSWORD,
  WIFI_SECONDARY_SSID, WIFI_SECONDARY_PASSWORD,
  ADMIN_USER, ADMIN_PASSWORD, SETUP_AP_PASSWORD,
  15000
};

ESPDeviceCore deviceCore(coreConfig);

String applicationStatus() {
  return String("active_outputs=") + activeOutputCount();
}

void setup() {
  // Initialize application logic and messaging integrations here.
  // Register application routes on deviceCore.server() before begin().
  deviceCore.setStatusProvider(applicationStatus);
  deviceCore.begin();
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
  `http://192.168.4.1/`.

Keep messaging tokens in a local `secrets.h` excluded from Git. Do not commit
application credentials to a public repository.

Migrate and test one board at a time. Compile the target firmware and verify
the application routes, administration routes, OTA update, and device logic
before deployment.
