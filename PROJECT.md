# WSPR Beacon 2.0 — Project Architecture

This document describes current architecture and technical policy. Operational
instructions belong in the [User Guide](docs/USER_GUIDE.md); detailed task
evidence remains in [TASKS.md](TASKS.md).

## 1. Current project status

WSPR Beacon 2.0 is an experimental autonomous WSPR Type 1 beacon. ESP8266
NodeMCU and ESP32 Dev Module are primary development platforms sharing the same
firmware core. ESP8266 has prior hardware evidence and is the planned target of the
first binary release. ESP32 hardware validation is partially complete: core
operation, persistent WSPR v3, HTTP/Web UI, periodic SNTP renewal and natural
WSPR frames have been exercised on an ESP32 Dev Module. A live Dashboard
snapshot at 188 minutes 5 seconds uptime showed synchronized NTP and 47
started/completed frames with zero interruptions. Continuous logging of that
full interval, real NTP-failure recovery, quantitative RF validation and an
official binary remain pending. Nano Every
support is retained but frozen for new features; Nano 33 IoT is planned.

Three ESP32 reliability changes are integrated and consolidated on `main`:
non-blocking HTTP socket writes, priority-aware Web UI
navigation and explicit 15-minute SNTP renewal with bounded recovery. Native
DEVELOPMENT/PUBLIC verification passes 559/559 tests and all four firmware
environments build successfully.

The firmware supports manual software selection of 40, 30, 20, 17, 15, 12 and
10 m in `FIXED` and per-frame `RANDOM` frequency modes, with 20 m as the
default. It has persistent WSPR v3 and Wi-Fi configuration, GPS/PPS
timing, experimental NTP authorization, OLED status pages, serial control,
centralized diagnostics, experimental JSON status/configuration APIs and a
responsive local dashboard. Authenticated-by-request-context HTTP operations
can stage, discard and explicitly save/apply the WSPR candidate. The
experimental `/config` page provides session-controlled WSPR editing, discard
and explicit save/apply controls, plus a protected request to restart in the
existing AP Provisioning portal for Wi-Fi setup. Multiband RF hardware validation remains
open and software selection does not replace band-appropriate filtering.

The predecessor project recorded a stable pre-multiband baseline. Its private
history identifier is intentionally omitted and is not present in this
repository. Functional NodeMCU and IC-705 observations exist, but quantitative
RF validation remains open.

PlatformIO provides explicit DEVELOPMENT and PUBLIC build environments for
ESP8266, ESP32 and native verification. Exactly one profile is selected at
compile time, while Nano Every is explicitly marked HISTORICAL. ESP8266
DEVELOPMENT is the default build. DEVELOPMENT retains `IW2NNV`/`JN45`; PUBLIC
starts with an empty callsign and `JN45`, keeping TX inhibited until a valid
identity is saved. Valid saved records still take precedence over build defaults.
This safety default does not by itself make the firmware release-ready.

## 2. System architecture

- **NMEA/GPS layer:** parses checksummed GGA/RMC sentences, tracks time, fix,
  satellites and optional PPS association.
- **TimeService:** arbitrates UTC independently of position. Precise GPS/PPS
  has priority; confirmed fresh NTP may authorize TX as `NTP_EXPERIMENTAL`.
- **LocatorService:** selects the configured locator, current GPS locator or
  last valid GPS locator without coupling location to the UTC source.
- **WsprScheduler:** identifies eligible slots and converts an armed UTC slot
  to wrap-safe monotonic millisecond/microsecond deadlines.
- **WsprTransmitter:** owns `IDLE`, `PREPARING`, `TRANSMITTING` and inhibited
  states, freezes a prepared snapshot, encodes symbols and controls RF through
  the safety layer.
- **RfSafetyController and Si5351A driver:** keep output disabled unless device,
  configuration and runtime operations are acceptable.
- **BandPlan and ConfigValidator:** define band metadata, validate the complete
  four-tone set and separately enforce TX authorization.
- **ConfigurationManager:** maintains CANDIDATE, SAVED and ACTIVE state and
  versioned WSPR/Wi-Fi records through platform storage adapters.
