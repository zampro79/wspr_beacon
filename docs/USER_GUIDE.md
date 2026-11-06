# WSPR Beacon 2.0 — User Guide

## 1. Introduction and capabilities

WSPR Beacon 2.0 is experimental firmware for an autonomous beacon using a
Si5351A, GPS receiver and optional SSD1306 OLED. It generates WSPR Type 1
messages containing a callsign, four-character Maidenhead locator and declared
power. Declared power is message data, not measured RF output.

The web interface identifies the project as **WSPR Beacon 2.0** and shows
**Developed by IW2NNV / AC2WZ** separately from the configured operating
callsign. An absent operating callsign is displayed as `Not configured`.

> **Build-profile warning:** DEVELOPMENT builds use `IW2NNV` as the default
> operating callsign and must not be distributed as public firmware. PUBLIC is
> implemented for both ESP8266 and ESP32 and starts with an empty callsign plus
> `JN45`, unless a valid persistent configuration already exists. PUBLIC remains
> TX-inhibited until a valid operating identity is saved, but this safety default
> does not by itself authorize binary distribution.

Implemented features include non-blocking WSPR scheduling, GPS/NMEA and PPS,
experimental NTP TX authorization on Wi-Fi boards, manual selection of seven
bands with fixed or per-frame randomized frequency operation, persistent
ESP8266/ESP32 configuration, OLED status pages, a serial CLI and RF fail-safe
handling.

### Operator workflow

1. Install or build the firmware using the [Installation Guide](INSTALLATION.md)
   while the RF path is safely isolated.
2. On first boot, use AP Provisioning to select or manually enter Wi-Fi
   credentials, then restart manually.
3. Open the Dashboard and inspect operating identity, time authority, RF status,
   manual permission and effective eligibility.
4. Enter Configuration explicitly before editing WSPR settings. Review ACTIVE,
   CANDIDATE and SAVED, then save/apply only the intended configuration.
5. Keep the RF path isolated until filtering, frequency and output behavior are
   appropriate and verified for the selected band.

AP Provisioning, AP Operational and normal STA are distinct modes. AP
Provisioning always disables TX. AP Operational retains the local AP but can
become eligible only with `GPS_PRECISE` and all normal safety gates.

## 2. Supported hardware and limitations

| Platform | Status |
| --- | --- |
| NodeMCU Amica V2 / ESP8266-12F | Primary development platform; build, upload, OLED, console, Wi-Fi, SNTP, persistence and functional RF behavior exercised; planned first binary-release target |
| ESP32 `esp32dev` | Primary development platform; DEVELOPMENT/PUBLIC builds and software tests available; core operation, persistence, HTTP/Web UI, SNTP renewal and natural WSPR frames partially hardware-validated; operation was observed in a live snapshot beyond three hours, while continuously logged endurance, fault recovery, quantitative RF validation and an official binary remain pending |
| Arduino Nano Every | Existing support retained; new development frozen because of flash limits |
| Arduino Nano 33 IoT | Planned, not currently supported |

Expected peripherals are a NEO-6M-compatible GPS, SSD1306 128×64 OLED at
`0x3C`, and Si5351A at `0x60`. See [Hardware](HARDWARE.md) for wiring and
electrical precautions.

For example, after discovering the actual port, upload an ESP8266 build with:

```sh
pio run -e esp8266 -t upload --upload-port /dev/ttyUSB0
```

The path above is only an example from the Linux development machine. Do not
assume it on another host. See the [Installation Guide](INSTALLATION.md) for
Linux, macOS, Windows, ESP32 and update procedures.

Do not connect an unverified RF chain or antenna merely to test firmware upload.

`BandPlan` makes 40, 30, 20, 17, 15, 12 and 10 m selectable in software; 20 m
remains the default. This support is not hardware/RF validation or regulatory
authorization. Use a suitable filter and RF chain for the selected band. A responsive read-only HTTP
dashboard is available at `/` and uses operational JSON from `/api/status`.
Experimental web configuration at `/config` provides secure sessions, WSPR
candidate editing, discard and explicit WSPR-only Save and apply.
The **Wi-Fi Setup** section offers **Enter Wi-Fi Setup Mode**. Enter
Configuration first, then confirm the request. A verified one-time marker
schedules a controlled restart into the existing AP Provisioning portal, even
if saved STA credentials are valid. The current connection closes; reconnect
to the device AP at `192.168.4.1`. The request does not erase saved Wi-Fi
credentials or change WSPR settings. If the HTTP response is lost, the restart
outcome may be uncertain; do not retry blindly. If the marker cannot be written
and verified, no restart is scheduled. AP Provisioning remains TX-inhibited.
Its existing scan, manual SSID entry, OPEN/PROTECTED selection and verified
credential save are unchanged. After a provisioning save, restart manually as
the portal instructs; the device does not automatically return to STA. This
flow is software-built for ESP8266 and ESP32. Normal saved-credential STA boot
and persistence have been exercised on ESP32; ESP32 AP Provisioning and AP
Operational behavior remain unverified.

On ESP8266 and ESP32, complete verified saved Wi-Fi credentials receive one
asynchronous station connection attempt at boot. If none are available, or if
the attempt has not connected within 15 seconds, the device enters provisioning
mode until manually restarted. It creates `WSPR-Beacon-<device suffix>` at
`192.168.4.1` with a new random eight-digit WPA2 password shown on the OLED and
serial console. Open `http://192.168.4.1/`, enter an SSID manually and supply
its password for a protected network, or explicitly select OPEN. A successful
save verifies only the Wi-Fi record; it does not
connect, restart, change WSPR settings or leave the AP. Restart manually to try
the saved network.

