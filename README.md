# ESP Device Core

Reusable Arduino infrastructure for ESP8266, ESP32 and ESP32-C3 devices.
Version 1.1 adds recovery during operation, reversible network changes, an
Italian mobile administration page, diagnostics and identity-checked OTA.

## What it does

- Tries primary and secondary Wi-Fi without waiting inside `begin()` or `loop()`.
- Reconnects after disconnection and keeps a working secondary network connected.
- Starts password-protected `ESP-Setup-XXXX` recovery after both networks fail,
  while continuing station retries. Recovery closes 30 seconds after connection.
- Tries a network change in RAM for 120 seconds. Confirmation through the new
  LAN address saves it; timeout, cancellation or power loss restores saved settings.
- Provides Wi-Fi search, DHCP/static settings, diagnostics, restart and OTA on
  `/admin`, on the application's one web server on port 80.
- Reports core/application versions, uptime, reset cause, memory, disconnections,
  last error, network state and application state independently.
- Offers a separate, read-only PC inventory in `fleet/`. Personal addresses and
  credentials belong in a private configuration outside this repository.

## Quick start

1. Install this library and copy `examples/BasicDevice` into a sketch.
2. Copy `secrets.example.h` to `secrets.h` and replace every example credential.
3. Set a unique application identity with `ESP_DEVICE_FIRMWARE` and reference it
   through `config.firmware`, as shown in the example.
4. Compile for the actual board and perform the first upload over USB.
5. Open `http://DEVICE-IP/admin`. In recovery, connect to `ESP-Setup-XXXX` and
   open `http://192.168.4.1/admin` using the administrator credentials.

The recovery password must have 8-63 characters. The core does not own `/` or
an application's not-found handler. The basic example redirects `/` to `/admin`;
other applications can keep their existing homepage.

## Network changes

The form is populated with current network settings, without sending saved
passwords to the browser. A blank password keeps the password only if the SSID
is unchanged; changing SSID clears the previous password. Explicit checkboxes
allow an open network. SSIDs are limited to 32 bytes and passwords to 63 bytes.

Choose **Prova nuova configurazione**, then open the new device address and
choose **Conferma questa connessione** within 120 seconds. Confirmation from
only the recovery AP is rejected: the new LAN address must be reachable. If a
DHCP change makes the address unknown, connect to the recovery AP and inspect
its `/admin` page for the new address. Reconnect to the LAN to confirm.

Timeout or **Ripristina la rete precedente** restores the previous configuration.
A reboot during a trial also restores saved settings. There is no automatic
reboot for a network change. Static IP validation checks address syntax, subnet
mask, network/broadcast addresses and gateway membership in the same subnet.

Network settings use the Arduino EEPROM backend. The core reserves bytes
0-1023. Valid v1 settings at offset zero are read automatically; a new versioned
record includes a CRC and bounded strings. Applications already using EEPROM
must resolve ownership before integration. This is not a guarantee against all
flash failures or power loss during an actual commit.

## OTA packages

OTA now accepts **.edc**, not a raw `.bin`. Existing v1 admin pages cannot upload
this format; migration to this version uses USB or the old version's own updater.
Each build embeds board, application and firmware version:

```cpp
ESP_DEVICE_FIRMWARE(firmwareIdentity, "my-application", "1.2.0");
// Set config.firmware = &firmwareIdentity before constructing ESPDeviceCore.
```

After compiling an application image:

```sh
python tools/package_firmware.py path/to/Sketch.ino.bin
python tools/package_firmware.py path/to/Sketch.ino.edc --inspect
```

Use the application image, not a merged flash image, bootloader or filesystem.
The packager extracts the identity from the binary and checks ESP32 chip IDs.
Supported package targets: ESP8266, ESP32, ESP32-C3, ESP32-S2/S3/C6. Compilation
is verified by CI for ESP8266, ESP32 and ESP32-C3.

The device checks header CRC, target and application before starting an update,
then exact payload length and MD5 before activation. Rejected, incomplete,
interrupted and oversized uploads do not schedule a reboot. Uploads require
Basic Authentication and an `X-CSRF-Token` header before any firmware write.
MD5 protects integrity; packages are not cryptographically signed and the
identity is not proof of authorship. Use trusted builds on a trusted LAN.

