# Third-party license texts

This directory contains verbatim license files copied from the exact locally
installed dependencies used to review the ESP8266 build. These are third-party
texts; the original modern WSPR Beacon 2.0 project material is separately
licensed under GPL-3.0-or-later by the repository-root `LICENSE`.

| File | Applies to | Local provenance |
| --- | --- | --- |
| `GPL-3.0.txt` | Etherkit Si5351 2.2.0 and JTEncode 1.3.1 (whose source headers specify GPL-3.0-or-later) | `Etherkit JTEncode/COPYING` |
| `LGPL-2.1.txt` | EspSoftwareSerial 8.2.0, ghostl 1.0.1 and LGPL portions of Arduino ESP8266 Core 3.1.2 | `EspSoftwareSerial/LICENSE` |
| `BSD-2-Clause-Adafruit-GFX.txt` | Adafruit GFX Library 1.12.6 | `Adafruit GFX Library/license.txt` |
| `BSD-3-Clause-Adafruit-SSD1306.txt` | Adafruit SSD1306 2.5.17 | `Adafruit SSD1306/license.txt` |
| `MIT-Adafruit-BusIO.txt` | Adafruit BusIO 1.17.4 | `Adafruit BusIO/LICENSE` |
| `ESPRESSIF-NONOS-SDK.txt` | Binary ESP8266 NONOS SDK components shipped with the pinned Arduino core | `tools/sdk/LICENSE` in Arduino ESP8266 Core 3.1.2 |
| `GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt` | Covered GCC 10.3.0 runtime files, including linked libgcc/libstdc++ portions bearing the exception notice | FSF GCC Runtime Library Exception 3.1; verified against the toolchain source license copy |

The actual link selects libgcc and libstdc++, and the ELF contains GCC runtime
symbols. The exception is limited to runtime files that carry its notice and
does not apply to unrelated SDK or toolchain components.

See [the project copyright and license scope](../COPYRIGHT.md) and [the
third-party inventory](../THIRD_PARTY_NOTICES.md). Project licensing does not
change or supersede any third-party term represented in this directory.
