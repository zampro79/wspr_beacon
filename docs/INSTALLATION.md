# WSPR Beacon 2.0 — Installation Guide

**Developed by IW2NNV / AC2WZ**

This guide describes the intended end-user path for checking and flashing a
future verified firmware package, starting the beacon and preserving its
configuration during updates. No official binary release is currently
available or authorized. To compile from source, use the
[Building Guide](BUILDING.md).

Keep the Si5351A CLK0 output disconnected from any antenna or unverified RF
chain throughout installation and initial configuration.

## 1. Supported hardware

| Platform | Installation status |
| --- | --- |
| AZDelivery NodeMCU Amica V2 / ESP-12F | Primary target; PlatformIO build and upload verified on hardware |
| ESP32 Dev Module (`esp32dev`) | Experimental; DEVELOPMENT build, first installation, readback and preserved configuration verified on one physical board; no approved official binary |
| Arduino Nano Every | Historical and frozen; not an official installation target |
| Arduino Nano 33 IoT | Unsupported |

The documented NodeMCU uses a 4 MB ESP8266EX and commonly has a Silicon Labs
CP2102 USB-to-UART bridge. Clones may instead use CH340 or another bridge.
Identify the actual USB device before installing a driver. See the
[Hardware Guide](HARDWARE.md) before wiring GPS, OLED or Si5351A modules.

## 2. Choose the correct target and profile

ESP8266 and ESP32 firmware are not interchangeable. Confirm the chip before
writing flash and stop if the detected chip differs from the package target.

Packages may describe either profile:

- **DEVELOPMENT** defaults to `IW2NNV`/`JN45` on fresh storage and is not public
  firmware.
- **PUBLIC** defaults to an empty callsign and `JN45`, keeping TX inhibited
  until a valid identity is saved.

PUBLIC is only a build profile. It does not mean that distribution is approved.
Existing valid SAVED configuration takes precedence over either default.

## 3. PC prerequisites

For a future precompiled package, use Python 3 and a dedicated virtual
environment. The standalone tool versions checked by this project are:

- ESP8266: `esptool==3.0`;
- ESP32: `esptool==4.11.0`.

Do not mix their commands in one procedure.

### Linux or macOS

```sh
python3 -m venv .venv-esp8266-flash
. .venv-esp8266-flash/bin/activate
python -m pip install "esptool==3.0"
python -m esptool version
```

For ESP32, create a separate environment and install `esptool==4.11.0`.

### Windows PowerShell

```powershell
py -3 -m venv .venv-esp8266-flash
.\.venv-esp8266-flash\Scripts\Activate.ps1
python -m pip install "esptool==3.0"
python -m esptool version
```

Use another virtual environment containing `esptool==4.11.0` for ESP32.
PowerShell execution policy and driver installation are host-administration
issues; do not weaken system security merely to run an unverified package.

Developers using PlatformIO should follow the [Building Guide](BUILDING.md).

## 4. Verify the firmware package

No approved package download exists today. When one is eventually authorized,
extract exactly one target/profile package into its own directory. Do not mix
files from different packages.

From a matching source checkout, run:

```sh
python3 tools/verify_firmware_package.py /path/to/package
```

The validator checks manifest identity, profile, target, compatible board,
flash parameters, component inventory, offsets, sizes, SHA-256 values,
`SHA256SUMS`, source identity, local documentation links and the ESP32
partition layout. It also rejects unexpected files and any package that claims
public distribution authorization under the current policy. Packaging revision
3 requires these flash and documentation fields; revisions 1 and 2 remain
verifiable under their original rules.

Revision 3 packages include `INSTALLATION.md`, `BUILDING.md`, `USER_GUIDE.md`
and `HARDWARE.md` at package root so that essential installation, build,
operation and hardware guidance remains locally readable after extraction.
Without a source checkout, verify the included checksum inventory from inside
the package.

Linux:

```sh
sha256sum -c SHA256SUMS
```

macOS:

```sh
shasum -a 256 -c SHA256SUMS
```

Windows PowerShell can cross-check individual files:

```powershell
Get-FileHash -Algorithm SHA256 .\firmware-file.bin
```

Stop if any checksum fails, the manifest target is wrong, a required component
is missing, or the package does not identify the intended commit and profile.

