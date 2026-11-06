# Third-party notices — ESP8266 release preparation

Original modern WSPR Beacon 2.0 project material is licensed under
GPL-3.0-or-later as described in `LICENSE` and `COPYRIGHT.md`. This document is
the separate review inventory for the pinned ESP8266 build. It is not release
clearance or a legal opinion. Third-party components retain the terms below,
and a package must be rebuilt and inspected at its exact release commit.

## Firmware dependencies

| Component | Version | Copyright / attribution found locally | License | Role and inclusion | Upstream | Local evidence | Review status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Etherkit Si5351 | 2.2.0 | Jason Milldrum; adapted portions attributed to Bernardo Innocenti | GPL-3.0-or-later | Direct runtime library; used by the firmware | <https://github.com/etherkit/Si5351Arduino> | `Etherkit Si5351/src/si5351.h`; GPL text shared in `licenses/GPL-3.0.txt` | Source and notice must accompany a distribution as required by GPLv3 |
| Etherkit JTEncode | 1.3.1 | Jason Milldrum; Reed-Solomon files also attribute Phil Karn, KA9Q | GPL-3.0-or-later | Direct runtime library; WSPR encoding code is linked | <https://github.com/etherkit/JTEncode> | `Etherkit JTEncode/COPYING` and source headers | Source and notices must be preserved |
| Adafruit GFX Library | 1.12.6 | Adafruit Industries; individual optional font files have additional notices | BSD-2-Clause | Direct display runtime library; only referenced objects/assets are expected to link | <https://github.com/adafruit/Adafruit-GFX-Library> | `Adafruit GFX Library/license.txt` | Retain notice; do not infer that every packaged font is in the binary |
| Adafruit SSD1306 | 2.5.17 | Adafruit Industries | BSD-3-Clause | Direct OLED runtime library; linked | <https://github.com/adafruit/Adafruit_SSD1306> | `Adafruit SSD1306/license.txt` | Reproduce copyright, conditions and disclaimer |
| Adafruit BusIO | 1.17.4 | Adafruit Industries (2017) | MIT | Transitive display/I/O runtime dependency; referenced code may link | <https://github.com/adafruit/Adafruit_BusIO> | `Adafruit BusIO/LICENSE` | Include copyright and permission notice |
| EspSoftwareSerial | 8.2.0 | Peter Lerup; Dirk O. Kaar | LGPL-2.1-or-later | Direct runtime library used for the GPS serial path; statically linked | <https://github.com/plerup/espsoftwareserial> | `EspSoftwareSerial/LICENSE` and source headers | Relinking/replacement obligations require completion; see below |
| ghostl | 1.0.1 | Dirk O. Kaar; one scheduler file also attributes esp8266/Arduino | LGPL-2.1-or-later | Transitive dependency of EspSoftwareSerial; referenced objects may be statically linked | <https://github.com/dok-net/ghostl> | `ghostl/LICENSE` and source headers | Relinking/replacement obligations require completion; see below |
| Arduino ESP8266 Core | 3.1.2 (PlatformIO package 3.30102.0) | Multiple contributors; file-level notices govern individual components | Predominantly LGPL-2.1-or-later, with bundled components under their own terms | Framework/runtime statically linked into ESP8266 firmware | <https://github.com/esp8266/Arduino/tree/3.1.2> | framework root `LICENSE`, file-level notices, package metadata | Mixed-component inventory must be retained for a release package |
| ESP8266 NONOS SDK | pinned by Core 3.1.2; default `NONOSDK22x_190703` | Espressif Systems | ESPRESSIF MIT License (ESP8266-restricted) | Proprietary binary SDK archives participate in the framework link | <https://github.com/espressif/ESP8266_NONOS_SDK> | framework `tools/sdk/LICENSE`, build script and archive set | GPLv3 compatibility/system-library treatment is `UNRESOLVED_LEGAL_COMPATIBILITY` |
| GCC runtime components | xtensa-lx106 toolchain 2.100300.220621 (GCC 10.3.0) | Free Software Foundation and component contributors | GPLv3 with GCC Runtime Library Exception 3.1 where the file bears its notice | `libgcc` and `libstdc++` are selected by the static link; GCC runtime symbols occur in the ELF | <https://www.gnu.org/licenses/gcc-exception-3.1.en.html> | Compiler/ELF identity, linker command and `licenses/GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt` | Exception evidence verified for covered GCC runtime files; not generalized to SDK/toolchain files |

Dependency paths above are relative to PlatformIO's installed library/framework
packages, not files maintained by this repository. Exact texts verified from
those packages are preserved under [`licenses/`](licenses/).

## Build and test tooling

PlatformIO Core and the `espressif8266` platform package 4.2.1 orchestrate the
build but are not themselves copied into the application firmware. The native
test environment uses Unity as test-only code. Their own distributions and
licenses remain independently governed; they are not evidence that the
firmware may be redistributed.

## Static linking and corresponding source

The PlatformIO firmware link is static. GPL-covered Si5351 and JTEncode objects
are linked with project objects. EspSoftwareSerial, ghostl and LGPL portions of
the ESP8266 framework may also be statically linked. For a future binary
distribution, the project therefore intends to provide the exact project and
dependency corresponding source, build inputs and notices described in
[`docs/SOURCE_DISTRIBUTION.md`](docs/SOURCE_DISTRIBUTION.md).

Whether LGPL compliance additionally requires project object files or another
practical relinking/replacement mechanism has not yet been resolved. No claim
of completed LGPL compliance is made.

## ESP8266 SDK boundary

The pinned framework selects `NONOSDK22x_190703`. Its build logic exposes
binary archives including `libhal`, `libphy`, `libpp`, `libnet80211`, the
selected lwIP library, `libwpa`, `libcrypto`, `libmain`, `libwps`,
`libbearssl`, `libespnow`, `libsmartconfig`, `libairkiss` and `libwpa2` to the
static link. This list describes the framework link inputs; it does not assert
that every member of every archive is copied into the final binary.

Final-ELF symbol comparison confirms contributions from `libhal`, `libphy`,
`libpp`, `libnet80211`, `liblwip2-536-feat`, `libwpa`, `libcrypto` and
`libmain`. The remaining listed SDK archives were offered to the linker but had
no matching contributed global symbols in this build.

The local SDK license permits use, copying and distribution for ESP8266 hardware
and must be preserved exactly. Complete source for the binary SDK archives is not present
in the installed package. Whether these archives qualify for a GPLv3 system
library exception, and whether the hardware restriction is compatible with
the intended GPLv3 combined distribution, remain
`UNRESOLVED_LEGAL_COMPATIBILITY`.

## Release gate

No public distribution is authorized. Before release, at minimum: preserve the
project grant and all third-party notices, confirm that removed historical
material is excluded, settle LGPL relinking materials, settle the SDK/GCC
exception questions, and verify this inventory against the exact clean ESP8266
release build.
