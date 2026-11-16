# Flashing precompiled firmware

This guide describes the verified offline layout and the intended installation
procedure for a future WSPR Beacon 2.0 PUBLIC package. No precompiled firmware
is currently approved or published. `PUBLIC` is a safe build profile, not a
distribution authorization.

Keep Si5351A CLK0 disconnected from an antenna or unverified RF chain during
installation and first configuration. Flashing success is not RF certification.

## 1. Identify the board and package

ESP8266 and classic ESP32 images are not interchangeable. Use a data-capable
USB cable, compare the operating system's port list before and after connection,
and run the appropriate `chip_id` command before writing flash. Stop if the
detected family differs from the package manifest.

The supported package layouts are specific to the current `nodemcuv2` and
`esp32dev` configurations:

| Target | Component | Offset |
| --- | --- | ---: |
| ESP8266 | `firmware.bin` | `0x000000` |
| ESP32 | `bootloader.bin` | `0x1000` |
| ESP32 | `partitions.bin` | `0x8000` |
| ESP32 | `boot_app0.bin` | `0xE000` |
| ESP32 | `firmware.bin` | `0x10000` |

Both targets use DIO, 40 MHz and 4 MB flash. Recommended serial rates are
115200 baud for ESP8266 and 460800 baud for ESP32. Do not reuse these values for
another board without a matching manifest.

Verify `SHA256SUMS` before installation. On Linux use `sha256sum -c
SHA256SUMS`; on macOS use `shasum -a 256 -c SHA256SUMS`; on Windows compare
`Get-FileHash -Algorithm SHA256 FILE` with the manifest.

## 2. Serial ports and drivers

- **Windows:** compare Device Manager **Ports (COM & LPT)** before and after
  connection. Install a CP210x, CH340 or other vendor driver only after
  identifying the actual USB-UART bridge.
- **Linux:** inspect `/dev/serial/by-id/`, `/dev/ttyUSB*` and `/dev/ttyACM*`.
  Follow the distribution's serial-group policy if access is denied; do not run
  the complete procedure as root.
- **macOS:** inspect `/dev/cu.*`. Prefer a callout device and install a driver
  only if the exact bridge and macOS version require it.

Close serial monitors and other programs that hold the port.

## 3. Windows: Espressif Flash Download Tool