- **ConfigurationService:** is the application-level owner of configuration
  mutations. It gives the serial CLI and web API one validated,
  revisioned path for staging, immediate RF-safe mode selection, discard and
  explicit save/application without duplicating persistence logic. A guarded
  WSPR-only save/reconciliation operation supports the future web workflow by
  reusing the versioned WSPR A/B record without validating, writing, promoting or
  reconnecting Wi-Fi; a separate web Wi-Fi save path persists only the Wi-Fi
  A/B record and schedules a guarded restart. The CLI retains its existing
  combined save behavior.
- **ConfigurationSessionController:** is the firmware-level owner of temporary
  CLI/web configuration sessions. Active ownership, a protected operation or
  the post-session resume guard inhibits new scheduler admission; RF enable is
  checked again defensively. The serial CLI owns sessions through `config` and
  `exit`; the HTTP API exposes guarded enter/heartbeat/exit operations for one
  browser owner.
- **ManualTxController:** owns one RAM-only manual permission shared by CLI and
  web. It defaults to ON at every boot. OFF is temporary until explicit ON or
  reboot, aborts preparation or an active frame and disables RF without
  creating a hardware fault; ON only permits the existing safety gates to
  admit a future scheduler slot.
- **FactoryResetCore and FactoryResetService:** provide a recoverable reset transaction for
  WSPR and Wi-Fi A/B records. Two sequenced CRC-protected metadata records are
  stored outside the existing payload slots. Boot recovery completes a pending
  invalidation before configuration is accepted; completed or ambiguous state
  stays unconfigured and TX-inhibited. Fresh development storage remains
  distinct and retains the `IW2NNV`/`JN45` defaults. One shared service exposes
  separately confirmed CLI and protected Configuration-page actions, serializes
  them against configuration writes, invalidates sessions/tokens and requires a
  manual reboot without changing the current network mode. After a completed reset, a verified explicit save
  of a new valid WSPR identity clears both metadata records; failure to clear
  them leaves the next boot fail-closed. Fresh PUBLIC storage instead starts
  with an empty callsign and `JN45`; valid saved records still take precedence.
- **Diagnostics/UI:** compact snapshots feed the serial console, OLED and
  telemetry without making either interface part of RF decision logic.
- **Firmware identity and artifact preparation:** one compile-time component
  reports project, `dev` version, profile, target and a verified source commit
  when available. The same identity is printed once at startup and included
  additively in `/api/status`. Every PlatformIO firmware build also emits a
  deterministic provenance record bound to the firmware SHA-256. Public
  staging builds the requested PUBLIC environment in a detached clean worktree
  at the verified commit, validates that record and the embedded identity,
  records all component hashes and publishes through an atomic directory
  rename. A separate host-only validator checks the resulting PUBLIC package,
  checksum inventory, offsets and ESP32 partition identity without accessing a
  serial device. DEVELOPMENT artifacts remain non-distributable.
- **Bounded HTTP transport:** on ESP8266/ESP32, a single-client fixed-buffer
  state machine serves the read-only dashboard and JSON API. The dashboard is
  a self-contained program-flash asset and allocates no full-page RAM buffer. A
  scheduler-derived T−7 s gate closes the listener/client before `PREPARING`;
  no application HTTP work runs during `PREPARING` or TX.
- **Platform adapters:** isolate serial ports, GPIO, interrupts, storage,
  Wi-Fi/SNTP and board-specific LED polarity.

The core timing, parsing, configuration, location and diagnostic components
remain independent of ESP-specific libraries and are covered by native tests.

## 3. Supported platforms

| Platform | Role and status |
| --- | --- |
| AZDelivery NodeMCU Amica V2 / ESP8266-12F (`nodemcuv2`) | Primary development platform; build/upload and major operational paths exercised; planned first binary-release target |
| ESP32 Dev Module (`esp32dev`) | Primary development platform; builds and shared-core tests pass; core operation, persistence, HTTP/Web UI, SNTP renewal and natural WSPR frames partially hardware-validated; a live snapshot exceeded three hours, while continuously logged endurance, fault recovery, quantitative RF validation and an official binary remain pending |
| Arduino Nano Every | Historical/current source support retained; feature growth frozen because of flash limits |
| Arduino Nano 33 IoT | Future integration; no `BoardConfig.h` support yet |