The OLED keeps the provisioning state, SSID, password and IP address visible
throughout AP Provisioning. Hardware Check continues internally while its OLED
page is suppressed, and its results remain available from serial diagnostics.
Controlled NodeMCU verification confirmed that the credentials page remained
visible for at least 30 seconds, the provisioning page stayed accessible at
`192.168.4.1`, and no unexpected reset occurred during a 45-second serial
observation. Hardware Check completion was not directly inspected on hardware.

`Scan for networks` starts one explicit asynchronous scan; no scan runs when the page
opens and none repeats periodically. Up to ten valid SSIDs are listed from
strongest to weakest with protected/open status. Selecting a row copies only
its SSID, while the password remains manual and the SSID field remains editable.
Hidden or unsupported SSIDs are omitted, so manual entry is always available.
Because the single Wi-Fi radio briefly visits other channels, the AP and HTTP
page may pause or disconnect during a scan; the page performs bounded polling
and tolerates temporary errors without clearing the entered SSID or password.
Scanning enables the STA interface only for radio discovery and never calls
`WiFi.begin()`, associates with a network or applies saved credentials.

Provisioning exposes its page, CSRF token, Wi-Fi scan and save endpoints, and
the AP Operational transition. The
normal dashboard and configuration API are unavailable. WSPR TX remains
structurally disabled and no missed slot is recovered. The serial command
`wifi ap` reprints the AP SSID, password and URL; mutation, RF recovery and
serial connection/scan commands are rejected while provisioning is active.
The reviewed HTTP scan action is the only scan entry point in AP mode.

`Use without Wi-Fi network` performs an explicit, protected transition to AP
Operational for the current boot. The AP, password and `192.168.4.1` remain
unchanged; no station connection, restart or persistence occurs. Dashboard and
WSPR Configuration then replace the provisioning routes. TX remains inhibited
during the transition and a resume guard prevents preparation near the next
slot. Once the guard clears, AP Operational permits normal WSPR operation only
with precise GPS/PPS time, valid configuration and operational RF. NTP, coarse,
stale or invalid time cannot authorize AP Operational TX. The normal T−7,
PREPARING/TX HTTP closure and configuration-session protections apply.
Restarting repeats the normal saved-credential STA decision unless a verified
one-time AP Setup request is pending.

In AP Operational, `wifi ap` continues to print the AP credentials on serial.
Serial Wi-Fi scan, candidate editing, connect/disconnect, configuration defaults, the
guided menu and `rf recover` are blocked to prevent loss of the AP, Wi-Fi
persistence or an RF-safety bypass. Direct WSPR configuration and the web
WSPR-only Save and apply flow retain their existing session protections. OLED,
serial status and Dashboard report the effective waiting, inhibited, ready,
PREPARING and transmitting state. `wifi ap` remains the explicit serial path
for recovering the AP credentials. Operational HTTP APIs never expose the AP
password.

Controlled NodeMCU verification confirmed saved-credential STA boot, fallback
to the latched AP when that network was unavailable, smartphone access to this
page, manual scanning and SSID selection. The AP remained active when the
original network returned, and a later manual restart restored STA operation.
WEB-004C additionally confirmed the explicit AP Operational transition,
Dashboard and Configuration navigation, Configuration-session heartbeat and
Exit without saving. The AP remained active through the transition and after
the original network returned; a later manual restart restored STA operation.
With no qualified GPS/PPS, Dashboard showed `TX RESUME GUARD`, time
`NONE / UNKNOWN`, TX authority `NONE`, `TIME_UNAVAILABLE` and no completed
frames, as expected. No new Wi-Fi credentials or WSPR settings were saved.
Real AP Operational TX, GPS/PPS loss behavior, the T−7 gate during real TX,
instrumental RF-silence and RF-quality measurements and detailed heap/watchdog
margins remain unverified. ESP32 validation of these AP-specific paths also
remains pending.
The stable pre-multiband baseline remains historical evidence from the
predecessor project and is not a resolvable Git reference in this repository.

The preliminary HTTP service accepts one local STA client at a time and normally
closes connections before the RF-critical window. It is intentionally
unavailable from approximately T−7 s through `PREPARING` and TX, then resumes
when RF is safe. During an active configuration session it remains available
across nominal slot boundaries because TX admission is already inhibited, but
actual `PREPARING` or TX always closes it.

`GET /api/status` returns experimental schema version 1 with `identity`, `tx`,
`time`, `network`, `gps`, `modules`, `operating` and `system` objects. The
additive `operating` object reports `NORMAL`/`CONFIGURATION`, CLI/web ownership,
mode revision, resume guard and effective TX admission; it never exposes a
session identifier or CSRF token. Frequencies are
integer hertz; durations and ages are milliseconds; scheduled slots are UTC
milliseconds from the start of day. Unavailable values are JSON `null`.
Configured, prepared/current-frame and last-frame frequencies are separate;
in RANDOM mode the selected frequency remains `null` until a frame has been
prepared. The endpoint reports local IPv4 and optional free heap but never
SSID, password or stored credentials. Responses use `Cache-Control: no-store`,
`Connection: close` and a fixed maximum body size of 2048 bytes. Snapshot
collection occurs only for this route while the existing RF-safe gate is open.

