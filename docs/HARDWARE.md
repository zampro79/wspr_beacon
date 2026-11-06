# WSPR Beacon 2.0 — Hardware

This document separates firmware-defined pin assignments from connections and
RF characteristics that still require physical verification. It is not a
complete schematic and does not authorize on-air transmission. The definitive
MCU pin definitions are in `include/BoardConfig.h`.

## Platforms

### AZDelivery NodeMCU Amica V2 / ESP-12F

This is a primary development platform, the planned target of the first binary
release, and uses PlatformIO board `nodemcuv2`.

| Function | NodeMCU pin | GPIO | Notes |
| --- | --- | --- | --- |
| FLASH button to GND | D3 | GPIO0 | Boot strap: never hold low during reset or power-up |
| GPS PPS input | D5 | GPIO14 | Rising-edge interrupt |
| GPS TX → MCU RX | D6 | GPIO12 | Software serial, 9600 baud |
| Optional MCU TX → GPS RX | D7 | GPIO13 | Check GPS input voltage before connecting |
| Primary onboard status LED | D4 / `LED_BUILTIN` | GPIO2 | Active-low; also a strap pin |
| Time-authority onboard LED | D0 | GPIO16 | Active-low; NTP blink, GPS/PPS steady |
| I²C SDA | D2 | GPIO4 | ESP8266 `Wire` default |
| I²C SCL | D1 | GPIO5 | ESP8266 `Wire` default |

ESP8266 GPIO uses 3.3 V logic and is not normally 5 V tolerant. USB may power
the board, but signal levels remain 3.3 V.

No external LED is required. Firmware initializes both onboard LED outputs to
their inactive electrical levels before enabling output mode. D4 carries the
primary status patterns; D0 separately reports the existing qualified time
authority and is frozen during RF-critical activity. Reset-time pin behavior
before firmware startup is hardware-controlled and is not covered by this
software guarantee.

### ESP32 Dev Module

This is a primary development platform sharing the firmware core with ESP8266.
The tested board identifies as an ESP32-D0WD-V3 revision 3.0 with 4 MiB flash
and a CP2102 USB-UART bridge, using the PlatformIO `esp32dev` target and Arduino
ESP32 framework 2.0.17. Hardware validation is partial and does not authorize
an official binary release.

| Function | Firmware definition |
| --- | --- |
| Button | GPIO25 |
| GPS PPS | GPIO26 |
| GPS TX → MCU RX2 | GPIO16 |
| Optional MCU TX2 → GPS RX | GPIO17 |
| Status LED | GPIO2, active-high |
| I²C SDA / SCL | GPIO21 / GPIO22, explicit ESP32-WROOM-32 configuration |

GPIO21 and GPIO22 match the ESP32-WROOM-32 DevKit/DevKitC `Wire` defaults.
GPIO2 boot and LED behavior varies by manufacturer.

Controlled DEVELOPMENT validation confirmed upload and byte-for-byte readback,
normal boot, saved-credential STA operation, WSPR v3 persistence with
ACTIVE/CANDIDATE/SAVED agreement, and detection of the Si5351A, SSD1306 OLED
and GPS NMEA receiver. The tested wiring used GPIO21/GPIO22 for I2C and GPIO16
for GPS RX2; GPIO17 remained the optional GPS transmit path. GPIO26 PPS was not
connected, so this test provides no ESP32 PPS electrical or timing evidence.
Peripheral detection and decoded WSPR frames do not establish RF output level,
frequency accuracy, harmonics, spectral purity or electrical compliance.

The assigned physical button does not currently change band on either primary
platform. Manual band selection is available through the CLI and web interface.

### Arduino Nano Every

Historical source support from the original GPS firmware is retained, but active
feature work is frozen because of flash limits. Nano Every is not an official
release target.

| Function | Firmware definition |
| --- | --- |
| Button to GND | D2 |
| GPS PPS | D3, rising-edge interrupt |
| Status LED | `LED_BUILTIN`, active-high |
| GPS NMEA | `Serial1`: GPS TX → D0/RX1; D1/TX1 only if needed |
| I²C | `Wire`, normally A4/SDA and A5/SCL |

The Nano Every MCU uses 5 V logic. Confirm that every GPS, OLED and Si5351A
breakout tolerates the applied signal levels; supply-voltage labels alone are
not sufficient evidence.

### Arduino Nano 33 IoT

This is a future platform. `BoardConfig.h` has no Nano 33 IoT branch, so the
current firmware rejects it as unsupported. Its SAMD21 GPIO is 3.3 V and not
5 V tolerant. Pinout, GPS serial, PPS interrupt, LED, storage and Wi-Fi must be
defined and verified before use.