Common peripherals are a NEO-6M-compatible GPS receiver, SSD1306 128×64 OLED
and Si5351A using CLK0. Wiring and voltage constraints are documented in
[HARDWARE.md](docs/HARDWARE.md).

## 4. Implemented capabilities

### Time and location

GPS time can exist without a position fix. A GPS reference is TX-qualified as
`PRECISE` only after three consecutive PPS periods in the 900–1100 ms
qualification window and a coherent association with UTC. A first or isolated
edge is only a candidate; glitches, missing edges, queued multiple edges and
incoherent UTC revoke qualification and require a complete new sequence. The
window is a conservative periodicity check, not a claim of absolute timing
accuracy. On Wi-Fi platforms,
a callback-confirmed, plausible and fresh SNTP reference may be selected as
`NTP_EXPERIMENTAL` when precise GPS is unavailable. A coarse GPS reference does
not block that fallback. Late qualification never recovers a slot already in
progress.

The WSPR locator is always four characters. It begins as configured `FIXED`,
becomes `GPS_LIVE` with a valid position and remains `GPS_LAST_KNOWN` in RAM
after fix loss. Time and locator selection are independent.

### Scheduling and transmission

The nominal symbol interval is approximately 682.667 ms. An eligible frame is
armed no earlier than about T−5 s. During `PREPARING`, the transmitter freezes
callsign, locator, declared power, band, selected frequency, time authority and
slot; encodes all 162 symbols; and preprograms symbol 0 with CLK0 disabled.

The OLED may show `PREPARING`, then `TX ON` at about T−1 s if at least 500 ms
of safe rendering margin remains. Display completion is never a TX prerequisite.
At the monotonic slot deadline, only CLK0 is enabled and symbol 0 begins. Later
symbols use absolute deadlines anchored to that slot. No OLED or serial output
occurs during the RF-critical interval. Completion or abort disables RF before
normal UI activity resumes.

### Band and frequency policy

`BandPlan` contains stable IDs and nominal ±100 Hz windows for 40, 30, 20, 17,
15, 12 and 10 m, and all seven are manually software-selectable. `FIXED` uses
the selected band's configured base plus offset. `RANDOM` selects tone zero
once per frame from nominal −90 through +89 Hz for the selected band. The
prepared value is immutable, and validation keeps all four approximately
1.46 Hz-spaced tones inside the selected band's allowed window. Automatic band
rotation is not implemented.

### Configuration and persistence

The serial interface edits validated candidates and applies changes only in RF-
safe states. Frequency mode selection updates ACTIVE and CANDIDATE immediately
when safe; `config save` makes it persistent.

ESP8266 and ESP32 instantiate `ConfigurationManager` at runtime. WSPR and Wi-Fi
records are separate, use two logical slots, sequence numbers and CRC-32, and
are read back after writes. WSPR payload v3 stores band, frequency mode and
Power Save while retaining the 22-byte payload and 40-byte record layout. V1
and v2 are read with Power Save OFF and are migrated only by an explicit save.
Downgrade after a v3 save is not guaranteed.

Boot diagnostics distinguish a rejected A/B slot followed by successful SAVED
recovery from rejection of every available record followed by DEFAULTS. An
absent slot does not produce either warning, and Factory Reset fail-closed keeps
its dedicated diagnostic instead of being described as an ordinary default
fallback. These messages do not alter record decoding, sequence selection or
persistence.

The configuration and transmitter layers also support an explicit unconfigured
operating identity represented by an empty internal callsign and the state
`OPERATING_IDENTITY_UNCONFIGURED`. It is distinct from a corrupt stored record,
is never encoded as a WSPR callsign and keeps scheduler admission and RF output
inhibited even with qualified time. DEVELOPMENT remains `IW2NNV`/`JN45`, while
PUBLIC starts unconfigured with locator `JN45`. Existing valid SAVED records
and the v1/v2 formats remain readable with Power Save OFF.