Follow the official
[Espressif Flash Download Tool User Guide](https://docs.espressif.com/projects/esp-test-tools/en/latest/esp32/production_stage/tools/flash_download_tool.html).
The names below follow the current official guide; older releases may present a
different launcher.

1. Start the tool and select the detected chip family, not merely the board's
   appearance.
2. Select **Develop** work mode and **UART** load mode when offered.
3. In **Download Path Config**, enable each required BIN and enter its exact
   offset from the table above. For an ESP32 factory image, select only
   `wspr-beacon-esp32-public-factory.bin` at `0x0`.
4. Select DIO, 40 MHz and 4 MB only when the tool exposes these controls. Recent
   versions may keep parameters from the image header.
5. Select the discovered COM port and a conservative baud rate.
6. Press **START** and wait for the success indication.
7. Do not press **ERASE** for an ordinary installation or update.
8. Reset the device and inspect the 9600-baud startup identity.

Flash Download Tool supports both ESP8266 and ESP32. Its factory mode is not
needed for individual installation and no eFuse, secure-boot or flash-encryption
operation is part of this procedure.

## 4. Linux, macOS and Windows CLI: esptool

Use isolated Python environments. These commands were checked with
`esptool==3.0` for ESP8266 and `esptool==4.11.0` for ESP32. Newer esptool
versions use hyphenated command names; consult the documentation for the
installed version rather than mixing syntaxes.

```sh
python3 -m venv .venv-flash
. .venv-flash/bin/activate
python -m pip install "esptool==4.11.0"
python -m esptool version
```

### ESP8266 first installation or ordinary application update

```sh
python -m esptool --chip esp8266 --port "$PORT" chip_id
python -m esptool --chip esp8266 --port "$PORT" --baud 115200 \
  write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB --verify \
  0x0 firmware.bin
python -m esptool --chip esp8266 --port "$PORT" --baud 115200 \
  verify_flash --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x0 firmware.bin
```

The current application ends below the emulated EEPROM sector. An ordinary
application write is intended to preserve configuration, but verify
ACTIVE/CANDIDATE/SAVED after reboot.

### ESP32 first installation with separate components

```sh
python -m esptool --chip esp32 --port "$PORT" chip_id
python -m esptool --chip esp32 --port "$PORT" --baud 460800 \
  write_flash --flash_mode dio --flash_freq 40m --flash_size 4MB --verify \
  0x1000 bootloader.bin 0x8000 partitions.bin \
  0xE000 boot_app0.bin 0x10000 firmware.bin
python -m esptool --chip esp32 --port "$PORT" --baud 460800 \
  verify_flash --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000 bootloader.bin 0x8000 partitions.bin \
  0xE000 boot_app0.bin 0x10000 firmware.bin
```

### ESP32 factory image

```sh
python -m esptool --chip esp32 --port "$PORT" --baud 460800 \
  write_flash 0x0 wspr-beacon-esp32-public-factory.bin
```

The raw factory image fills gaps with `0xFF`, including the NVS configuration
range at `0x9000-0xDFFF`. Treat it as a first-installation or deliberately
destructive recovery path. Never use it as a configuration-preserving update.

### ESP32 application-only update

Writing only `firmware.bin` at `0x10000` does not overwrite NVS directly, but
is safe only when the installed bootloader and partition table match and OTA
selection is understood. If OTA metadata selects `ota_1`, writing `ota_0` does
not prove that the new application will boot. This path remains hardware-test
dependent and is not promised as a general end-user update.

## 5. ESP Launchpad

[ESP Launchpad](https://espressif.github.io/esp-launchpad/) is an optional
browser flasher documented by Espressif for ESP32 devices. Its DIY mode accepts
one or more local images and offsets. Classic ESP32 compatibility is plausible
for the present layout but has not been hardware-validated by this project.
ESP8266 support is not claimed. No custom Web installer is provided.

## 6. First boot and configuration

Open a 9600-baud monitor and confirm project, PUBLIC profile, target and source
commit. With no usable Wi-Fi record, connect to the password-protected
`WSPR-Beacon-<suffix>` AP using the random password shown by OLED or serial,
then open `http://192.168.4.1/`. Save Wi-Fi credentials and restart manually.

In STA mode, open the displayed IPv4 address, enter Configuration, set a valid
callsign and locator, review band, frequency mode, correction, declared power
and interval, then explicitly **Save and apply**. PUBLIC firmware remains
TX-inhibited until a valid operating identity and every existing safety gate
permit a future slot.

## 7. Conservative recovery

- No port: try a known data cable, another USB port and compare device lists.
- Permission denied: use the operating system's documented serial-access policy.
- Wrong chip: stop; never use `--force`.
- Connection failure: close monitors, retry at a lower baud and use the exact
  board's documented BOOT/FLASH procedure.
- Interrupted write: retain logs and retry only the same verified files and
  offsets.
- No boot: preserve the 9600-baud boot log and package before changing anything.
- Lost configuration: do not erase or Factory Reset until ACTIVE, CANDIDATE,
  SAVED and storage diagnostics have been recorded.

`erase_flash` and the Flash Download Tool **ERASE** button are not generic
troubleshooting steps. They destroy saved data and differ from the firmware's
controlled Factory Reset.

## 8. RF safety

Keep the RF output disconnected or connected to an appropriate dummy load
during initial verification. Before connecting an antenna, use a suitable
band-specific filter and measure frequency, power, harmonics and startup
transients. The WSPR message's declared dBm value is not a power measurement.
Operation remains subject to the operator's authorization and local amateur
radio rules.