The page shows transmission progress, waits for a new boot session and checks
the installed version before reporting success. If the board does not return,
it reports that verification is incomplete. Physical OTA and power-loss recovery
still need testing on each target board and partition layout.

## Administration API

All routes below require Basic Authentication. POSTs require the per-boot
CSRF token from authenticated `GET /admin/api/status`, in `X-CSRF-Token`
(or `csrf_token` for non-upload form requests).

| Route | Method | Purpose |
|---|---|---|
| `/admin` | GET | Italian administration page |
| `/admin/api/status` | GET | Diagnostics and password-free settings |
| `/admin/api/config` | POST | Start reversible network trial |
| `/admin/api/config/confirm` | POST | Save trial from the new LAN address |
| `/admin/api/config/cancel` | POST | Restore previous settings |
| `/admin/api/scan` | POST | Start/poll asynchronous Wi-Fi scan (202 while busy) |
| `/admin/api/restart` | POST | Schedule restart after replying |
| `/admin/api/update` | POST | Multipart `.edc` upload, field `firmware` |

Application state is supplied by `setStatusProvider()`. The core services only
its own infrastructure. Application sensors, actuators, schedules and messaging
remain in the application. Keep `deviceCore.loop()` running frequently; synchronous
web uploads are maintenance operations and can delay application work.

## Private device inventory

```sh
python fleet/server.py --config /private/path/devices.json --port 8765
```

Copy `fleet/devices.example.json` outside the public repository. Open
`http://127.0.0.1:8765/`. The service binds only to loopback, performs read-only
LAN requests and preserves last successful contact/version in a local state file.
No Internet service, telemetry or cloud account is required. It does not start
automatically after a PC reboot.

Adapters: `core` reads `/admin/api/status`, `json` reads `/api/status`, and `http`
checks a web server's `/`. A null URL means unmonitored, not offline. For a JSON
adapter, optional `expected` fields check identity and `data_current_field`
separates fresh sensor data from HTTP reachability. HTTP 401/403 on an `http` adapter
means a reachable protected server, not a sensor health check.

Core adapter credentials come from environment variables named by `user_env`
and `password_env`. They stay in the PC process; neither passwords nor CSRF tokens
are forwarded to the browser. Do not include personal inventory or credentials
in the public core repository. The dashboard is for use on the same PC; LAN or
remote access would need a separately designed authentication/deployment setup.

An optional second, read-only telemetry request can use `telemetry_path` set to
`/api/status`, with an explicit `telemetry_keys` allowlist and a boolean
`telemetry_current_field`. The viewer forwards only those configured keys.
Energy telemetry using `pv_w`, `load_w`, `battery_percent` and `fresh` enables
the production/consumption view; stale values are displayed as unavailable.

## Validation

```sh
python -m unittest discover -s tests -v
python tests/run_native.py  # g++ with UndefinedBehaviorSanitizer
```

Native tests exercise Wi-Fi failover/reconnect, AP recovery, timeout rollback,
LAN confirmation, persistence, invalid settings, protected operations and OTA
rejection/abort/activation using fake hardware. Tool tests cover package integrity,
embedded identity, inventory freshness, last contact and credential filtering.
They complement compilation and do not replace physical radio/flash tests.
`tests/admin_preview.py` is an explicit local UI fixture, never device firmware.

See [ARCHITECTURE-REVIEW.md](ARCHITECTURE-REVIEW.md) and
[Legacy integration](examples/LegacyIntegration/README.md).

## Security and scope

Keep `secrets.h` out of Git. Basic Authentication runs over local HTTP: do not
expose administration or the recovery AP to the Internet. No provider-specific
bot, token or API key is shipped. A recovery AP is not a NAT repeater.

MIT. See [LICENSE](LICENSE).

After editing `src/DeviceAdminPage.h`, regenerate its compressed flash copy with
`python tools/compress_admin.py`. A regression test verifies the two copies match.