### Interfaces and diagnostics

The five-second controlled startup and sequential Hardware Check inhibit TX.
Operational OLED pages show time/location/network and WSPR IDLE state, rotating
every five seconds when time is qualified. The serial console provides MONITOR
and INTERACTIVE modes, configuration, diagnostics and telemetry at 9600 baud.
Exact commands and screen tokens are authoritative in the User Guide.

WEB-001C composes the existing authoritative snapshots on demand into a
versioned, bounded JSON response at `/api/status`. WEB-002C similarly exposes
the authoritative ACTIVE, CANDIDATE and SAVED WSPR views, revision and compact
validation capabilities at `/api/config/v1`. Both APIs are read-only and
contain no credentials. WEB-001D serves a responsive, read-only page from
program flash; browser-side polling is serialized and bounded, and stale data
is labelled without becoming a source of scheduling decisions.

WEB-002D adds bounded candidate-only mutations to the same transport. A strict
fixed-buffer parser accepts only complete JSON objects with an expected
configuration revision. Mutations require the current IPv4 Host, exact
same-origin Origin and a per-boot entropy-derived CSRF token obtained from a
same-origin bootstrap route. The RF gate and transmitter state are checked
again before dispatch. These controls reduce accidental and cross-origin LAN
requests; they do not authenticate another client already on the trusted LAN.
WEB-002E.3 adds an experimental, explicitly confirmed WSPR-only save/apply
route. It requires the matching web configuration-session owner and delegates
to the guarded `ConfigurationService` operation; HTTP response delivery is not
part of the persistence transaction, and Wi-Fi persistence remains isolated.
WEB-002F.2 adds a self-contained, flash-resident `/config` page that polls the
existing configuration and session snapshots without acquiring a session on
load. WEB-002F.3 lets the operator explicitly enter, maintain and exit the
existing authoritative web configuration session. WEB-002F.3.1 preserves the
same browser owner across one or two transient heartbeat failures and performs
two bounded authenticated renewal attempts; it never treats read-only owner
visibility as proof of ownership or automatically reacquires an expired
session. WEB-002F.4 adds capability-derived local WSPR editing, atomic
CANDIDATE staging, browser-only Reset and authenticated server Discard. ACTIVE
and SAVED remain untouched until an explicit action. WEB-002F.5 connects the
existing Save and apply control to the guarded WSPR-only endpoint. Eligibility
requires confirmed ownership, no local unstaged edits and authoritative work
to reconcile; response loss triggers read-only reconciliation and never an
automatic persistence retry. Wi-Fi configuration remains isolated.

CFG-MODE-001B adds an internal, RAM-only configuration-session controller with
independent CLI and web ownership, 25-second web expiry and a scheduler-derived
resume guard. CFG-MODE-001C exposes CLI ownership through `config`, `exit` and
`config exit`; WSPR mutations require that ownership. CFG-MODE-001D exposes the
authoritative mode through JSON, console, OLED and dashboard status. While a
configuration session is active, HTTP remains available across nominal slot
boundaries but still closes for actual `PREPARING`/TX. CFG-MODE-001E binds
secure HTTP session enter/heartbeat/exit and candidate/discard authorization to
the same controller.

WEB-003B.1 adds a boot-mode controller for Wi-Fi targets. Complete verified
saved credentials receive one 15-second asynchronous STA attempt; absent or
failed credentials enter a boot-latched provisioning AP. Provisioning uses a
per-boot random eight-digit WPA2 password, a dedicated bounded HTTP profile and
a Wi-Fi-only verified persistence path. TX admission and transmitter execution
are independently blocked outside `NORMAL`. WEB-003B.2 adds an explicit,
asynchronous provisioning-only network scan with bounded snapshots and no STA
association; manual SSID entry remains available independently of scan status.
WEB-004B.1 adds an explicit, RAM-only transition from provisioning to an
operational HTTP profile on the same AP. Dashboard and WSPR Configuration are
available at `192.168.4.1`, while station association and provisioning routes
are disabled. Configuration can instead request a verified one-time AP Setup
marker and a deferred restart into the existing provisioning portal. WEB-004B.2 extends the centralized RF-mode
policy to AP Operational, requires precise GPS/PPS there, and applies the
existing resume, configuration, scheduler and HTTP T−7 protections. Provisioning
and provisioning-error modes remain structurally TX-inhibited.
Controlled NodeMCU verification covered the AP transition, operational pages,
configuration-session heartbeat, AP continuity and fail-closed unqualified-time
status. AP Operational RF transmission and timing remain unverified on hardware.

