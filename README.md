# ESP Device Core

A small, reusable Arduino core for ESP8266 and ESP32 devices.

## Features

- primary and secondary Wi-Fi networks;
- DHCP or static IP configuration;
- local setup access point when configured networks are unavailable;
- persistent Wi-Fi settings;
- Basic Authentication for administration;
- authenticated OTA firmware updates;
- CSRF protection for local write operations;
- restart and JSON diagnostics;
- hooks for application status and optional messaging integrations.

## Quick start

1. Copy `examples/BasicDevice` into a new sketch.
2. Copy `secrets.example.h` to `secrets.h`.
3. Add local credentials to `secrets.h`.
4. Install the `ESP Device Core` library.
5. Compile and upload the first firmware over USB.
6. Open the device IP and visit `/admin`.

If no configured network is available, the device creates an access point named
`ESP-Setup-XXXX`. Connect to it and open `http://192.168.4.1/`. The setup page
can save both networks, select DHCP or static networking, and restart the
device. Once connected to a LAN, administration remains on the same web server
and port 80 at `http://DEVICE-IP/admin`.

## Security

- `secrets.h` is excluded from Git.
- OTA and configuration require Basic Authentication.
- Configuration, restart, and OTA POST requests require a per-boot CSRF token.
- The core contains no Wi-Fi passwords, messaging tokens, or API keys.
- Replace all example credentials before deploying.
- Do not expose the device administration page directly to the Internet.

## Messaging integrations

The core does not ship with a provider-specific bot or token. Applications can
connect their preferred messaging library and use
`deviceCore.statusText()` and `deviceCore.restart()`. This keeps application
commands and credentials outside the reusable core.

## Web server architecture

Each device uses one web server on port 80. Applications that already expose
web pages should register their routes on the same server object as the core;
the core must not start a second server on another port. See
[examples/LegacyIntegration/README.md](examples/LegacyIntegration/README.md).

## Repeater scope

Version 1 is not a NAT repeater. The setup access point is for local recovery
and configuration, not Internet sharing. A full repeater requires separate
chipset, firmware, and network-topology design.

## License

MIT. See [LICENSE](LICENSE).
