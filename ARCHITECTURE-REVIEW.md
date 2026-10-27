# Architecture Review

## Purpose

This repository is a public, generic starting point for ESP8266 and ESP32
projects. It must remain independent from any private application firmware,
private device names, private network addresses, and application-specific
repositories.

The core provides:

- primary and secondary Wi-Fi;
- persistent configuration;
- DHCP or static IP settings;
- a local setup access point;
- a protected web administration page;
- authenticated OTA updates;
- restart and JSON diagnostics;
- extension points for application status and messaging integrations.

## Architecture

Each physical device has its own server on port 80. The reusable core is
embedded into the application firmware and shares that firmware's one web
server. The core must not create a second server on another port.

The application remains responsible for its own:

- routes and user interface;
- sensors and actuators;
- scheduling and device behavior;
- optional messaging provider;
- provider credentials.

The core owns only reusable device infrastructure and `/admin/*` routes.

## Review checklist

1. Verify that application routes and core routes can coexist on one server.
2. Ensure the core does not unexpectedly replace an application's `/` route.
3. Verify setup mode and `/admin` are reachable from the local access point.
4. Check EEPROM persistence on ESP8266 and ESP32.
5. Check heap usage, string ownership, and reboot behavior on constrained
   devices.
6. Validate static IP, gateway, subnet, and DNS input.
7. Review CSRF protection for local write operations.
8. Review Basic Authentication and example credentials.
9. Verify OTA error handling and invalid-file handling.
10. Verify that optional messaging integrations remain outside the core.
11. Consider whether ESP32 should use `Preferences` or another native storage
    abstraction in a future release.
12. Consider adding ESP32-C3 to the continuous integration matrix.
13. Keep NAT repeater functionality outside this generic core unless it becomes
    a separately designed feature.

## Version 1.1 behavior and validation

Network lifecycle is driven from `loop()` without connection waits. Trials stay
in RAM until confirmation through the new LAN address; timeout and cancellation
restore the previous configuration. A new boot always loads committed settings.
The core owns only `/admin` and `/admin/*`; `/` and Serial remain application-owned.

OTA uses an identity-bearing package with header validation before flash writes
and payload size/MD5 validation before activation. It requires an authenticated
CSRF header at upload start. Packages are not signed. Storage reserves bytes
0-1023 and reads bounded, validated legacy records before migrating on save.

The optional PC fleet viewer is generic and read-only. Personal inventory,
environment credentials and state files live outside the public repository.
Device reachability is separate from fresh application data. The viewer binds
to loopback and does not change device configuration.

Automated validation includes:

- portable protocol checks and core lifecycle scenarios with fake hardware,
  compiled with warnings as errors and UndefinedBehaviorSanitizer;
- Python tests for packaging, device identity, stale data, last contact and
  credential filtering;
- CI behavior checks and a third compile target for ESP32-C3.

Physical radio, flash, AP access, recovery after power loss and OTA boot must
still be exercised on each actual target. Compilation and fake-hardware tests
do not constitute a completed hardware rollout.

## Original validation baseline

- local ESP8266 compilation;
- local ESP32 compilation;
- GitHub Actions compilation for ESP8266;
- GitHub Actions compilation for ESP32;
- public example without real credentials;
- `.gitignore` protection for local secrets.

## Scope boundary

This repository is not an application collection. It does not document,
reference, or distribute any private device firmware, private IP plan,
application bot, personal network, or project-specific hardware behavior.

Application authors should copy the generic example, provide local secrets in
an ignored file, and implement their own routes and device logic through the
core's public API.