## 5. Architecture reference

### Boot and network states

| State | Entry and network behavior | RF policy |
| --- | --- | --- |
| `BOOT_DECISION` | Checks Factory Reset and a one-time AP Setup marker before deciding whether to use saved Wi-Fi credentials | TX inhibited |
| `STA_CONNECTING_INITIAL` | One asynchronous STA attempt, bounded to 15 seconds | TX inhibited |
| `NORMAL` | Connected STA; operational HTTP profile | Existing qualified GPS/PPS or experimental NTP policy may authorize TX |
| `PROVISIONING_AP` | Latched WPA2 AP at `192.168.4.1`; provisioning-only HTTP profile | Structurally inhibited |
| AP Operational transition pending | Completes the accepted response, closes/discards HTTP work and changes profile | TX inhibited; resume guard armed |
| `AP_OPERATIONAL` | Same AP, Dashboard and Configuration; the protected Wi-Fi Setup request restarts into AP Provisioning | Requires `GPS_PRECISE` plus every normal RF/configuration gate |
| `PROVISIONING_ERROR` | AP start failed | Fail-closed and TX inhibited |

Restarting repeats the saved-credential decision unless a verified one-time AP
Setup request is pending. Neither AP mode automatically
switches back to STA when the original network becomes available.

### RF admission and diagnostic eligibility

Actual RF admission remains owned by `WsprTransmitter`, `WsprScheduler`,
`RfSafetyController`, the boot-mode policy and configuration-session controller.
The read-only `TxEligibility` calculation is only a shared diagnostic view for
Dashboard status, CLI `tx status` and immediate HTTP TX-control responses.

| Diagnostic prerequisite | Required condition |
| --- | --- |
| Manual permission | RAM-only Manual TX is ON |
| Operating mode | NORMAL or AP Operational |
| Operating identity | Valid configured callsign |
| RF readiness | RF safety layer operational |
| Configuration | No active CLI/web Configuration owner |
| Resume policy | Resume guard inactive |
| Time | Mode-qualified authority; AP Operational requires `GPS_PRECISE` |

Eligibility means ready for a possible future automatic slot. It is not an
active-transmission state and cannot start, prepare or authorize a frame. Manual
TX ON is the boot default, but it removes only the manual inhibit. Actual states
such as `IDLE`, `RF_INHIBITED`, `PREPARING` and `TRANSMITTING` remain separate.

### Configuration and persistence domains

| Domain | ACTIVE | CANDIDATE | SAVED |
| --- | --- | --- | --- |
| WSPR | Used for future frames | Validated RAM edits | Latest verified WSPR A/B record |
| Wi-Fi | Current runtime credentials/state | RAM credentials | Latest verified Wi-Fi A/B record |

Ordinary WSPR field edits stage CANDIDATE. RF-safe CLI frequency-mode selection
is the documented exception that updates ACTIVE and CANDIDATE immediately while
leaving SAVED unchanged until explicit `config save`. The web Save and apply
operation persists WSPR only; the CLI combined save retains its existing WSPR
and Wi-Fi behavior. Formats, CRC, slots and sequence selection remain owned by
`ConfigurationManager` and `PersistentConfigRecord`.

### Factory Reset markers and recovery

Factory Reset uses two sequenced, CRC-protected metadata slots outside the WSPR
and Wi-Fi payload slots.