## GPS receiver

- Expected family: u-blox NEO-6M-compatible receiver.
- Protocol: NMEA at 9600 baud.
- Parsed sentences: checksummed GGA and RMC with GP or GN talkers.
- Minimum wiring: power, common ground and GPS TX to MCU RX.
- GPS RX is optional; current operation does not require commands to the GPS.
- PPS is optional and uses a dedicated rising-edge input.
- Valid UTC, valid position and PPS/UTC association are separate states.
- PPS qualification requires three consecutive periods between 900 and
  1100 ms before it can contribute `PRECISE` time. An isolated edge, a glitch,
  a missing edge, multiple unconsumed edges or incoherent NMEA UTC revokes the
  qualification and requires a complete new sequence. This digital periodicity
  check does not establish absolute PPS accuracy; signal integrity and timing
  still require measurement on the actual receiver and wiring.

Commercial NEO-6M boards differ in regulators and level adaptation. Verify the
actual module's supply and TX/RX/PPS signal levels before connection.

## Si5351A synthesizer

- I²C address: `0x60`.
- RF output: CLK0; CLK1–CLK7 remain disabled.
- Crystal load: 8 pF; initial reference: 25 MHz.
- Configured drive: 8 mA.
- Initial correction value: `76000`, not quantitatively calibrated for every
  module.

The wrapper disables all outputs around Etherkit initialization. During the
maximum five-second `PREPARING` phase it programs the real symbol 0 while both
the global output gate and CLK0 remain disabled. Only CLK0 is enabled at the
scheduled slot. Frequency-programming or runtime errors disable output and
inhibit or abort TX.

Software can verify I²C presence, initialization paths and driver calls. It
cannot prove frequency accuracy, RF level, spectral purity or the absence of
all reset transients. The Etherkit library also does not expose every individual
I²C write failure. Those properties require an oscilloscope, SDR, frequency
counter or spectrum analyzer.

## SSD1306 OLED

- Resolution: 128×64 monochrome.
- I²C address: `0x3C`.
- Software rotation: 180° (`setRotation(2)`).
- Adafruit splash disabled to reduce flash use.

OLED absence or failure does not block WSPR operation. The firmware skips
rendering when the display is unavailable or timing margin is insufficient.
No OLED writes occur during TX.

## Shared I²C bus and power

OLED and Si5351A share `Wire` at different addresses. On ESP8266 the firmware
calls `Wire.begin()` and uses the core defaults D2/GPIO4 for SDA and D1/GPIO5
for SCL. On ESP32 it explicitly calls
`Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL)`, with GPIO21 for SDA and GPIO22 for SCL.
Confirm these connections against the physical board.

Do not assume that a breakout accepting 5 V power has 5 V-safe signal pins.
Inspect regulators, translators and pull-ups. I²C pull-ups to 5 V must not be
connected directly to ESP8266, ESP32 or Nano 33 IoT GPIO.

For diagnostics, OLED and Si5351A may be intentionally disconnected. A missing
OLED only removes display output. A missing Si5351A produces
`NOT_DETECTED`/`RF INHIBITED` and prevents transmission.

## Nonvolatile storage

ESP8266 and ESP32 instantiate their storage backends in the operational
firmware. WSPR and Wi-Fi use separate versioned records, two logical slots,
sequence selection and CRC-32. Writes are read back and decoded before SAVED
state is advanced.

### ESP8266 EEPROM emulation

The backend reserves 512 bytes. WSPR and Wi-Fi occupy distinct logical regions,
but `EEPROM.commit()` updates the same underlying flash sector. A/B records can
recover logically from an incomplete or corrupt record but cannot guarantee
atomicity across every power failure. Write/read persistence and recovery from
one corrupt slot were verified on a physical NodeMCU.

### ESP32 Preferences/NVS

Namespace `wspr_cfg` contains `wspr0`, `wspr1`, `wifi0` and `wifi1`. NVS handles
physical flash entries while the common format still checks type, version,
length, sequence and CRC-32. DEVELOPMENT upload and subsequent reboot retained
the WSPR v3 record with matching ACTIVE, CANDIDATE and SAVED views on physical
ESP32 hardware. Power-loss recovery, deliberate slot corruption and every NVS
failure mode remain unverified on that platform.

### ESP32 integrated hardware check — 2026-11-05

The ESP32 DEVELOPMENT firmware served Dashboard, Configuration and JSON APIs,
returned valid gzip assets and recovered HTTP service after natural TX. Firefox
IDLE navigation passed 5/5 checks. The non-blocking writer's largest observed
submission time was approximately 6–9 ms; this observation is not a formal
worst-case bound. Full Firefox navigation coverage throughout the protected TX
windows remains pending.

