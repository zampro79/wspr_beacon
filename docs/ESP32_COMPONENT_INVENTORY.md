# ESP32 PUBLIC component and license inventory

Status: `COMPLIANCE_INCOMPLETE`.

This is the Phase 1 inventory for the exact archives shown by the GNU ld
archive-member table of an `esp32_public` build. Presence in the installed
framework alone is not evidence of incorporation. The generated package SBOM
records the resolved PlatformIO package identifier and every archive observed
in the packaged link map.

## Resolved baseline

- PlatformIO platform: `platformio/espressif32@7.1.3`;
- Arduino API version: 2.0.17;
- ESP-IDF baseline: 4.4.7;
- toolchain: `toolchain-xtensa-esp32@8.4.0+2021r2-patch5`;
- locally resolved framework during the audit:
  `framework-arduinoespressif32@4.20017.260907+sha.dcc1105b`.

The platform requirement is `~4.20017.0`, so a release must preserve the exact
resolved framework identifier and content hash. The older short version alone
is not a sufficient lock.

## Classification rules

| Class | Examples | License/status |
| --- | --- | --- |
| Project objects and archives | WSPR Beacon `src/*.o` | GPL-3.0-or-later |
| Direct Etherkit libraries | Si5351, JTEncode | GPL-3.0-or-later; retain exact source and notices |
| Direct Adafruit libraries | GFX, SSD1306, BusIO | BSD-2-Clause, BSD-3-Clause and MIT notices |
| Arduino ESP32 core | `libFrameworkArduino.a`, bundled Arduino libraries | LGPL-2.1-or-later; relink path required |
| ESP-IDF open components | NVS and many driver/system components | Usually Apache-2.0, but verified per component rather than inherited globally |
| FreeRTOS | `libfreertos.a` | MIT |
| lwIP | `liblwip.a` | BSD-3-Clause |
| mbedTLS | `libmbed*.a` | Apache-2.0 for the examined IDF baseline |
| newlib | `libnewlib.a` | BSD-family notices must be retained |
| GCC runtime | libgcc/libstdc++ contributions | GPL runtime plus GCC Runtime Library Exception 3.1 |

## Incorporated archives still requiring exact terms

The Phase 1 generator deliberately reports at least the following as unresolved
when the link map proves their inclusion:

- `libbt.a`;
- `libcoexist.a`;
- `libesp_phy.a`;
- `libesp_wifi.a`;
- `libnet80211.a`;
- `libpp.a`;
- `libsmartconfig.a`.

Some contain open adapter code, some contain or depend on Espressif binary
components, and some are mixed. They must not all be labelled Apache-2.0 merely
because ESP-IDF's top-level license is Apache-2.0. Exact upstream component
notices or an authentic Espressif redistribution statement must be retained
before the inventory can be declared complete.

## Tools not incorporated

PlatformIO Core, esptool, SCons, objcopy, compiler executables and build-time
Python modules are tools. They are recorded for reproducibility but are not
classified as incorporated solely because they ran during the build. Runtime
objects selected from libgcc, libstdc++ or libc are classified separately from
the compiler executable.

## Completion criterion

The inventory becomes complete only when every incorporated archive has:

1. exact version or source commit;
2. source/package provenance;
3. applicable license text;
4. required notice;
5. source-availability determination;
6. relink determination where applicable; and
7. evidence tying it to the release ELF/map.

Until then, generated ESP32 packages remain local preparation artifacts with
`release_ready: false` and `public_distribution_authorized: false`.