Open `http://<device-ip>/` on the same local network to view identity, TX state,
next slot and estimated countdown, frequency, last-frame telemetry, time, GPS
receiver detail, network, RF readiness and expandable module diagnostics. The
self-contained page works from 320 px wide screens, follows the browser light/dark preference
and loads no external assets. It requests schema-v1 data every three seconds,
with one bounded request at a time. After roughly six seconds without a fresh
snapshot it marks retained data `STALE`; after a prolonged interruption it
shows `UNAVAILABLE`. A locally decreasing countdown is only an estimate from
the last eligible scheduler snapshot and never controls a TX transition. Near
the slot, the browser may show `RF window approaching`, `Preparing expected`,
`TX expected` and `Awaiting telemetry`. During the nominal 110.6-second WSPR
window it shows estimated time remaining and a progress bar because HTTP is
intentionally suspended. These browser-only indications use a monotonic clock:
they do not prove that RF started or completed, do not calculate a later slot,
and are discarded after timer discontinuities or when RF/time is unqualified.
The next successful snapshot immediately restores authoritative status.
If repeated status requests fail, the Dashboard makes three bounded fast
recovery attempts and then continues retrying every 30 seconds; it no longer
stops polling at that point. Returning to a tab after a long browser-timer
suspension restarts recovery promptly, except that a still-current estimated TX
window continues to suppress requests. Controls remain disabled until a fresh,
validated status snapshot is accepted.
Unavailable API values appear as `--`. In RANDOM mode the primary frequency
label remains `RANDOM`; configured and last-frame frequencies remain separate.
The frequency card reserves `Last frame` for the immutable transmitted WSPR
message. It shows the callsign, locator and declared dBm value frozen for the
most recent frame that actually started; it remains `--` before the first frame.
`Last frequency` comes from the same authoritative last-frame telemetry. A
firmware-completed or interrupted result preserves that identity, but does not
prove that RF was radiated or received. The dedicated GPS
card distinguishes NMEA reception, position fix, satellites, PPS presence and
PPS association; coordinates are shown only for a current valid fix. Its
expandable details reuse the GPS/PPS diagnostics and effective locator source.
`COMPLETED_BY_FIRMWARE` is displayed as `Completed`, which still does not
confirm reception by another station.

The dashboard shows `CONFIGURATION — TX DISABLED` while a session owns the
configuration mode and `TX RESUME GUARD` after the last owner exits until safe
admission resumes. In both states the browser suppresses its estimated TX
countdown and continues normal three-second polling while HTTP is available.

The primary navigation keeps the current page as a keyboard-accessible,
non-navigating control, so selecting Dashboard from Dashboard or Configuration
from Configuration never reloads the page or sends an HTTP request. The
cross-page controls remain ordinary links with the RF-window protections
described below. The dashboard links to the experimental configuration page at
`/config`. That
page polls `GET /api/config/v1` and `GET /api/config/v1/session` serially and
displays the authoritative ACTIVE, CANDIDATE and SAVED WSPR views, revisions,
dirty/runtime state and session owners. Merely opening the page does not
acquire a configuration session, inhibit TX, fetch a CSRF token or change
configuration.

`Enter Configuration` is an explicit action. It obtains the per-boot CSRF
token, creates a browser-RAM-only 128-bit owner identifier and requests the
existing authoritative web session. Only a confirmed session shows
`CONFIGURATION — TX DISABLED` and enables `Exit Configuration`. A heartbeat is
sent about every five seconds. After a transient heartbeat failure, ownership
is marked uncertain and session-dependent mutation controls remain disabled.
The page preserves the same RAM-only owner identifier and makes at most two
authenticated renewal attempts, after approximately 1.5 and 3 seconds. A
successful renewal restores confirmed ownership and the normal heartbeat; an
expired, invalid or unrecoverable session requires an explicit new Enter.
Read-only `web_active` status alone is never treated as proof that this browser
owns the session. Explicit Exit remains available during bounded recovery.
The page never takes over another browser's owner and does not automatically
reacquire an expired session. Exit releases only this browser owner, preserves
any CLI owner and may show `TX RESUME GUARD`; it never saves or discards the
candidate and never forces an immediate TX. Dashboard navigation first tries
to release the session and warns if release is uncertain, because unattended
ownership can otherwise remain until the 25-second expiry. Browser unload
cleanup is best-effort only.

With confirmed browser ownership, the WSPR form becomes editable. Its band,
frequency, power, interval and numeric limits come from the authoritative API
capabilities; only TX-enabled bands are offered. Local edits are distinct from
the server CANDIDATE and are not applied or saved. `Update candidate` validates
and submits the complete form atomically to RAM, while preserving ACTIVE and
SAVED. `Reset form` only restores the last confirmed CANDIDATE in the browser
and sends no mutation. `Discard changes` requires confirmation and restores the
server CANDIDATE from SAVED without a persistent write or ACTIVE change. If
session ownership becomes uncertain, all mutation controls are disabled while
local form values are retained.

`Save and apply` becomes available only after local edits have first been
staged with `Update candidate`, while this browser still has confirmed
ownership and TX remains inhibited. It requires explicit confirmation, then
persists and applies only the authoritative WSPR CANDIDATE; it never changes
the Wi-Fi candidate or record and leaves CONFIGURATION active. An unchanged
result is reported without claiming a flash write. If the response is lost or
an error occurs, the page does not retry: it rereads ACTIVE, CANDIDATE and
SAVED to determine the authoritative outcome. A persisted-but-not-applied
result is shown as a partial failure, preserving the distinction between SAVED
and ACTIVE. Exit remains a separate explicit action.

The serial summary includes a compact `[HTTP]` diagnostic section. `Gate`
reports `OPEN` or `CLOSED` and the lifecycle reason, while `Listener` reports
whether the firmware intends the listener to be active. Counters cover accepted
clients, served responses, request/response timeouts, failed requests and forced
closures. A response is counted as served when all bytes have been submitted to
the framework TCP API; this does not prove that the browser received or
acknowledged them. As with all console output, this section is not printed during
`PREPARING` or TX.

The transport uses ordinary `WiFiServer` and `WiFiClient` APIs on both ESP8266
and ESP32, fixed request buffers and incremental input/output. Request reception
retains a 600 ms total limit and a 200 ms inactivity limit. Response submission
uses absolute implementation limits: 5 s for JSON and 15 s for static HTML.
There is no shorter inactivity abort during response transmission, allowing
the TCP stack to recover from temporary loss and retransmission stalls within
the absolute deadline. These limits are not user-configurable. The T−7 RF gate
overrides every HTTP deadline and terminates an active transfer before the
critical interval. On ESP8266, flash-resident dashboard data is copied through
a reusable 536-byte staging buffer and submitted at most once per main-loop
service step; the full page is never copied into RAM. The listener backlog is
limited to one where the framework supports it. HTTP is intentionally
best-effort: an occasional failed request is acceptable, but it must recover
without affecting WSPR scheduling.

