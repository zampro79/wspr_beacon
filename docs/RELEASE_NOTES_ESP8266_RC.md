# WSPR Beacon 2.0 — ESP8266 release candidate notes

Proposed package identifier: `2.0.0-rc.1`. This is a provisional release
candidate designation, not a stable version or Git tag. The embedded firmware
version remains `dev` until a separately approved version change.

## Target and profile

- Official RC target: NodeMCU-compatible ESP8266 only.
- ESP32 is experimental and excluded from this package.
- The PUBLIC profile starts with an unconfigured callsign and locator `JN45`.
- A valid SAVED configuration continues to override profile defaults.
- TX remains inhibited until a valid operating identity and every independent
  time/RF safety condition are satisfied.

## Included behavior

- GPS/PPS is the preferred time authority; synchronized NTP authorization is
  available as an explicitly experimental fallback.
- Dashboard, Configuration and serial CLI expose configuration and diagnostics.
- Versioned WSPR and Wi-Fi persistence, transactional Factory Reset and manual
  TX permission controls are included.
- Seven WSPR bands and FIXED/RANDOM software selection are implemented.

## Validation and limitations

NodeMCU development firmware has been built, installed and exercised through
the documented provisioning, web, console and frame-completion paths. Firmware
frame completion is not instrumented proof of emitted RF quality. Frequency
accuracy, output power, harmonics, filtering, startup transients and full
multiband RF performance are not completely validated. ESP32 hardware is not
validated for this RC.

The original modern project material is GPL-3.0-or-later. Third-party notices
and license texts accompany the package. LGPL source/relink retention and
ESP8266 NONOS SDK compatibility remain open; GCC runtime exception evidence is
now included. This candidate is
`NOT_APPROVED_FOR_PUBLIC_DISTRIBUTION`.
