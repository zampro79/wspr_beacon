# WSPR Beacon 2.0 — Building Guide

This guide is for developers who want to build, test or upload WSPR Beacon 2.0
from source. Operators installing a future verified binary package should use
the [Installation Guide](INSTALLATION.md). Runtime operation is documented in
the [User Guide](USER_GUIDE.md).

No public firmware binary is currently approved or published. A locally built
`.bin` file is a development artifact, not a release.

## 1. Supported source targets

| Environment | Board | Profile | Hardware status |
| --- | --- | --- | --- |
| `esp8266` | NodeMCU 1.0 / ESP-12E (`nodemcuv2`) | DEVELOPMENT | Build, upload and major runtime paths verified |
| `esp8266_public` | NodeMCU 1.0 / ESP-12E (`nodemcuv2`) | PUBLIC | Build verified; no public binary distribution authorized |
| `esp32` | ESP32 Dev Module (`esp32dev`) | DEVELOPMENT | Build, upload, persistence, HTTP/Web UI and SNTP renewal partially hardware-validated |
| `esp32_public` | ESP32 Dev Module (`esp32dev`) | PUBLIC | Build verified; no public binary distribution authorized and no PUBLIC hardware upload performed |

DEVELOPMENT defaults to `IW2NNV`/`JN45`. PUBLIC defaults to an empty callsign
and `JN45`, keeping TX inhibited until a valid identity is explicitly saved.
Existing valid SAVED configuration takes precedence over either build default.
PUBLIC is a firmware profile, not permission to distribute a binary.

## 2. Development tools

Required:

- Git;
- Python 3;
- PlatformIO Core, which provides `pio`;
- a data-capable USB cable for upload;
- a suitable USB-to-UART driver when the operating system does not already
  provide one.

### VS Code

Install Visual Studio Code and the official PlatformIO IDE extension, then open
the repository directory. PlatformIO reads `platformio.ini` automatically. Use
an explicit environment when building or uploading; the default environment is
ESP8266 DEVELOPMENT only for convenience.

### PlatformIO CLI

Use PlatformIO's supported installer or an isolated environment. For example,
where `pipx` is available:

```sh
pipx install platformio
pio --version
```

Do not bypass operating-system Python protections merely to install this
project globally.

## 3. Clone the repository

```sh
git clone https://github.com/zampro79/wspr_beacon.git
cd wspr_beacon
git status --short --branch
```

Builds embed the selected profile, target and available Git source identity.
For reproducible review artifacts, start from the intended clean commit.

## 4. Pinned build configuration