## 5. Connect USB and identify the serial port

Use a known data-capable USB cable. A charge-only cable may power LEDs while
providing no serial device. List ports before connecting the board and again
after connecting it.

### Linux

Typical names are `/dev/ttyUSB0` or `/dev/ttyACM0`; stable links may appear
under `/dev/serial/by-id/`.

```sh
ls -l /dev/serial/by-id/ 2>/dev/null
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

CP210x support is normally provided by the kernel `cp210x` driver; CH340 uses
the `ch341` driver. If access is denied, inspect ownership before changing group
membership:

```sh
PORT=/path/to/discovered-port
ls -l "$PORT"
```

Serial access groups vary by distribution (`dialout`, `uucp` or another local
policy). Apply the distribution's documented procedure and start a new login
session. Do not run the complete installation as root.

### macOS

Prefer `/dev/cu.*` callout devices:

```sh
ls -l /dev/cu.*
```

Recent macOS versions may recognize CP210x or CH340 automatically. Otherwise
install a vendor driver compatible with the exact macOS version and Intel or
Apple Silicon architecture, then approve it through the normal security UI.
This project has not hardware-validated the macOS procedure.

### Windows

Open Device Manager and inspect **Ports (COM & LPT)** before and after connecting
the board. Use the COM number belonging to the detected CP210x, CH340 or other
bridge, for example `COM5`. Install a vendor driver only when Windows does not
already expose the device. The COM number may change between USB ports.

Close PlatformIO monitors, terminal programs and other applications that may
hold the port before flashing.

## 6. ESP8266 first installation

The package contains one flash component:

| Component | Offset |
| --- | ---: |
| `firmware.bin` | `0x0` |

Use the exact filename and SHA-256 from the verified manifest. In the ESP8266
esptool 3.0 environment:

### Linux or macOS

```sh
PORT=/path/to/discovered-port
FIRMWARE=/path/to/package/...-firmware.bin
python -m esptool --chip esp8266 --port "$PORT" chip_id
python -m esptool --chip esp8266 --port "$PORT" --baud 115200 \
  write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB --verify \
  0x0 "$FIRMWARE"
python -m esptool --chip esp8266 --port "$PORT" --baud 115200 \
  verify_flash --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x0 "$FIRMWARE"
python -m serial.tools.miniterm "$PORT" 9600
```

### Windows PowerShell

```powershell
$Port = "COM5"
$Firmware = "C:\path\to\package\...-firmware.bin"
python -m esptool --chip esp8266 --port $Port chip_id
python -m esptool --chip esp8266 --port $Port --baud 115200 `
  write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB --verify `
  0x0 $Firmware
python -m esptool --chip esp8266 --port $Port --baud 115200 `
  verify_flash --flash_mode dio --flash_freq 40m --flash_size 4MB `
  0x0 $Firmware
python -m serial.tools.miniterm $Port 9600
```

The `dio`, `40m` and `4MB` parameters match the supported NodeMCU PlatformIO
target. PlatformIO upload has been exercised on the project NodeMCU. This exact
standalone end-user esptool sequence has been checked against the tool and
packaging layout but has not yet received complete cross-platform hardware
validation.

Do not add `erase_flash`, `--erase-all`, other offsets or a merged image.

## 7. ESP32 first installation — experimental

The ESP32 package contains four required components:

| Component | Offset |
| --- | ---: |
| `bootloader.bin` | `0x1000` |
| `partitions.bin` | `0x8000` |
| `boot_app0.bin` | `0xE000` |
| `firmware.bin` | `0x10000` |

These offsets are enforced by the packaging validator. `firmware.bin` alone is
not a complete first installation.

In the ESP32 esptool 4.11.0 environment, using filenames from the verified
manifest:

### Linux or macOS

```sh
PORT=/path/to/discovered-port
BOOTLOADER=/path/to/package/...-bootloader.bin
PARTITIONS=/path/to/package/...-partitions.bin
BOOT_APP0=/path/to/package/...-boot_app0.bin
FIRMWARE=/path/to/package/...-firmware.bin
python -m esptool --chip esp32 --port "$PORT" chip_id
python -m esptool --chip esp32 --port "$PORT" --baud 460800 \
  write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB --verify \
  0x1000 "$BOOTLOADER" 0x8000 "$PARTITIONS" \
  0xE000 "$BOOT_APP0" 0x10000 "$FIRMWARE"
