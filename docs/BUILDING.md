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

## 2. Recommended path: VS Code and PlatformIO IDE

This is the recommended path for users who do not normally use PlatformIO.
Visual Studio Code and PlatformIO IDE are available for Linux, Windows and
macOS. PlatformIO IDE manages its own PlatformIO Core and normally uses its
built-in Python, so do **not** install a second standalone Core merely to follow
this procedure. Git is needed only for the clone method below. A USB cable,
USB-to-UART driver and serial permissions are needed for upload, not for a
source-only build.

1. Install [Visual Studio Code](https://code.visualstudio.com/Download).
2. In VS Code Extensions, install the official
   [PlatformIO IDE](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide)
   extension. On Linux, make sure the distribution's Python 3 virtual-environment
   support is installed (`python3-venv` or the distribution's equivalent). Do
   not use `sudo` to build the project.
3. Obtain the source using either **Git clone** or **Download ZIP** as described
   in section 3.
4. Extract the ZIP if applicable. In VS Code choose **File → Open Folder** and
   open the `wspr_beacon` folder that directly contains `platformio.ini`. Do not
   create a new PlatformIO project and do not open only `src/` or its parent.
5. Wait for PlatformIO to initialize the project and download the pinned
   platform, framework, toolchain and library dependencies. This first operation
   requires network access and can take several minutes.
6. Open the PlatformIO view (ant icon) and expand **Project Tasks**.
7. Expand exactly one community profile: `esp8266_public` for NodeMCU ESP8266,
   or `esp32_public` for the classic ESP32 Dev Module.
8. Under that environment select **General → Build**. Do not use a generic Build
   button until the PlatformIO environment switcher visibly shows the intended
   `_public` environment: the project default is `esp8266` DEVELOPMENT.
9. Wait for the terminal to report `SUCCESS`. The RAM and Flash percentages are
   capacity summaries for the selected target; a successful build is not a
   hardware or RF test.
10. Find the application binary relative to the project folder:

    ```text
    .pio/build/esp8266_public/firmware.bin
    .pio/build/esp32_public/firmware.bin
    ```

11. Confirm that the terminal named the intended environment and that the path
    matches the target. Never interchange ESP8266 and ESP32 artifacts.
12. Continue with the [Flashing Guide](FLASHING.md). An ESP32 application
    `firmware.bin` alone is not a complete first-installation image; the guide
    documents the required bootloader, partition table, `boot_app0` and offsets.

PlatformIO's official documentation confirms that Core is built into the IDE
and describes the environment switcher and per-environment Project Tasks:
[PlatformIO IDE for VS Code](https://docs.platformio.org/en/latest/integration/ide/vscode.html).

## 3. Obtain the source: Git clone or Download ZIP

### Git clone

Use a Git client or a terminal:

```text
git clone https://github.com/zampro79/wspr_beacon.git
cd wspr_beacon
git status --short --branch
```

A clone includes `.git`, allowing the build to embed the current source commit.
For reviewed artifacts, start from the intended clean commit.

### Download ZIP

On the GitHub repository page choose **Code → Download ZIP**, extract the archive
and open the extracted folder that contains `platformio.ini`. Download ZIP is a
valid method for a personal source build and does not require Git.

GitHub ZIP archives do not contain `.git`. The build therefore records
`source_commit: unknown` in the firmware identity and provenance file. This does
not change the selected profile, scheduler, RF safety or firmware functionality.
It only removes verifiable Git provenance, so the controlled public-release
packaging path rejects such a build. `unknown` is not a toolchain error.

## 4. Optional advanced path: standalone PlatformIO Core CLI

Skip this section when using PlatformIO IDE's built-in terminal. For a standalone
CLI, follow PlatformIO's current
[Core installation guide](https://docs.platformio.org/en/latest/core/installation/index.html).
The recommended installer creates an isolated Python environment; current
PlatformIO documentation requires Python 3.9 or later. Homebrew is also an
official macOS option. Avoid multiple Core installations and do not bypass
operating-system Python protections.

After `pio --version` succeeds, change to the directory containing
`platformio.ini` and build one explicit PUBLIC environment:

```text
pio run -e esp8266_public
pio run -e esp32_public
```

These commands are the same on Linux, Windows PowerShell/CMD and macOS once
`pio` is available in that terminal. They are alternatives: build only the
target you intend to use.

The host-only test suites are optional developer checks, not prerequisites for
building firmware:

```text
pio test -e native
pio test -e native_public
```

## 5. Host-specific notes

### Linux

- Install VS Code and PlatformIO IDE as described in section 2.
- PlatformIO IDE requires Python 3 virtual-environment support. Package names
  vary by distribution; use its documented equivalent of `python3-venv`.
- For standalone Core, use PlatformIO's isolated installer or another supported
  method. Never run ordinary builds with `sudo`.
- Serial groups, udev rules and USB drivers are upload concerns and are not
  required to compile source.
- A clean Fedora Linux build has been verified for both PUBLIC targets with
  PlatformIO Core 6.1.19.

### Windows 10/11

- Use VS Code with PlatformIO IDE for the simplest setup. Git for Windows is
  optional when using Download ZIP.
- The PlatformIO IDE terminal already exposes its built-in `pio`. A separate
  system-wide CLI is optional.
- Do not install CP210x/CH340 drivers merely to compile; they matter only when
  connecting a board.
- If a standalone `pio` is not recognized, use the PlatformIO IDE terminal or
  follow PlatformIO's shell-command/PATH instructions rather than copying Bash
  profile commands into PowerShell or CMD.
- **Windows host builds have not yet been validated directly by this project.**
  This is a host-toolchain evidence gap, separate from hardware validation.

### macOS Intel and Apple Silicon

- Use VS Code with PlatformIO IDE, or optionally install standalone Core using
  an officially supported method such as the isolated installer or Homebrew.
- Both Git clone and Download ZIP follow sections 2 and 3; build the explicit
  PUBLIC environment and use the same relative artifact paths.
- **Neither macOS Intel nor macOS Apple Silicon has yet been validated with a
  real project build.** General PlatformIO Core support for macOS does not prove
  that every pinned Xtensa toolchain package is available for Apple Silicon.
- Do not assume Rosetta is required or sufficient. If PlatformIO reports that a
  pinned toolchain is unavailable for the host architecture, stop and preserve
  the exact error for review rather than changing package versions.

## 6. Pinned build configuration

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

The documented clean Fedora baseline used PlatformIO Core 6.1.19 and resolved
the following target packages. These are recorded observations, not additional
version overrides:

| Target | Framework package | Toolchain package |
| --- | --- | --- |
| ESP8266 | `framework-arduinoespressif8266@3.30102.0` (Arduino core 3.1.2) | `toolchain-xtensa@2.100300.220621` (GCC 10.3.0) |
| ESP32 | `framework-arduinoespressif32@4.20017.260907+sha.dcc1105b` (Arduino core 2.0.17) | `toolchain-xtensa-esp32@8.4.0+2021r2-patch5` |

## 7. Build evidence and developer checks

Developers can build all firmware profiles explicitly:

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
and successful builds for all four firmware environments. A clean Fedora Linux
environment reproduced functional ESP8266 PUBLIC and ESP32 PUBLIC builds using
PlatformIO Core 6.1.19. Windows, macOS Intel and macOS Apple Silicon builds have
not been directly tested. Functional Linux reproducibility is verified;
byte-for-byte reproducibility across clean builds or host systems is not
demonstrated.

Web assets are generated deterministically. Check them without rewriting:

```sh
python3 tools/generate_web_assets.py --check
```

Run the whitespace check before review:

```sh
git diff --check
```

## 8. Build outputs

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

For the recommended profiles, the application binaries are exactly:

```text
.pio/build/esp8266_public/firmware.bin
.pio/build/esp32_public/firmware.bin
```

These paths are relative to the project folder. ESP32 `firmware.bin` is the
application component, not necessarily a complete first installation. Follow
the [Flashing Guide](FLASHING.md) for the complete component set and offsets.

`firmware.provenance.json` binds the project, development version, environment,
profile, target and source commit to the SHA-256 values of the BIN, ELF and
linker map. It is build evidence, not release authorization.

## 9. Upload from source

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

## 10. Build troubleshooting

- **`pio` is not recognized:** in VS Code use the PlatformIO IDE terminal and
  confirm that built-in Core is enabled. For standalone use, follow PlatformIO's
  shell-command instructions instead of editing PATH blindly.
- **Multiple PlatformIO Core installations:** keep the IDE's built-in Core or
  one intentional standalone installation. Record `pio --version` and remove an
  obsolete duplicate only through the method that installed it.
- **Dependency download blocked:** confirm ordinary network access, available
  disk space and whether a firewall, antivirus or corporate proxy blocked the
  PlatformIO package host. Preserve the full error before retrying.
- **Proxy required:** use the documented PlatformIO/VS Code proxy settings. Do
  not publish proxy credentials or disable TLS verification without an approved
  local policy.
- **Python or virtual-environment error:** on Linux install the distribution's
  Python 3 venv support; for standalone Core use PlatformIO's isolated installer.
  Do not modify the system Python with privileged `pip` commands.
- **Wrong project folder:** the opened/current directory must directly contain
  this repository's `platformio.ini`.
- **Wrong firmware environment:** stop if the terminal does not name exactly
  `esp8266_public` or `esp32_public`; clean and rebuild the intended environment.
- **Dependencies unresolved:** retry only after checking the network/proxy and
  the pinned names in `platformio.ini`; do not silently substitute versions.
- **Toolchain unavailable for the host architecture:** retain the package name,
  version and complete error. Do not prescribe emulation or unpin the toolchain
  without a separately reviewed compatibility decision.

Official references: [PlatformIO troubleshooting](https://docs.platformio.org/en/latest/core/installation/troubleshooting.html)
and [proxy configuration](https://docs.platformio.org/en/latest/core/installation/proxy-configuration.html).

## 11. Local technical packaging

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
`recommended_baud`). It also includes the installation, building, flashing,
component-inventory, relinking, user and hardware guides at package root. The
validator checks these fields against the selected target, verifies the local
links among the bundled guides and rejects missing or unexpected material.
Earlier revision 1 and 2 packages remain subject to their historical rules;
existing archives are not migrated or rewritten.

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

### ESP32 PUBLIC compliance preparation

After producing and validating a local `esp32_public` package, prepare the
separate fail-closed Phase 1 package with:

```sh
python3 tools/package_public_firmware.py \
  --staged-package /path/to/verified-esp32-public-package \
  --output-dir /path/to/new-empty-output
```

The tool reuses the existing package validator, verifies the four component
offsets and bootloader flash header, generates a factory image with the
installed esptool, checks every component at its merged offset, writes an SBOM
from actual link-map evidence and records exact resolved PlatformIO packages.
It always emits `COMPLIANCE_INCOMPLETE`, `release_ready: false` and
`public_distribution_authorized: false`. The factory image is a destructive
first-install/recovery path because its raw `0xFF` padding covers NVS.
The preparation manifest preserves the input clean/dirty state and marks source
correspondence verified only for a technically eligible clean package.

See the [Flashing Guide](FLASHING.md), [ESP32 component inventory](ESP32_COMPONENT_INVENTORY.md)
and [LGPL relinking notes](LGPL_RELINKING.md).

## 12. Distribution restrictions

Every current package must retain:

```text
public_distribution_authorized: false
NOT_APPROVED_FOR_PUBLIC_DISTRIBUTION
```

The unresolved LGPL relinking and GPLv3/ESP8266 NONOS SDK questions remain
release blockers. ESP32 also lacks target-specific legal review and complete
hardware/RF validation. The current repository distribution model is
source-only; locally built binaries remain private development artifacts. Do
not upload them to a public release, attach them to a Git tag or describe PUBLIC
artifacts as downloadable firmware.

See the repository's
[Source Distribution](https://github.com/zampro79/wspr_beacon/blob/main/docs/SOURCE_DISTRIBUTION.md),
[ESP8266 license review](https://github.com/zampro79/wspr_beacon/blob/main/docs/ESP8266_LICENSE_REVIEW.md) and
[third-party notices](https://github.com/zampro79/wspr_beacon/blob/main/THIRD_PARTY_NOTICES.md).

## 13. Privacy before sharing diagnostics or commits

Never commit Wi-Fi credentials, local configuration, session tokens, NVS or
flash backups. Sanitize browser HAR files, screenshots and serial or diagnostic
logs before attaching them to an issue; they may contain network identifiers,
addresses, request headers or other personal data.

Review Git author and committer metadata before publishing history. Contributors
who do not want their personal email exposed can configure a GitHub `noreply`
address for future commits. This prevents future exposure only; it does not
change metadata already stored in existing commits. Public examples and test
fixtures must use clearly non-secret placeholder values.