| Marker/recovery condition | Boot behavior |
| --- | --- |
| No marker | Normal development defaults or ordinary A/B recovery |
| `IN_PROGRESS` | Complete invalidation before accepting configuration; fail closed on ambiguity |
| `COMPLETE` | WSPR remains unconfigured; a valid post-reset Wi-Fi record may load independently |
| Invalid/unreadable marker | Load neither domain and remain fail closed |

A Wi-Fi-only provisioning save does not clear the COMPLETE marker. On a later
boot its verified record can enable STA while WSPR remains unconfigured and TX
inhibited. Only the existing verified path that persists a valid WSPR identity
may clear the marker. Reset execution requires explicit CLI or web confirmation
and a manual reboot.

### HTTP profiles and security

| Profile | Routes | Mutation protection |
| --- | --- | --- |
| AP Provisioning | `GET /`, `GET /api/provision/v1/token`, `GET|POST /api/provision/v1/scan`, `POST /api/provision/v1/wifi`, `POST /api/provision/v1/operational` | Valid Host; mutations also require exact Origin and per-boot CSRF token |
| NORMAL / AP Operational | `GET /`, `GET /config`, `GET /api/status`, `GET /api/config/v1`, token/session routes, candidate/discard/save, `POST /api/config/v1/wifi/setup`, TX ON/OFF and Factory Reset | Host/Origin/CSRF for mutations; configuration mutations additionally require the active `X-WSPR-Session` owner |

The operational session routes are `GET /api/config/v1/session` and protected
`POST /api/config/v1/session/enter`,
`POST /api/config/v1/session/heartbeat` and
`POST /api/config/v1/session/exit` operations.
Candidate mutation uses `PUT /api/config/v1/candidate`; discard and WSPR-only
save use `POST /api/config/v1/discard` and `/save`. Manual control uses
`POST /api/tx/v1/on` and `POST /api/tx/v1/off`; Factory Reset uses
`POST /api/factory/v1/reset`. Provisioning routes are absent from operational
profiles, and operational routes are absent from AP Provisioning.

These controls mitigate cross-origin and DNS-rebinding requests on a trusted
LAN; they are not user authentication. The single-client transport remains
subordinate to the T−7 gate and performs no HTTP work during PREPARING or TX.

### OLED display priority

The selection priority is AP Provisioning, provisioning error, Hardware Check,
then normal operational screens. AP Provisioning therefore keeps SSID, password
and IP visible while Hardware Check continues internally. AP Operational and
NORMAL retain Hardware Check priority, followed by the existing operational,
PREPARING and TX presentation. Display success is never an RF prerequisite.

## 6. Technical invariants and safety requirements

- Never start TX without qualified time, valid configuration and operational RF.
- Keep CLK0 and the global output gate disabled through initialization and
  preparation; enable CLK0 only at the scheduled slot.
- Do not recover missed slots or intentionally delay the first symbol for UI,
  networking or logging.
- Freeze every frame input before TX; changes apply only between frames except
  critical faults, which abort and disable RF.
- Use UTC only to select/arm a slot; use monotonic absolute deadlines after
  arming and throughout modulation.
- Do not perform OLED, console, network or diagnostic I²C work during TX.
- Treat an absent OLED as non-fatal and an absent/failed Si5351A as TX-inhibiting.
- Never expose Wi-Fi credentials in source, status output or logs.
- Treat declared WSPR dBm independently from measured RF power.
- Do not claim RF validation from I²C detection, driver success or build tests.

## 7. Historical decisions and superseded approaches

The staged decisions below remain traceable because they explain why the
current design exists. They are **not current behavior where marked
superseded**.