The authoritative configuration is
[`platformio.ini`](https://github.com/zampro79/wspr_beacon/blob/main/platformio.ini).

| Component | Declared version |
| --- | --- |
| ESP8266 PlatformIO platform | `platformio/espressif8266@4.2.1` |
| ESP8266 board | `nodemcuv2` |
| ESP32 PlatformIO platform | `platformio/espressif32@7.1.3` |
| ESP32 board | `esp32dev` |
| Native PlatformIO platform | `platformio/native@1.2.1` |
| Etherkit Si5351 | `2.2.0` |
| Etherkit JTEncode | `1.3.1` |
| Adafruit GFX | `1.12.6` |
| Adafruit SSD1306 | `2.5.17` |
| EspSoftwareSerial, ESP8266 only | `8.2.0` |

PlatformIO resolves framework, toolchain and transitive package versions from
the pinned platforms. Technical packages record the resolved versions.

## 5. Build and test

Build all firmware profiles explicitly:

```sh
pio run -e esp8266
pio run -e esp8266_public
pio run -e esp32
pio run -e esp32_public
```

Run the two host test profiles:

```sh
pio test -e native
pio test -e native_public
```

The latest confirmed baseline is 559/559 passing cases in each native profile
and successful builds for all four firmware environments.

Web assets are generated deterministically. Check them without rewriting:

```sh
python3 tools/generate_web_assets.py --check
```

Run the whitespace check before review:

```sh
git diff --check
```

## 6. Build outputs

Each environment has a separate `.pio/build/ENVIRONMENT/` directory. Relevant
outputs include:

```text
firmware.bin
firmware.elf
firmware.map
firmware.provenance.json
```

ESP32 builds additionally produce `bootloader.bin` and `partitions.bin`; the
framework supplies `boot_app0.bin`. The Installation Guide lists the exact
component offsets required for a first ESP32 installation.

`firmware.provenance.json` binds the project, development version, environment,
profile, target and source commit to the SHA-256 values of the BIN, ELF and
linker map. It is build evidence, not release authorization.

## 7. Upload from source

Before upload, isolate CLK0 from any antenna or unverified RF chain and identify
the actual serial port:

```sh
pio device list
```

Linux/macOS example:

```sh
PORT=/path/to/discovered-port
pio run -e esp8266 -t upload --upload-port "$PORT"
pio device monitor --port "$PORT" --baud 9600
```

Windows PowerShell example:

```powershell
$Port = "COM5"
pio run -e esp8266 -t upload --upload-port $Port
pio device monitor --port $Port --baud 9600
```

Replace `esp8266` with `esp8266_public`, `esp32` or `esp32_public` only when
that exact target and profile are intended. ESP32 DEVELOPMENT upload has been
validated on the identified project board; ESP32 PUBLIC has not been uploaded
and remains only a build profile.

Do not use `erase_flash` for an ordinary upload. Do not assume `/dev/ttyUSB0`
or a fixed COM number. Close other serial monitors before uploading.

## 8. Local technical packaging

Archive verified local technical packages for all four environments with:

```sh
python3 tools/archive_local_firmware.py
```

Archive one environment with:

```sh
python3 tools/archive_local_firmware.py --environment esp8266
```

Packages are stored under the Git-ignored `local_firmware/ENVIRONMENT/`
directory. Existing archives are preserved. The package contains components,
manifest, `SHA256SUMS`, technical build evidence, dependency inventory, source
material and applicable notices. Validate a package with:

```sh
python3 tools/verify_firmware_package.py /path/to/package
```

For a separate staging directory, build the selected environment first, then
run:

```sh
python3 tools/stage_firmware.py --environment esp8266 \
  --output-dir /path/to/new/empty-directory
```

The same command accepts the other three environment names. Do not combine
profiles or targets in one directory.

Generated packages retain manifest schema 2 and use packaging revision 3.
Revision 3 adds the exact compatible board and a required
`flash_parameters` object (`mode`, `frequency`, `size` and
`recommended_baud`). It also includes `INSTALLATION.md`, `BUILDING.md`,
`USER_GUIDE.md` and `HARDWARE.md` at package root. The validator checks these
fields against the selected target, verifies the local links among the bundled
guides and rejects missing or unexpected material. Earlier revision 1 and 2
packages remain subject to their historical rules; existing archives are not
migrated or rewritten.

The standalone flashing recommendations recorded by revision 3 are DIO,
40 MHz and 4 MB for both targets, with 115200 baud for ESP8266 and 460800 baud
for ESP32. ESP8266 retains `firmware.bin` at `0x0`. ESP32 retains its four
components at `0x1000`, `0x8000`, `0xE000` and `0x10000`; no merged image is
produced. These values describe package installation and do not change the
PlatformIO build configuration.

The `--public-release` staging path is restricted to a clean
`esp8266_public` commit and performs a controlled rebuild. It still does not
publish a release or authorize distribution. Do not use it as a substitute for
release approval.

## 9. Distribution restrictions

Every current package must retain:

```text
public_distribution_authorized: false
NOT_APPROVED_FOR_PUBLIC_DISTRIBUTION
```

The unresolved LGPL relinking and GPLv3/ESP8266 NONOS SDK questions remain
release blockers. ESP32 also lacks target-specific legal review and complete
hardware/RF validation. Do not upload locally produced firmware to a public release, attach
it to a Git tag or describe PUBLIC artifacts as downloadable firmware.

See the repository's
[Source Distribution](https://github.com/zampro79/wspr_beacon/blob/main/docs/SOURCE_DISTRIBUTION.md),
[ESP8266 license review](https://github.com/zampro79/wspr_beacon/blob/main/docs/ESP8266_LICENSE_REVIEW.md) and
[third-party notices](https://github.com/zampro79/wspr_beacon/blob/main/THIRD_PARTY_NOTICES.md).

## 10. Privacy before sharing diagnostics or commits

Never commit Wi-Fi credentials, local configuration, session tokens, NVS or
flash backups. Sanitize browser HAR files, screenshots and serial or diagnostic
logs before attaching them to an issue; they may contain network identifiers,
addresses, request headers or other personal data.

Review Git author and committer metadata before publishing history. Contributors
who do not want their personal email exposed can configure a GitHub `noreply`
address for future commits. This prevents future exposure only; it does not
change metadata already stored in existing commits. Public examples and test
fixtures must use clearly non-secret placeholder values.
