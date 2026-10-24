# Review Request for GPT-6.1

## Objective

Build one reusable core for ESP8266/ESP32 firmware providing:

- primary and secondary Wi-Fi connection;
- persistent configuration;
- DHCP or static IP support;
- web interface;
- administration;
- authenticated OTA firmware updates;
- restart and diagnostics;
- Telegram integration without publishing tokens;
- preservation of all existing application features.

Each firmware/device must have **one web server and one port 80**. The four
firmwares do not share one physical server: each board keeps its own server on
its own IP. The common core must be integrated into that firmware's existing
application server and must not start a second server on port 8080 or any other
port.

## Repository and Git status

- Public repository: https://github.com/fedrux/esp-device-core
- Current merged branch: `main`
- Pull request: https://github.com/fedrux/esp-device-core/pull/1
- Initial commit: `4e089c1`
- Initial CI fix: `70da55b`
- Platform implementation: `8a460f9`
- Single-server correction: `7876ab7`
- Documentation clarification: `a4e8743`
- Merge commit on `main`: `8362739`

## Files changed or added

- `src/ESPDeviceCore.h`
- `src/ESPDeviceCore.cpp`
- `examples/BasicDevice/BasicDevice.ino`
- `examples/LegacyIntegration/README.md`
- `README.md`
- `.github/workflows/compile-example.yml`
- `REVIEW-GPT6.1.md`

## What the core implements

### Wi-Fi

- tries the primary network;
- falls back to the secondary network;
- starts `ESP-Setup-XXXX` access point mode if both networks fail;
- stores configuration persistently in EEPROM;
- supports DHCP;
- supports static IP, gateway, subnet, and DNS;
- provides a web form for updating the configuration.

### Web and security

- `ESP8266WebServer` on ESP8266;
- `WebServer` on ESP32;
- Basic Authentication for administration routes;
- a per-boot CSRF token for configuration, restart, and OTA POST requests;
- authenticated `.bin` firmware upload;
- JSON status endpoint;
- administration on the same server and port 80.

Administration routes:

- `/admin`
- `/admin/api/status`
- `/admin/api/config`
- `/admin/api/restart`
- `/admin/api/update`

### Telegram

The core does not contain a Telegram bot or Telegram tokens. It exposes
reusable status and restart hooks, leaving project-specific commands in each
application. Tokens must stay in a local `secrets.h`, excluded from Git.

### CI and compilation

GitHub Actions compiles the example for:

- `esp8266:esp8266:nodemcuv2`;
- `esp32:esp32:esp32`.

Successful local builds were completed for:

- ESP8266 NodeMCU;
- generic ESP32.

A previous local error was caused by parallel Arduino builds sharing one
temporary directory, not by a source-code failure.

## Four firmware families to preserve

| Project | Planned IP | Main responsibilities |
|---|---:|---|
| Pylontech | `192.168.1.109` | NodeMCU monitor and Pylontech parser |
| Spruzzini | `192.168.1.110` | Eight-output controller and Telegram bot |
| Energia | `192.168.1.111` | LED/OLED variants and server requests |
| Spruzzini orto | `192.168.1.112` | Garden controller variant and Telegram bot |

Planned gateway/DNS: `192.168.1.254`.

## Application features that must not be lost

### Spruzzini

- output control;
- timed irrigation cycles;
- existing Telegram commands;
- existing web page;
- output status;
- physical button;
- NTP;
- application messages and logs.

The current firmware uses a manual `WiFiServer` on port 80 and functions such
as `checkForClient`, `triggerPin`, `cicloSpruzzini`, and `writer`.

### Spruzzini orto

- garden-specific Telegram commands;
- dedicated pin control;
- existing web page;
- NTP and application logs.

This firmware also uses a manual server and must not be replaced blindly.

### Energia

- LED variant;
- ESP32-C3 OLED variant;
- requests to the primary and fallback servers;
- battery, grid, inverter, and Tuya status display;
- static IP and fallback logic.

### Pylontech

- Pylontech serial parser;
- existing web page and APIs;
- existing authentication and OTA;
- battery monitoring and protocol completion confirmation.

The previously diagnosed Pylontech issue was in the RS232 return path: only the
`pwr` echo was received, without the data frame and without
`Command completed successfully`.

## Critical architecture point

The first proposal used the core on port `8080` while leaving the application
server on port 80. That was incorrect because the required architecture is
**one core/server inside each firmware**.

The correction `7876ab7` returned the core to port 80 and updated the
architecture documentation. The actual migration of the Spruzzini legacy
firmware is still not complete:

- the old sketch still uses manual `WiFiServer` HTTP parsing;
- the core uses `ESP8266WebServer`;
- both cannot simply be instantiated together;
- the Spruzzini server must be migrated to one `ESP8266WebServer`;
- application routes and `/admin/*` routes must be registered on that same
  server object;
- Telegram, pins, cycles, and the existing page must remain functional;
- the migrated firmware must be compiled and tested before upload.

The same strategy must then be applied to the other three firmware families,
with project-specific adapters where their existing server implementations
differ.

## Requested technical review

GPT-6.1 should specifically check:

1. Whether `ESPDeviceCore` can share one `WebServer` with application routes
   without route collisions.
2. Whether route registration order is correct.
3. Whether the core `/` route can overwrite an application home page.
4. Whether AP setup mode remains reachable through `/admin`.
5. Whether EEPROM persistence is compatible with ESP8266 and ESP32.
6. Whether string handling and pointers avoid leaks and heap fragmentation on
   ESP8266.
7. Whether IP validation is complete.
8. Whether local CSRF protection is adequate for a device administration page.
9. Whether OTA handles partial failures and invalid file types correctly.
10. Whether Basic Authentication and example passwords are safe.
11. Whether the core preserves the existing Telegram bots.
12. Whether migrating manual `WiFiServer` parsing preserves all Spruzzini
    features.
13. Whether explicit APIs are needed to register application routes before or
    after `begin()`.
14. Whether the core `/` route should be optional or conditionally registered.
15. Whether ESP32 should use `Preferences` or LittleFS instead of EEPROM.
16. Whether CI should include ESP32-C3 in addition to generic ESP32.
17. Whether a real NAT repeater should remain outside the core.

## Constraints

- never publish Wi-Fi passwords;
- never publish admin passwords;
- never publish Telegram tokens;
- never modify production credentials;
- never delete or overwrite original firmware archives;
- never upload firmware without identifying the correct board and port;
- never claim the four firmware migrations are complete until each one is
  compiled and tested individually;
- keep one server and one port 80 per firmware;
- preserve all existing application features.

## Honest current status

The public core has been expanded, compiled, documented, and merged into
`main`. The complete multi-firmware platform is not yet ready for physical
deployment. The remaining work is the actual migration of the four legacy
sketches to one server per firmware and functional verification of each board.