Browsers that explicitly advertise gzip support receive deterministic,
precompressed Dashboard and Configuration assets directly from program flash.
Clients that omit gzip support, advertise only unsupported encodings or reject
gzip receive the original identity representation. JSON API responses are not
compressed. This is an internal transfer-size optimization; routes, page
content, configuration behavior and RF gating are unchanged. Gzip is the
preferred static-page delivery path. Because HTTP is experimental and
best-effort, the larger identity representation is not guaranteed to complete
under adverse TCP conditions; retry the page after the RF-critical interval or
after a transient network failure.

## 3. First startup and Hardware Check

After successful OLED initialization, a radio-tower banner displays for five
seconds with `IW2NNV-AC2WZ`, `WSPR Beacon 2.0` and `(2026)`. The primary D4 LED
is steadily on while the D0 time-authority LED remains off. GPS, Wi-Fi, NTP,
watchdog and console continue running, but TX is inhibited. This startup gate
also runs without an OLED, and missed slots are not recovered.

`HARDWARE CHECK` then tests these items sequentially:

| Item | `OK` criterion | Timing |
| --- | --- | --- |
| `OLED` | Existing initialization succeeded | Minimum 250 ms |
| `GPS` | Valid NMEA received; no satellite fix required | 10 s timeout |
| `Si5351A` | Existing diagnostic is operational/degraded | 1 s timeout |
| `LED` | Three software-controlled flashes completed | 600 ms |

Each row changes from `WAIT` to `OK` or `FAIL`; results remain for three
seconds. LED `OK` confirms the software sequence, not emitted light. A failed
module does not stop the remaining checks.

## 4. OLED screens and LED indications

The GPS/Wi-Fi/Time page resembles:

```text
GPS: FIX SAT:08
UTC: 12:34:56 NTP *
45.464N 009.190E
LOC: JN45 (GPS)
WiFi: CONNECTED (STA)
```

`SEARCH`, `FIX` and `LOST` mean no fix yet, current fix and lost previous fix.
Retained coordinates are marked `OLD`; unavailable coordinates are explicit.
UTC sources are `GPS`, `NTP` or `---`: `*` means TX-qualified, `!` means time
available but unqualified, and `-` means absent. Locator origins are `DEF`
(configured), `GPS` (live fix) and `LAST` (last valid GPS locator).

Wi-Fi states are `CONNECTING`, `CONNECTED`, `DISCONNECTED`, `RECONNECTING` and
`OFF`. After a station connection, `CONNECTED` appears for three seconds and is
then replaced by the IPv4 address. `AP` is not currently used.

With qualified time, this page alternates every five seconds with IDLE:

```text
MSG: IW2NNV JN45 23
FREQ: 14.097100 (20m)
RF: READY
NEXT TX: 12:36 UTC
REMAINING: 42s
```

`RF` is `READY`, `INHIBITED` or `ERROR`. Missing slot data appears as `--`.
In RANDOM mode the frequency line is `FREQ: RANDOM (20m)`, never the previous
frame frequency. Loss of time qualification immediately returns to the first
page; recovery restarts rotation from it.

At about T−5 s, `PREPARING` shows the frozen message and selected frequency
while RF is OFF. The 162 symbols are encoded and symbol 0 is programmed with
CLK0 disabled. At about T−1 s, the display changes once to `TX ON`: this appears
approximately one second before actual RF emission, not when RF is enabled.
Rendering is attempted only with at least 500 ms margin; otherwise it is skipped
without delaying TX. At the scheduled slot CLK0 is enabled for symbol 0. `TX ON`
remains static with no OLED writes throughout TX. Operational pages return
after completion or abort.

During configuration mode the OLED instead shows `CONFIGURATION`,
`TX DISABLED` and `SESSION: CLI`, `WEB` or `BOTH`. After final release it shows
`TX RESUME GUARD` until a qualified, RF-ready future slot with sufficient
margin is available. Startup, Hardware Check and actual RF-critical states keep
their existing priority; an RF fault remains visible instead of being hidden by
the configuration page.

```text
WSPR BEACON             WSPR TRANSMITTING
   PREPARING                  TX ON
IW2NNV JN45 23           IW2NNV JN45 23
14.097100 MHz            14.097100 MHz
```

| LED condition | Pattern |
| --- | --- |
| Startup | Steady on for five seconds |
| Hardware Check | Three 100 ms flashes |
| Waiting for time | One short flash every 2 s |
| Ready | One short flash every 3 s |
| Wi-Fi activity | Two short flashes every 2 s |
| `PREPARING` | 150 ms on / 150 ms off |
| TX | Steady on |
| Critical RF error | Three short flashes every 2 s |

On ESP8266 NodeMCU, both onboard indicators are active-low. D4/GPIO2 retains
the primary patterns above. D0/GPIO16 indicates the effective qualified TX time
authority: off without qualified authority, a slow 1 s on / 1 s off blink for
experimental NTP authority, and steady on for GPS/PPS authority. Both LEDs run
the three-flash Hardware Check; D0 remains off during the startup banner and is
frozen throughout `PREPARING` and TX. Other platforms retain their existing
single status LED.

## 5. USB serial console

The console uses **9600 baud**:

```sh
pio device list
pio device monitor --port /dev/ttyUSB0 --baud 9600
```

The port may differ. Disable terminal local echo; firmware echo supports
Backspace, DEL, CR, LF and CRLF. Password entry is completely blind. Lines over
95 characters produce `ERR LINE_TOO_LONG`. During `PREPARING`/TX all console
output is suppressed; a line begun then is discarded and later reported as
`ERR INPUT_DROPPED_DURING_TX`.