| Decision | Historical rationale and current disposition |
| --- | --- |
| T-005C.1 | Introduced non-blocking native SNTP as diagnostic-only input. Superseded by T-005D.3, which permits explicitly experimental NTP TX authorization. |
| T-005D.1 | Moved scheduler UTC to `TimeService` and initially authorized only precise GPS/PPS. Its monotonic-deadline model remains; its NTP prohibition is superseded. |
| T-005D.2 | Established independent `FIXED` / `GPS_LIVE` / `GPS_LAST_KNOWN` locator behavior; still current. |
| T-005D.3 | Added GPS-priority and experimental synchronized-NTP authorization; still current. References to freezing “before warm-up” now mean freezing in `PREPARING`. |
| T-006A.1/A.2 | Introduced RAM-only WSPR/Wi-Fi candidates. Superseded by runtime persistence; the candidate model remains. |
| T-006B.1/B.2 | Designed versioned records and storage backends before runtime integration. Statements that backends are absent or uninstantiated are superseded by T-006B.3A. |
| T-006B.3A/B | Integrated storage, Wi-Fi and active WSPR settings. Early statements that WSPR settings or NTP did not affect TX are superseded. |
| OLED-002E | Added a display-coordinated PREPARING/warm-up sequence. RF-ARCH-002 subsequently removed intentional RF warm-up. |
| RF-ARCH-002 | Established the current `IDLE → PREPARING (RF OFF) → TRANSMITTING → IDLE` sequence. |
| RF-BAND-002A/B/C | Added BandPlan, RAM-only RANDOM and UI integration in stages. RAM-only mode is superseded by RF-BAND-002D.2 persistence v2. |
| RF-BAND-002D.1/.2 | Unified band/mode state, then added v2 persistence with v1 read compatibility; still current. |

The original Arduino `.ino` sketches were historical reference material, not
an alternative active firmware. They were excluded from the consolidated tree
after their provenance was recorded and are not recoverable from this
repository's current Git history. Early “no tests/no hardware validation”
findings belong to that predecessor baseline and are likewise historical; its
private Git identifier is intentionally omitted.

## 8. Open work and roadmap

1. Quantitatively measure first-symbol timing, frequency error and stability.
2. Measure RF power, startup transients, harmonics and filter attenuation on a
   documented dummy-load setup; confirm decode performance independently.
3. Extend ESP32 hardware validation with continuously logged endurance, real
   NTP failure recovery, prolonged OLED observation and quantitative RF
   behavior. The existing 188-minute snapshot is evidence of observed uptime,
   not continuous event logging.
4. Complete remaining WEB-003 hardware work: actual
   new-credential saving, detailed heap/watchdog measurement and instrumental
   RF-silence verification; long-press entry remains future work.
5. Define safe button short/long-press behavior; band selection is currently
   available through CLI/web, not through the physical button.
6. Integrate Nano 33 IoT; revisit Nano Every flash only as a separate task.

OTA, automatic multiband scheduling and advanced history/export remain future
work. All RF changes require explicit authorization, suitable filtering and a
clear distinction between software tests, functional observation and measured
performance.

## 9. License and distribution preparation

Original modern project material is licensed under GPL-3.0-or-later with the
notice `Copyright (C) 2026 IW2NNV`. The separate project credit remains
`Developed by IW2NNV / AC2WZ`; both callsigns identify the same principal
developer, while `zampro79` is the public GitHub identifier. See
[LICENSE](LICENSE) and [COPYRIGHT.md](COPYRIGHT.md) for the grant and scope.

The ESP8266 dependency inventory and exact locally verified license texts are
documented in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and
[`licenses/`](licenses/README.md). The separate [legacy provenance
record](docs/LEGACY_PROVENANCE.md) preserves uncertainty around the removed
Arduino sketches, while [the source-distribution plan](docs/SOURCE_DISTRIBUTION.md)
defines the evidence and corresponding source expected for a future binary.
Unresolved LGPL relinking requirements and ESP8266 SDK/toolchain compatibility
questions remain release blockers. The first binary candidate is ESP8266-only;
ESP32 remains in parallel active development while its binary distribution is
deferred. Formal project licensing does not authorize a public
binary release or resolve third-party redistribution requirements.

RC-ESP8266-001 prepares provisional package identity `2.0.0-rc.1` without
changing the embedded `dev` firmware version or creating a tag. Schema-2
packaging binds the exact ESP8266 PUBLIC binary, legal/documentation inventory
and deterministic corresponding-source archive to one commit. Technical package
eligibility remains separate from legal authorization; the current status is
`NOT_APPROVED_FOR_PUBLIC_DISTRIBUTION` pending LGPL and SDK/GCC review.
