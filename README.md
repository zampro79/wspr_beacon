# WSPR Beacon 2.0

**Developed by IW2NNV / AC2WZ**

WSPR Beacon 2.0 is experimental firmware for an autonomous WSPR Type 1 beacon
using a GPS receiver, Si5351A synthesizer and optional SSD1306 OLED. It combines
deterministic scheduling, fail-closed RF control, persistent configuration, a
serial console and local web interfaces.

> DEVELOPMENT builds retain the `IW2NNV`/`JN45` operating defaults. PUBLIC
> builds start with an empty callsign and `JN45`, so transmission remains
> inhibited until a valid operating identity is explicitly saved. Public release
> remains blocked by the other release-readiness items documented below.

## Platform status

| Platform | Release status |
| --- | --- |
| NodeMCU Amica V2 / ESP8266-12F | Primary development platform; repeatedly build-, upload- and function-tested; planned target of the first binary release |
| ESP32 Dev Module | Primary development platform; builds and software tests pass; core operation, persistence, HTTP/Web UI, SNTP renewal and natural WSPR frames have partial hardware validation; a live snapshot exceeded three hours, while continuously logged endurance, fault recovery, quantitative RF validation and an official binary remain pending |
| Arduino Nano Every | Historical/frozen; not an official release target |
| Arduino Nano 33 IoT | Unsupported |

The software supports manual selection of 40, 30, 20, 17, 15, 12 and 10 m,
with FIXED or per-frame RANDOM tone-zero frequency selection. Multiband RF
hardware, filtering and spectral performance have not been validated. Software
support is not RF certification or authorization to transmit.
Band selection is available through the CLI and web configuration; changing
band with the physical button is not currently implemented.

## Current capabilities

- WSPR Type 1 encoding and non-blocking 162-symbol transmission;
- GPS/NMEA with optional PPS and experimental NTP TX authorization;
- persistent WSPR v3 (with v1/v2 read compatibility) and Wi-Fi configuration on ESP8266/ESP32;
- AP Provisioning with manual network scan and manual SSID entry;
- AP Operational mode, Dashboard and protected WSPR Configuration page with
  a controlled entry to the existing Wi-Fi AP Provisioning portal;
- ACTIVE, CANDIDATE and SAVED configuration views;
- serial CLI and guided menu, including FIXED/RANDOM selection;
- shared RAM-only manual TX permission and consistent effective-eligibility
  diagnostics;
- transactional Factory Reset for WSPR and Wi-Fi records;
- OLED startup, Hardware Check, operational, PREPARING and TX pages;
- centralized diagnostics and fail-safe Si5351A output control;
- deterministic firmware identity in the startup log and `/api/status`.

TX eligibility always remains subject to a configured operating identity,
qualified time, RF readiness, operating mode, configuration-session state and
the scheduler safety gates. Manual TX ON grants permission only; it never forces
a frame or bypasses those conditions.

## Quick start from source

Install Python and PlatformIO, clone this repository, and build an explicit
environment. See the [Building Guide](docs/BUILDING.md) for prerequisites,
upload, outputs and technical packaging.

```sh
pio run -e esp8266
pio run -e esp32
pio run -e esp8266_public
pio run -e esp32_public
pio test -e native
pio test -e native_public
```

`esp8266` and `esp32` are DEVELOPMENT environments. The `_public` environments
use safe unconfigured operating-identity defaults; they do not erase or override
valid saved settings. An unqualified `pio run` builds ESP8266 DEVELOPMENT as a
build-system default, not as a statement of platform priority. Nano Every remains
historical/frozen.

Every firmware reports the project, development version (`dev`), selected
profile, target and source commit (or `unknown`). After an explicit build,
`tools/stage_firmware.py` can copy its components into a separate directory
using deterministic profile/target/commit filenames and a SHA-256 manifest.
This is development-only preparation: no official firmware package is
published, and DEVELOPMENT artifacts are rejected from public-release staging.

Discover the actual serial port before uploading:

```sh
pio device list
PORT=/path/to/serial-device
pio run -e esp8266 -t upload --upload-port "$PORT"
pio device monitor --port "$PORT" --baud 9600
```

The port is system-specific; do not assume `/dev/ttyUSB0` or a fixed COM number.
Keep the RF path safely isolated during installation and initial verification.
Complete operating-system instructions and provisional precompiled flashing
layouts are in the [Installation Guide](docs/INSTALLATION.md).

Before connecting an antenna:

- confirm that the operator is authorized for the selected amateur band;
- select the intended band and use a suitable low-pass filter;
- begin into a dummy load and measure output power, frequency and harmonics;
- verify the RF-chain impedance match and antenna suitability; and
- confirm that the firmware reports qualified time and no RF inhibit.

Software diagnostics, a decoded WSPR spot and the declared message power do not
replace these checks or constitute RF certification.

Public artifact staging is fail-closed: it performs a clean build from the
verified commit, validates build-generated provenance and embedded firmware
identity, and emits a deterministic manifest plus `SHA256SUMS`. DEVELOPMENT
artifacts are never eligible for public staging. No public download is
currently available; licensing, release approval and hardware installation
validation remain open.