## 6. MONITOR and INTERACTIVE modes

The console starts in `INTERACTIVE` with periodic reports disabled and `wspr> `
available for commands. This RAM-only choice is restored after every reboot.
Any input byte other than CR or LF received in a timing-safe window also selects
`INTERACTIVE` before the command line is complete. Command responses remain
available, and `wspr> ` is shown whenever the console is ready for a new line.
The selected mode survives TX and does not change after inactivity.

- `monitor on` changes `INTERACTIVE → MONITOR`, enables periodic reports and
  hides the interactive prompt.
- `monitor off` changes `MONITOR → INTERACTIVE` and restores the prompt.
- `status` prints one full snapshot when timing is safe.

When `MONITOR` is explicitly enabled, reports retain their nominal five-second
interval but may be briefly deferred while an HTTP client is active. HTTP is
serviced before an optional periodic report; interactive command responses are
not deferred. Console and HTTP output remain suppressed by their existing
`PREPARING`/TX protections.

## 7. Complete CLI reference

Commands are lower-case and must follow these forms.

| Syntax | Purpose / restriction |
| --- | --- |
| `help` | Show firmware help, including how frequency-mode changes are persisted. |
| `menu` | Open the numeric guided menu; exit preserves candidates. |
| `config` | Enter RF-safe CLI configuration mode, inhibit new frames and show its command menu. |
| `exit` / `config exit` | Leave CLI configuration mode without saving or discarding candidates. |
| `status` | Full status snapshot. |
| `status gps` | GPS, fix, satellites and PPS. |
| `status time` | Selected time, quality, age and TX authority. |
| `status rf` | RF state, frequency, slot and inhibition. |
| `status wifi` | Wi-Fi/NTP detail without passwords. |
| `status modules` | GPS, PPS, Si5351A, OLED, Wi-Fi and NTP diagnostics. |
| `tx status` | Current TX state, inhibit reason and counters. |
| `tx last` | Last frame that actually started, with slot, source, locator, frequency and result. |
| `tx off` | Latch the RAM-only manual inhibit and disable RF output. |
| `tx on` | Remove only the manual inhibit in NORMAL or AP Operational mode. |
| `monitor on` / `monitor off` | Enable/disable periodic reports. |
| `wspr show` | Show WSPR CANDIDATE, band, mode and dirty state. |
| `wspr set callsign CALL` | Set candidate Type 1 callsign. |
| `wspr set locator GRID` | Set candidate four-character locator. |
| `wspr set band NAME` | Set candidate band and atomically reset its base to that band's nominal frequency. |
| `wspr set power DBM` | Set candidate declared power. |
| `wspr set interval MIN` | Set even interval from 2 to 60 minutes. |
| `wspr set frequency HZ` | Set candidate base frequency within the selected band's advertised window. |
| `wspr set offset HZ` | Set −100 to +100 Hz offset. |
| `wspr set correction VALUE` | Set Si5351A correction from −1,000,000 to +1,000,000. |
| `wspr frequency mode fixed` | Select FIXED for ACTIVE and CANDIDATE when RF-safe. |
| `wspr frequency mode random` | Select RANDOM for ACTIVE and CANDIDATE when RF-safe. |
| `config show` | Show CANDIDATE, SAVED, ACTIVE, origins and payload/migration state. |
| `config discard` | Restore CANDIDATE from SAVED/defaults; ACTIVE unchanged. |
| `config defaults` | Load default WSPR and empty Wi-Fi candidates; no flash write. |
| `config save` | Validate, persist, verify and safely apply candidates on ESP8266/ESP32. |
| `wifi status` | Show connection and candidate credential presence. |
| `wifi ap` | During provisioning, reprint the AP SSID, password and URL. |
| `wifi scan` | Start a non-blocking scan. |
| `wifi ssid NETWORK NAME` | Set candidate SSID; spaces are accepted. |
| `wifi password` | Read the password blindly on the next line. |
| `wifi connect` | Connect with RAM candidates without saving. |
| `wifi disconnect` | Disconnect and disable Wi-Fi. |
| `rf recover` | Outside `PREPARING`/TX, disable CLK0 and repeat Si5351A checks. |

Wi-Fi commands are operational only on ESP8266/ESP32. Valid power codes are
`0, 3, 7, 10, 13, 17, 20, 23, 27, 30, 33, 37, 40, 43, 47, 50, 53, 57, 60`.
Callsigns are supported 3–6 character WSPR Type 1 callsigns; locators range
from `AA00` through `RR99`. Accepted band names are `40m`, `30m`, `20m`, `17m`,
`15m`, `12m` and `10m`. Each band has its own nominal ±100 Hz window advertised
by the configuration API, and the complete four-tone set, including offset,
must fit inside the selected window.

Manual TX permission is RAM-only and defaults to `TX ON` after every restart.
This is operator permission, not effective TX eligibility: identity, RF, time,
boot-mode, configuration-session, resume and T−7 gates still apply. `tx off`
immediately cancels a prepared frame or aborts an active frame when the serial
command is actually processed, disables RF output and prevents automatic
resumption for the current boot. The manual inhibit is temporary rather than a
persistent safety lockout; explicit `tx on` or a reboot restores permission,
without forcing immediate preparation or recovering a missed slot. Serial input
is deliberately discarded during PREPARING/TX, so a CLI command cannot
interrupt those periods under the current deterministic console policy.

Dashboard status, `tx status` and the immediate TX ON/OFF HTTP response use one
read-only effective-eligibility calculation. It requires manual permission, an
RF-capable operating mode, a configured identity, ready RF hardware, no active
Configuration session, no resume guard and qualified time. AP Operational
specifically requires `GPS_PRECISE`; NORMAL retains its existing qualified-time
policy. Eligibility describes readiness for a future automatic slot, not the
current transmitter state or a promise that a frame will start. `IDLE`,
`RF_INHIBITED`, `PREPARING` and `TRANSMITTING` remain separate state reporting.
Values that previously differed between these interfaces are therefore corrected
diagnostics; RF admission and execution behavior are unchanged. This reporting
alignment has not yet been validated on hardware.

