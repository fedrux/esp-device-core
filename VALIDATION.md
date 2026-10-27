# Validation — 2026-10-25

## Software checks completed

- Example compilation: ESP8266 NodeMCU (core 3.1.2), ESP32 (core 3.3.12),
  ESP32-C3 with USB CDC enabled (core 3.3.12).
- 11 Python tests: embedded OTA identity, integrity, truncation, chip mismatch,
  inventory identity, last contact, data freshness, protected HTTP reachability
  and removal of credentials/CSRF tokens from viewer responses.
- Native C++ protocol and lifecycle tests compiled on Linux with `-Wall -Wextra
  -Werror` and UndefinedBehaviorSanitizer. Scenarios cover fallback, reconnect,
  AP recovery, trial timeout and cancellation, confirmation from the LAN,
  persistence, reboot during trial, password preservation, JSON escaping,
  invalid input, authentication and OTA rejection, interruption and activation.
- A package generated from each of the three actual compiled example images
  passed the Python package inspector, including embedded board/app/version.
- Browser checks against the explicitly simulated local admin fixture: current
  form values, Wi-Fi search, trial confirmation/cancellation, restart verification,
  successful upload with version verification and incompatible-package rejection.
- Inventory browser checks: filtering, recovery from a failed status request,
  freshness labels, desktop/mobile layout at 390px, no horizontal overflow or
  console errors on the actual inventory page.
- `git diff --check` passed. CI was extended with behavior tests and ESP32-C3;
  no remote CI run is claimed by this local report.

## Remaining hardware validation

No physical board was identified on USB during this work. These results do not
claim installation on existing application firmware. Actual radio switching,
AP access, flash/OTA boot and power interruption must be tested on each board.
The BasicDevice example must not replace an existing device's application.

For an existing application, integrate on its one server, preserve its UART,
peripheral logic and EEPROM ownership, compile its full firmware, then install
and verify it on that particular device. Use that application's established
deployment and synchronization rules. The generic core contains no personal
inventory, credentials or environment deployment receipt.

## Follow-up — 2026-10-26

ESP32-C3 was installed and exercised with a private application, preserving its
OLED pages and adding its missing production/consumption rotation during warnings.
Core 1.1.1 serves its administration page as gzip (12,913 -> 4,817 bytes), verified
on the physical board along with authenticated diagnostics and live telemetry.
The complete application compiles for ESP32-C3; the updated core also compiles
for ESP8266 and ESP32. Fourteen Python tests and native UBSan scenarios passed.
The local fleet viewer now supports allowlisted read-only application telemetry.
This report does not claim physical OTA or Wi-Fi trial tests on all board types.