Future extracted PUBLIC packages can be checked without hardware using
`python3 tools/verify_firmware_package.py PACKAGE_DIRECTORY`. The validator
checks schema, identity, target, inventory, offsets, sizes, SHA-256 data and the
ESP32 partition layout, and rejects DEVELOPMENT or unexpected files.

On first boot, a device without usable saved Wi-Fi credentials starts the
password-protected `WSPR-Beacon-<suffix>` AP. Read its random eight-digit
password from OLED or serial, connect to it, and open `http://192.168.4.1/`.
Saving credentials does not reconnect or restart automatically; restart the
device manually to attempt STA operation.

## Documentation

- [User Guide](docs/USER_GUIDE.md) — installation hand-off, operation,
  provisioning, Dashboard, configuration, CLI, OLED and troubleshooting.
- [Installation Guide](docs/INSTALLATION.md) — end-user package verification,
  Linux, macOS and Windows flashing, first boot, updates and recovery.
- [Flashing Guide](docs/FLASHING.md) — future precompiled ESP8266/ESP32
  installation with Espressif tools, factory images and conservative recovery.
- [Building Guide](docs/BUILDING.md) — developer setup, four PlatformIO
  environments, tests, source uploads, outputs and local technical packaging.
- [Hardware Guide](docs/HARDWARE.md) — pin assignments, electrical limits,
  storage and RF precautions.
- [Technical Architecture](PROJECT.md) — state machines, safety, APIs and
  persistence.
- [Task History](TASKS.md) — implementation and verification evidence.
- [Licensing and distribution](docs/SOURCE_DISTRIBUTION.md) — future source
  distribution model and unresolved release obligations; see also the
  [third-party notices](THIRD_PARTY_NOTICES.md).

## Reporting issues

Use the [GitHub issue tracker](https://github.com/zampro79/wspr_beacon/issues)
and include the ESP8266/ESP32 platform, DEVELOPMENT/PUBLIC profile, version or
commit, a concise problem description, reproduction steps and sanitized logs
when relevant. Do not publish Wi-Fi credentials, passwords, tokens, cookies,
unsanitized HAR files, NVS backups, flash or memory dumps, private keys or
unnecessary personal information.

## Verification status

The current consolidated baseline has 559 passing tests in each native profile
and successful ESP8266/ESP32 DEVELOPMENT and PUBLIC builds. NodeMCU testing has
exercised upload, boot, OLED, console,
Wi-Fi/NTP, AP Provisioning, manual scan, AP Operational, persistence recovery
and major web flows. Functional RF observation with an IC-705 was positive.

These observations are not quantitative RF validation. Frequency error,
stability, output power, startup transients, harmonics, filter attenuation and
complete multiband behavior still require documented measurements. ESP32
DEVELOPMENT has partial hardware validation: upload and readback, WSPR v3
persistence, HTTP/Web UI, Firefox IDLE navigation, non-blocking HTTP writes,
periodic SNTP renewal and natural WSPR frames have been exercised. A later live
Dashboard snapshot at 188 minutes 5 seconds uptime showed synchronized NTP,
`NTP_EXPERIMENTAL` authority and 47 started/completed frames with zero
interruptions. That snapshot is not continuous logging of the full interval;
real NTP-failure recovery, physical PPS, prolonged OLED observation, complete
protected-window browser coverage and quantitative RF validation remain open.

An earlier project recorded a stable 20 m FIXED baseline. The associated
private-history identifier is intentionally omitted; the NodeMCU observations
remain historical evidence rather than a resolvable Git reference here.

## License and release status

Original modern WSPR Beacon 2.0 project material is licensed under
GPL-3.0-or-later. The project notice is `Copyright (C) 2026 IW2NNV`; public
credits remain `Developed by IW2NNV / AC2WZ`, and `zampro79` is the public
GitHub identifier. See [LICENSE](LICENSE) and [copyright and scope](COPYRIGHT.md).

Third-party components retain their respective licenses. GPL adoption does not
resolve the outstanding LGPL relinking or ESP8266 NONOS SDK questions, and no
public firmware distribution is currently authorized.

Public visibility of this source repository requires separate, explicit owner
authorization. If granted, that visibility alone would not create an official
source release, authorize a firmware archive or package, or authorize any
precompiled firmware binary. Those actions remain separately gated.

Compliance materials are available in [Third-party notices](THIRD_PARTY_NOTICES.md),
[historical sketch provenance](docs/LEGACY_PROVENANCE.md), [the future
corresponding-source model](docs/SOURCE_DISTRIBUTION.md), and [the collected
third-party license texts](licenses/README.md). The first binary release
candidate is planned for ESP8266 only. ESP32 remains a primary, actively
developed source target, but is excluded from that binary package until hardware
validation is complete.
The provisional RC package designation is `2.0.0-rc.1`; the embedded firmware
identity remains `dev`. See the [RC notes](docs/RELEASE_NOTES_ESP8266_RC.md) and
[targeted ESP8266 license review](docs/ESP8266_LICENSE_REVIEW.md). The RC is not
approved for public distribution.

## Repository layout

The active PlatformIO firmware is under `src/` and `include/`. Five obsolete
root-level Arduino `.ino` sketches from the predecessor project were excluded
from the consolidated repository after their provenance was documented; they
are not recoverable from its current Git history and were never an alternative
active implementation. Arduino Nano Every references and the relocated User
Manual redirect remain historical material.