The Dashboard exposes the same shared control through Host/Origin/CSRF-protected
requests. HTTP is closed during PREPARING and TX; consequently web TX OFF acts
immediately only when its request can be received and processed and cannot
interrupt an RF-critical period. No request is queued. TX ON is rejected in AP
Provisioning and provisioning-error modes; AP Provisioning remains inhibited
independently of manual state. Manual control performs no persistent write.

The firmware supports an unconfigured operating identity. Dashboard and CLI
show `Not configured`, the Configuration callsign field is empty and the OLED
shows `NO CALLSIGN`. These are presentation labels, not valid WSPR callsigns.
While the callsign is unconfigured, the dedicated
`OPERATING_IDENTITY_UNCONFIGURED` reason inhibits scheduler admission, message
preparation and RF output regardless of GPS/PPS or NTP status. Entering a
callsign only stages a candidate; the complete candidate must still pass normal
validation and be explicitly saved/applied. DEVELOPMENT defaults to `IW2NNV`
with locator `JN45`. PUBLIC starts with an empty callsign and locator `JN45`, so
the user must configure and explicitly save a valid operating identity before
TX can become eligible.

Enter `config` before using `wspr set`, `wspr frequency mode`, `config save`,
`config discard` or `config defaults`; outside configuration mode these
commands return `ERR CONFIG_MODE_REQUIRED`. The prompt changes from `wspr>` to
`config>` only while the CLI owns a configuration session. `exit`,
`config exit` or `0` leaves that session, preserves candidate edits and starts
the safe TX-resume guard; it never saves automatically.

Ordinary `wspr set` commands change CANDIDATE only. Mode selection also changes
ACTIVE immediately when RF-safe and otherwise returns
`ERR RF_BUSY MODE_UNCHANGED`. `rf recover` success proves software checks only,
not RF output quality.

Factory Reset requires an exclusive Configuration session. On serial, enter
`factory reset`, then type the exact separate phrase `FACTORY RESET` within 30
seconds; `cancel`, a different phrase, a timeout or buffered confirmation
cancels the request. On `/config`, use the separate **Reset WSPR and Wi-Fi**
section and accept both confirmations. The operation removes WSPR settings and
saved Wi-Fi credentials, reports `Not configured`, restores `JN45`, 20 m and
the nominal 20 m frequency, and keeps TX inhibited. It does not change the
current network mode or reboot automatically. Restart manually to enter Wi-Fi
provisioning. If the HTTP response is lost, do not retry: the outcome is unknown,
so restart and verify. HTTP and serial commands remain unavailable during
PREPARING/TX. Fresh development storage still defaults to `IW2NNV/JN45`; boot
recovery completes an interrupted reset or remains fail-closed.

After a completed reset, Wi-Fi and the WSPR operating identity may be
configured independently. A verified Wi-Fi provisioning save becomes usable
after the required manual reboot while the WSPR identity remains unconfigured
and TX remains inhibited. The completed-reset marker remains present until a
valid WSPR identity is explicitly persisted; saving Wi-Fi alone never clears
it. Controlled NodeMCU verification confirmed recovery of an already saved
post-reset Wi-Fi record and successful STA startup while the WSPR identity and
TX remained inhibited. That test did not perform a new Wi-Fi credential save;
the equivalent post-reset recovery path remains unverified on ESP32.

The guided `menu` exposes WSPR and Wi-Fi candidates, GPS/time status, Si5351A
calibration, module/general status, save and discard actions. Enter the listed
number followed by CR or LF. `0` returns from a submenu; leaving the menu keeps
candidate edits, while its explicit discard action removes them. Password
entry remains blind and all normal `PREPARING`/TX restrictions apply.

The WSPR submenu includes `Frequency mode`. Select `FIXED` or `RANDOM`; the
menu shows both ACTIVE and CANDIDATE modes and applies the same RAM-only change
as `wspr frequency mode fixed|random`. SAVED remains unchanged until an explicit
`config save`. Selecting RANDOM does not choose a frequency immediately: the
per-frame frequency is still selected only when that frame enters `PREPARING`.

## 8. WSPR configuration and persistence

The experimental `POST /api/config/v1/save` endpoint exposes the guarded
WSPR-only save operation to the owning web configuration session. Its bounded
JSON body contains the current `expected_revision` and the exact confirmation
`"confirm":"save_wspr"`; it also requires the same Host, Origin, CSRF and
`X-WSPR-Session` protections as other web mutations. It persists and applies
WSPR only—never Wi-Fi—and performs no automatic save. The existing CLI
`config save` command keeps its combined WSPR and Wi-Fi behavior.

| View | Meaning |
| --- | --- |
| `CANDIDATE` | Editable RAM values, not necessarily applied or saved |
| `ACTIVE` | Values used for future frames |
| `SAVED` | Last verified persistent values |

`config save` validates both WSPR and Wi-Fi candidates. It writes only the
record type whose candidate differs from SAVED (plus a pending WSPR v1/v2→v3
migration), then reads each written record back for verification. An unchanged
Wi-Fi candidate therefore causes no Wi-Fi-record write. After a fully successful
save, changed WSPR settings become ACTIVE between frames; changed complete
Wi-Fi credentials become ACTIVE and start a connection attempt. Empty changed
Wi-Fi credentials become ACTIVE and disconnect Wi-Fi. A failed or partial save
is reported and does not apply the combined candidate set to ACTIVE; on a
partial result, one independently verified record may already have advanced in
SAVED while the other failed. Inspect `config show` and do not repeat writes
blindly. A prepared frame is immutable.