Periodic ESP32 SNTP renewal produced five successful synchronizations, four of
them renewals, and reset the NTP reference age. `NTP_EXPERIMENTAL` authority
remained present at the first checkpoint beyond the former limit, at 60 minutes
26 seconds uptime. Fifteen natural frames started and all fifteen completed,
with no interrupted frame or unexpected reboot observed in that run. Recovery
from a real network or NTP-server failure and prolonged visual OLED-rotation
observation remain pending.

On 2026-11-06, a separate user-observed live DEVELOPMENT Dashboard snapshot at
188 minutes 5 seconds uptime showed NTP `SYNCHRONIZED`,
`NTP_EXPERIMENTAL` authority, an 8-minute reference age, scheduler `IDLE`, Wi-Fi
`CONNECTED`, RF inhibit `NONE`, and 47 frames started and completed with zero
interruptions. This establishes observed operation beyond three hours but is
not continuous logging of the full interval. The five successful NTP
synchronizations and four renewals above belong to the earlier session and are
not attributed to this snapshot. Real NTP-failure recovery, quantitative RF
validation and prolonged visual OLED cycling remain unverified.

The 2026-11-05 20 m on-air session for `IW2NNV` recorded 166 WSPRnet spots from
38 distinct receiving callsigns, including a maximum reported distance of
1,675 km; the last listed spot was at 11:28 UTC and the declared WSPR power was
10 dBm. These reports demonstrate decodable frames, not calibrated RF output.
Two operational stops occurred during that session. The first is consistent
with the subsequently demonstrated mismatch between the 60-minute application
reference lifetime and the former 180-minute ESP32 SNTP interval; evidence was
insufficient to assign the second stop to the same cause.

The current WSPR payload is v3 and remains able to read v1 and v2. Older WSPR
records load with Power Save OFF and migrate only on an explicit save. The
22-byte payload, 40-byte record, A/B offsets and NVS keys are unchanged. Older
firmware is not guaranteed to read a record after it has been saved as v3.
Wi-Fi records are separate and unchanged. Saving unchanged candidates avoids a
new logical record write; no credentials are stored in the repository or
printed by status interfaces.

### ESP8266 AP Setup hardware check

The controlled check of **Enter Wi-Fi Setup Mode** passed on a NodeMCU ESP8266EX
with 4 MB flash and CP2102 USB-UART, running the reviewed DEVELOPMENT firmware
baseline current for that test. The private predecessor commit identifier is
intentionally omitted. The RF chain was physically isolated from antenna and
amplifiers; no instrumental RF measurements were made.

The firmware upload succeeded. On boot, the device joined the previously saved
STA network and served the Configuration page with the new button. The tester
entered the CONFIGURATION session, pressed **Enter Wi-Fi Setup Mode** and
confirmed the request. The device rebooted, advertised its beacon AP and
served the existing AP Provisioning portal. No credentials were changed there.
After a manual RST, the device returned to the original STA network using the
preserved credentials. **Result: PASS for this ESP8266 sequence.**

This check does not establish ESP32 hardware behavior, AP continuity under all
radio conditions, EEPROM-write power-loss recovery, CLK0 electrical state
during reset, RF accuracy or WSPR timing, Factory Reset after AP Setup,
recovery from incorrect credentials, or every Wi-Fi driver error. Saving new
credentials in the portal also remains to be checked on this firmware. General
firmware hardware validation and public-release status are unchanged.

## RF chain, filtering and verification

Si5351A CLK0 is a harmonic-rich digital source, not a complete transmitter.
Before an antenna is connected, the RF chain requires:

- a low-pass or switched-filter path appropriate for the selected band;
- any buffer/amplifier correctly designed and biased;
- suitable impedance matching, connectors and load;
- dummy-load measurements of frequency, power and spectrum;
- operation within licensing and band-plan requirements.

### Evidence levels

- **Software diagnostics:** I²C detection, initialization, safe state changes,
  symbol sequencing and error paths are covered by firmware/tests.
- **Functional observation:** NodeMCU startup/OLED behavior and the
  `PREPARING → TX ON → IDLE` sequence were observed; an IC-705 showed positive
  functional RF behavior.
- **Quantitative validation pending:** calibrated frequency error/stability,
  output level, startup transients, harmonics, filter attenuation and complete
  decode performance remain to be measured with a documented setup.

Do not describe `DEGRADED (RF_UNVERIFIED)` or a firmware-completed frame as
measured RF success. Software band selection has not been physically validated
and never replaces band-appropriate filtering or the operator's regulatory
responsibility.