python -m esptool --chip esp32 --port "$PORT" --baud 460800 \
  verify_flash --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000 "$BOOTLOADER" 0x8000 "$PARTITIONS" \
  0xE000 "$BOOT_APP0" 0x10000 "$FIRMWARE"
python -m serial.tools.miniterm "$PORT" 9600
```

### Windows PowerShell

```powershell
$Port = "COM5"
$Bootloader = "C:\path\to\package\...-bootloader.bin"
$Partitions = "C:\path\to\package\...-partitions.bin"
$BootApp0 = "C:\path\to\package\...-boot_app0.bin"
$Firmware = "C:\path\to\package\...-firmware.bin"
python -m esptool --chip esp32 --port $Port chip_id
python -m esptool --chip esp32 --port $Port --baud 460800 `
  write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB --verify `
  0x1000 $Bootloader 0x8000 $Partitions `
  0xE000 $BootApp0 0x10000 $Firmware
python -m esptool --chip esp32 --port $Port --baud 460800 `
  verify_flash --flash_mode dio --flash_freq 40m --flash_size 4MB `
  0x1000 $Bootloader 0x8000 $Partitions `
  0xE000 $BootApp0 0x10000 $Firmware
```

This procedure is derived from the current PlatformIO build and verified
packaging layout but has not been executed on project ESP32 hardware. The
project does not currently provide or document a merged ESP32 image.

NVS occupies `0x9000–0xDFFF`; OTA selection data starts at `0xE000`. A changed
partition table can reinterpret preserved data. Do not use `--force`, erase
commands or components from another package.

## 8. First boot and AP Provisioning

Open a 9600-baud serial monitor before reset. Confirm the startup identity:

- project `WSPR Beacon 2.0`;
- intended target and DEVELOPMENT/PUBLIC profile;
- expected source commit;
- no unexpected boot loop or critical storage error.

With no usable saved Wi-Fi credentials, the device enters AP Provisioning after
its bounded STA decision. OLED and serial report:

- `WSPR-Beacon-<suffix>` SSID;
- a random eight-digit WPA2 password;
- `192.168.4.1`.

Connect a phone or computer to this AP and open `http://192.168.4.1/` directly;
there is no captive portal. Select **Scan for networks** or enter the SSID
manually, choose OPEN only for an intentionally open network, enter the password
for a protected network and press **Save credentials**. The device verifies the
record but does not reconnect or restart automatically. Restart manually to
attempt normal STA operation.

Alternatively, **Use without Wi-Fi network** enters AP Operational for the
current boot. AP Provisioning always inhibits TX. AP Operational can become
eligible only with precise GPS/PPS and all RF safety gates satisfied.

## 9. Open the Dashboard and configure WSPR

In normal STA mode, read the assigned IPv4 address from OLED or serial output,
then open:

```text
http://DEVICE-IP/
```

Confirm firmware identity, operating mode, time authority, RF readiness and TX
eligibility. Open **Configuration**, explicitly enter the configuration
session, review ACTIVE/CANDIDATE/SAVED values, set a valid callsign and other
intended WSPR parameters, then use **Save and apply**. PUBLIC firmware remains
TX-inhibited until a valid callsign is saved.

For Wi-Fi replacement, use **Enter Wi-Fi Setup Mode** from Configuration and
confirm the request. Reconnect to the beacon AP at `192.168.4.1`; the request
does not erase the currently saved credentials by itself.

The complete operating workflow is in the [User Guide](USER_GUIDE.md).

## 10. Conservative firmware updates

Before updating:

1. Isolate the RF path.
2. Record firmware identity, ACTIVE/CANDIDATE/SAVED state, callsign, locator,
   band, frequency mode, Power Save, correction, power, interval and network
   mode without recording credentials.
3. Verify the new package, target, persistence compatibility and flash layout.
4. Close every serial program using the port.

### ESP8266 application update

Use the same verified `firmware.bin` write at `0x0` described in section 6.
The file ends below the emulated EEPROM sector at `0x3FB000`; an ordinary
application write is intended to preserve WSPR and Wi-Fi records. Verify the
configuration after reboot rather than assuming preservation from upload
success alone.