The CLI now routes these operations through one shared configuration service,
so CANDIDATE, ACTIVE and SAVED remain authoritative for future interfaces as
well as serial commands. The experimental `GET /api/config/v1` endpoint exposes
those three WSPR views, the RAM-only revision, dirty/migration state and compact
form capabilities as bounded JSON. It never includes Wi-Fi credentials and
does not modify or save configuration. `PUT /api/config/v1/candidate` can stage
one complete WSPR CANDIDATE and `POST /api/config/v1/discard` restores only the
WSPR CANDIDATE from SAVED. Both require the current revision, exact local-IP
Host and Origin headers, and a per-boot CSRF token from
`GET /api/config/v1/token`. They are rejected outside an RF-safe IDLE window,
never change ACTIVE or SAVED, never touch Wi-Fi candidates, and never write
flash. The token limits cross-origin requests but does not authenticate an
arbitrary client already connected to the local network. The save response
distinguishes `saved_applied`, `unchanged`, stale/session/RF rejections and
persistence, verification or runtime-application failures. A
`persisted_apply_failed` result means SAVED may have advanced while ACTIVE was
restored or remains unsynchronized; reread `GET /api/config/v1` before acting.

The firmware configuration-session controller inhibits admission of new WSPR
frames while the CLI configuration session is active. An already admitted
PREPARING/TX frame is never interrupted; entry is rejected until it finishes.
Console status, schema-v1 JSON, OLED and dashboard reflect the authoritative
operating mode and post-session resume guard. The experimental HTTP API offers
`GET /api/config/v1/session` plus protected `enter`, `heartbeat` and `exit`
operations under `/api/config/v1/session/`. A browser supplies its own
128-bit lowercase hexadecimal owner identifier; the identifier is never
returned by read-only telemetry. The web owner expires after 25 seconds without
a matching heartbeat. CLI and web ownership may coexist, and releasing either
one does not release the other.

Candidate update, discard and save requests require the active browser owner through
`X-WSPR-Session` in addition to the existing exact Host, same-origin Origin and
per-boot CSRF token checks. These controls are intended for a trusted LAN and
are not user authentication. If the client disconnects before the complete
request or security validation, no save
occurs. Once validated execution begins, persistence may complete even if the
HTTP response is lost; response delivery is not part of the flash transaction.
The client must reread `GET /api/config/v1` and must not retry blindly.

ESP8266 and ESP32 keep separate WSPR and Wi-Fi records with two logical slots,
sequence selection and CRC-32. WSPR payload v3 stores band, frequency mode and
Power Save without changing the 22-byte payload or 40-byte record. V1 and v2
records remain readable with Power Save OFF and are not rewritten at boot; the
next explicit save migrates them. Downgrade to older firmware is not guaranteed
after saving v3.

Power Save is OFF by default. Set the candidate with
`wspr set power-save on|off`, inspect it with `config show`, and use the normal
explicit save flow. The Configuration page exposes the same OFF/ON candidate.
Candidate edits alone do not affect the LEDs or OLED.

When Power Save is ON, firmware-controlled LEDs remain off at startup, retain
their normal flashes during Hardware Check, and are forced off afterward while
their diagnostic controllers continue to run. The OLED still shows the startup
banner and Hardware Check, then keeps the STA IPv4 address visible for 15
cumulative seconds, AP Provisioning or AP Operational information for 15
cumulative seconds, or AP start error for 10 cumulative seconds. Time is counted
only while that presentation is actually visible and pauses while another page
or PREPARING/TX replaces it. The display then receives `DISPLAYOFF`; rendering
and OLED I2C work stay suppressed. Disabling Power Save sends `DISPLAYON` in a
safe non-RF-critical interval and redraws current information without repeating
the startup presentation. No CPU sleep, modem sleep or Wi-Fi disconnect is used.
If STA reports connected without a usable IPv4 string, the firmware leaves the
OLED on rather than inventing completion of the required IPv4 presentation.

`config defaults` loads default WSPR values and an empty SSID/password into
CANDIDATE only. Existing SAVED and ACTIVE Wi-Fi credentials remain usable until
`config save`. Saving those empty candidates writes and verifies a newer empty
Wi-Fi record, which logically replaces the saved credentials and disconnects
Wi-Fi; it does not erase the entire EEPROM/NVS area. `config discard` before the
save restores the existing saved credentials. Conversely, saving WSPR-only
changes leaves an unchanged Wi-Fi record untouched. `wifi connect` uses RAM
candidates and never persists them by itself.

## 9. FIXED and RANDOM frequencies

FIXED uses the selected band's configured base plus integer-Hz offset for tone
zero. RANDOM selects tone zero once per frame, during `PREPARING`, from nominal
−90 through +89 Hz for the selected band. On 20 m this is
**14,097,010–14,097,189 Hz**. The value is frozen for the frame and shown on
`PREPARING`/`TX ON` and in last-frame telemetry.

The selected BandPlan nominal frequency, selected tone-zero
frequency, and four modulated tones are distinct. Tone spacing is approximately
1.46 Hz. Validation includes tone zero and the highest tone, keeping all four
inside the permitted window. RANDOM does not affect scheduling or rotate bands.

Example: select RANDOM and persist it, then later return to FIXED and persist
that choice:

```text
config
wspr frequency mode random
config show
config save

wspr frequency mode fixed
config show
config save
exit
```

Run these commands only outside `PREPARING` and TX. `config show` lets you
confirm ACTIVE, CANDIDATE and SAVED before each write.

## 10. GPS/PPS, experimental NTP and location

GPS is preferred. GPS with qualified, UTC-associated PPS and `PRECISE` quality
has priority. Qualification requires three consecutive approximately one-second
PPS periods; an isolated or irregular edge cannot promote GPS to `PRECISE`, and
loss of the sequence immediately falls back to another qualified authority or
inhibits TX. This qualification checks periodicity and association, not absolute
timing accuracy. Time may be valid without a position fix.

