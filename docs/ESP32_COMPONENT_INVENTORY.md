# ESP32 PUBLIC component and license inventory

Status: `COMPLIANCE_INCOMPLETE`.

This is the Phase 2 inventory for the exact archives shown by the GNU ld
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

## Phase 2 classification of the former 49 unresolved archives

The machine-readable classification is in
`tools/esp32_archive_licenses.py`. It is fixed to ESP-IDF commit
`38eeba213aa695aabfd6d89aa9f5078dbe5a94c3` and the exact submodule commits,
not inferred from archive names.

| Archives | Classification and evidence |
| --- | --- |
| 30 ESP-IDF source components, including `libbt.a`, `libesp_phy.a` and `libesp_wifi.a` | Apache-2.0 source components at ESP-IDF v4.4.7. These three are open adapter/host archives; they are not the radio blobs. |
| `libcoexist.a`, `libcore.a`, `libmesh.a`, `libnet80211.a`, `libpp.a`, `libsmartconfig.a` | Apache-2.0 object form from `esp32-wifi-lib` commit `f2aae4d44ec7908013066e69d29b9948846c335c`. Its README explicitly identifies binary RF libraries and its LICENSE grants Apache-2.0 object redistribution. |
| `libphy.a`, `librtc.a` | Apache-2.0 object form from `esp-phy-lib` commit `dcfdccf6cc2fc02d0886624b7998c890d1a19b28`. |
| `libbtdm_app.a` | Apache-2.0 object form from `esp32-bt-lib` commit `e43c9b121f7fac89b73c56077ede5901f64643cd`. |
| `libjson.a` | cJSON MIT, commit `87d8f0961a01bf09bef98ff89bae9fdec42181ee`. |
| `libprotobuf-c.a` | protobuf-c BSD-2-Clause, commit `abc67a11c6db271bedbb9f58be85d6f4e2ea8389`. |
| `libwpa_supplicant.a` | BSD-3-Clause terms in ESP-IDF `components/wpa_supplicant/COPYING`. |
| `libc.a`, `libm.a` | newlib's per-file BSD-family and other permissive notices; the complete `COPYING.NEWLIB` must accompany a binary distribution. |
| `libgcc.a`, `libstdc++.a` | GPL-3.0-or-later with GCC Runtime Library Exception 3.1 from the resolved Xtensa toolchain. |
| `libxtensa.a` | MIT Cadence/Tensilica portions and Apache-2.0 Espressif portions. |
| `libxt_hal.a` | MIT Cadence/Tensilica Xtensa HAL, with the permission notice present in the matched ESP-IDF component. |

No hardware-only restriction was present in the exact LICENSE files of the
three Espressif binary-library repositories. They use unmodified Apache-2.0
terms. This removes the hypothesized hardware-restriction/GPLv3 conflict for
these exact commits; it does not substitute for legal review of the combined
distribution.

### `libesp_diagnostics.a` closure

The resolved framework's `tools/sdk/versions.txt` records esp-rainmaker
`d8e9345`, resolved to
`d8e93454f495bd8a414829ec5e86842b373ff555`. That commit's esp-insights gitlink
is `203a87f671710295579404f7d4c62a8a5e25ae7c`. The component CMake source list
at that exact commit matches all seven archive members, and their embedded
DWARF paths name the same esp-rainmaker/esp-insights component and the expected
Xtensa 8.4.0 toolchain. The installed archive SHA-256 is
`f7e621272ab8b74c27a2dbd2cd818d6439cc01b0b130654e0fc9c6d3f796e940`.
Every matched source file carries Copyright 2021 Espressif Systems (Shanghai)
PTE LTD and Apache-2.0 headers; the repository license is Apache-2.0. This is
sufficient provenance and license evidence without claiming a byte-reproducible
rebuild.

All 49 formerly unresolved archives are now classified. A local Phase 2 bundle
maps all 65 incorporated archives to 15 deduplicated license/notice texts and
retains per-component attribution, version, package, redistribution conditions
and link-map evidence. The packaging tool rejects incomplete coverage or
changed text hashes. The package remains `COMPLIANCE_INCOMPLETE` until the
combined distribution receives the required legal decision.

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