### ESP32 application-only update

Use an application-only update only after proving that the installed
bootloader and partition table match the new package and that OTA selection is
understood:

```sh
python -m esptool --chip esp32 --port "$PORT" --baud 460800 \
  write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB --verify \
  0x10000 "$FIRMWARE"
python -m esptool --chip esp32 --port "$PORT" --baud 460800 \
  verify_flash --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x10000 "$FIRMWARE"
```

If OTA metadata selects `ota_1`, writing `ota_0` at `0x10000` does not prove
that the new application will boot. ESP32 application-only update remains
hardware-validation dependent; stop rather than rewriting OTA metadata
speculatively.

### Persistence compatibility

ESP8266 uses emulated EEPROM; ESP32 uses NVS. WSPR and Wi-Fi records are
separate A/B records with sequence and CRC checks. The current WSPR payload is
v3. V1 and v2 remain readable with Power Save OFF and migrate only on an
explicit save. Downgrading after a v3 save is not guaranteed.

Do not start an ordinary update with `erase_flash`. Full erasure destroys saved
configuration and is not equivalent to the controlled firmware Factory Reset.

## 11. Problems and conservative recovery

- **No serial port:** try a known data cable and another USB port. Compare the
  operating system's device list before and after connection. Identify CP210x,
  CH340 or the actual bridge before installing a driver.
- **Linux permission denied:** inspect port ownership and follow the
  distribution's serial-group policy; start a new login session afterward.
- **Port busy:** close serial monitors, VS Code/PlatformIO monitors and other
  applications using the port.
- **Wrong chip from `chip_id`:** stop. Do not use `--force` or try another
  target's files.
- **Automatic bootloader entry fails on the NodeMCU:** close the monitor and
  retry once with the same verified upload command. The project board supports
  the standard NodeMCU controls: hold **FLASH** (GPIO0), press and release
  **RST**, then release **FLASH** after the tool begins connecting. Project
  uploads have used automatic reset; this manual fallback has not been recorded
  as a separate validation. Return to normal boot without holding FLASH.
- **ESP32 automatic bootloader entry fails:** button labels and circuits vary.
  A common Dev Module sequence is to hold **BOOT**, pulse **EN/RESET**, start the
  connection and release **BOOT**, but this project has not hardware-validated
  that procedure. Confirm the exact board documentation before trying it.
- **Interrupted upload:** preserve the complete log and package, return the
  correct board to its documented bootloader mode and retry only the same
  verified command. Do not erase all flash as a shortcut.
- **Firmware does not boot:** capture the complete 9600-baud boot log and retain
  the manifest, package and flashing output before changing mode, offsets or
  images.
- **Provisioning page unavailable:** confirm connection to the beacon AP and
  open `http://192.168.4.1/` explicitly. Confirm the current random password on
  OLED or with the serial `wifi ap` command.
- **Configuration page unavailable in normal operation:** confirm the current
  IP, NORMAL or AP Operational mode and that the device is not inside the
  intentional PREPARING/TX HTTP closure. Retry after TX rather than changing
  the HTTP gate.
- **Configuration appears lost:** keep RF isolated, preserve logs and inspect
  ACTIVE/CANDIDATE/SAVED plus boot diagnostics. Do not save new values, run
  Factory Reset or erase flash until the cause is understood.
- **Package or checksum failure:** reject the package; do not reconstruct it
  from files belonging to other builds.

The governing recovery rule is to preserve evidence before destructive action.
A complete flash erase is not a generic recovery procedure and currently has no
routine end-user workflow in this project.

## 12. Distribution and safety status

No official precompiled package is publicly available. Any future package must
retain:

```text
public_distribution_authorized: false
NOT_APPROVED_FOR_PUBLIC_DISTRIBUTION
```

until the LGPL relinking and GPLv3/ESP8266 NONOS SDK questions are resolved and
distribution is separately authorized. ESP32 additionally awaits hardware and
target-specific legal validation.

Successful flashing does not validate RF output. Si5351A CLK0 is harmonic-rich;
use band-appropriate filtering, a safe load and measurement equipment before
connecting an antenna. Never infer measured RF power from the WSPR message's
declared dBm value.