On Wi-Fi boards, confirmed, fresh, `SYNCHRONIZED` NTP may authorize TX when
precise GPS/PPS is unavailable and is reported as `NTP_EXPERIMENTAL`. This is
implemented but not a quantitative timing guarantee. Wi-Fi connection or a
plausible system date alone is insufficient.

Without qualified time, new TX is inhibited. Qualification arriving after a
slot waits for the next eligible slot; missed slots are never recovered.
Locator selection is independent: `FIXED` before a fix, `GPS_LIVE` with a
current fix, and `GPS_LAST_KNOWN` after loss. Last-known data stays in RAM
without timeout or automatic persistence.

## 11. Diagnostics and troubleshooting

Module states are `UNKNOWN`, `DISABLED`, `NOT_DETECTED`, `INITIALIZING`,
`OPERATIONAL`, `DEGRADED` and `ERROR`. `DEGRADED (RF_UNVERIFIED)` means the
Si5351A passed available software checks but RF is not quantitatively verified.
`COMPLETED_BY_FIRMWARE` means all 162 symbols were processed without a detected
software error, not that a signal was radiated or decoded.

At boot, `*_CONFIG_SLOT_REJECTED_RECOVERED_SAVED` means one present A/B record
was rejected but another valid SAVED record was recovered and selected. It does
not report loss of the active saved configuration. In contrast,
`*_CONFIG_NO_VALID_RECORD_USING_DEFAULTS` means a present record was rejected
and no valid record for that domain remained. Fresh storage with absent slots
does not produce either warning. Factory Reset fail-closed retains its separate
error diagnostic. The firmware does not automatically rewrite a rejected slot.

- No serial port: check data cable, CP2102 driver and permissions.
- Port busy: close other monitors.
- NodeMCU will not boot: do not hold D3/GPIO0 low at reset/power-up.
- `WAIT TIME`: inspect `status time`, GPS/PPS and Wi-Fi/NTP.
- OLED/Si5351A absent: check power, common ground, SDA/SCL and `0x3C`/`0x60`.
- For diagnostic tests, OLED and Si5351A may be intentionally disconnected.
  Missing OLED only removes display output; missing Si5351A always keeps TX
  inhibited (`NOT_DETECTED` / `RF INHIBITED`).
- Wi-Fi failure: inspect `wifi status` and `config show`; never log passwords.
- `RF INHIBITED`: inspect `status rf` and `status modules` before recovery.

## 12. RF safety and verification status

The sequence is `IDLE → PREPARING (RF OFF) → TRANSMITTING → IDLE`. Symbol 0 is
preprogrammed with output gates disabled; CLK0 is enabled only at the armed
slot. Symbols 1–161 use absolute deadlines at approximately **682.667 ms** per
symbol. Error and abort paths prioritize disabling RF.

NodeMCU operation, OLED sequencing and functional RF observation with an
IC-705 were successful. They do not replace quantitative measurement of
frequency error, power, startup transients, harmonics or filter attenuation.
Si5351A CLK0 is harmonic-rich: use a suitable filter for the selected band, legal
operating privileges, and preferably a dummy load and test equipment before an
antenna. Never infer actual RF power from the dBm encoded in the WSPR message.

## 13. Firmware updates

Use the [Installation Guide](INSTALLATION.md) to identify the correct target,
port, artifacts and offsets. An ordinary update must not start with a full flash
erase: ESP8266 configuration resides in a separate emulated EEPROM sector, and
ESP32 configuration resides in NVS. ESP32 application-only updates additionally
require a compatible bootloader and partition table.

Before an update, record the current identity, band, frequency mode and network
state and isolate the RF path. Afterward, read those values again and confirm
effective TX inhibition before reconnecting RF hardware. Do not assume that a
successful upload proves configuration preservation. WSPR payload v3 remains
able to read v1/v2 records, but downgrade after explicitly saving v3 is not
guaranteed.

No official precompiled release package currently exists. Current development
builds contain the `IW2NNV` callsign default and are not public firmware.

## 14. Known limitations

- Original modern project material is GPL-3.0-or-later; no public-release
  package or binary distribution has been approved.
- PUBLIC is implemented for ESP8266 and ESP32 with an empty default callsign;
  valid SAVED configuration still takes precedence. No public binary distribution
  is currently approved.
- ESP32 hardware validation is partially complete for DEVELOPMENT upload and
  readback, saved-credential STA boot, WSPR v3 persistence, detected project
  peripherals, HTTP/Web UI, periodic SNTP renewal beyond 60 minutes and natural
  WSPR frames. A separate live Dashboard snapshot at 188 minutes 5 seconds
  uptime showed NTP `SYNCHRONIZED`, `NTP_EXPERIMENTAL` authority with an
  8-minute reference age, scheduler `IDLE`, Wi-Fi connected, RF inhibit `NONE`,
  and 47 frames started/completed with zero interruptions. The snapshot is not
  continuous logging of the full interval and does not establish how many NTP
  renewals occurred in that session. Real NTP failure recovery, prolonged OLED
  observation, complete protected-window browser navigation and quantitative
  RF validation remain pending. No official ESP32 binary is approved.
- Nano Every is historical/frozen and Nano 33 IoT is unsupported.
- AP Operational transmission with qualified GPS/PPS remains unverified on
  hardware.
- macOS and Windows installation procedures have not been hardware-verified by
  this project.
- New-credential saving was not exercised during the most recent post-reset
  recovery verification.
- Detailed heap/watchdog margins and instrumental proof of RF silence during
  provisioning remain pending.
- Seven-band software support has not been accompanied by final multiband RF,
  filter, spectral or power validation.
- Band selection is available through CLI/web; changing band through the
  physical button is not implemented.
- HTTP is experimental, single-client and intentionally unavailable during
  RF-critical intervals.
