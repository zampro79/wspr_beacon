# TASKS.md — WSPR Beacon 2.0

This file is the development ledger. Historical results describe the scope and
evidence available when each task closed; later tasks may supersede behavior
without invalidating that evidence. See [PROJECT.md](PROJECT.md) for current
architecture and [the User Guide](docs/USER_GUIDE.md) for operation.

## ESP32-HW-001 — Initial ESP32 integration validation (2026-11-05, in review)

- Tested an ESP32-D0WD-V3 revision 3.0 board with 4 MiB flash and CP2102
  USB-UART through the PlatformIO `esp32dev` DEVELOPMENT target and Arduino
  ESP32 framework 2.0.17. Upload and byte-for-byte readback passed; normal boot,
  saved-credential STA, WSPR v3 persistence with ACTIVE/CANDIDATE/SAVED
  agreement, and Si5351A, SSD1306 OLED and GPS NMEA detection were observed.
  PPS was not connected.
- The three integrated fixes are consolidated on `main`: ESP32 non-blocking
  HTTP writes, priority-aware Dashboard/Configuration navigation, and explicit
  15-minute ESP32 SNTP renewal with bounded 5/10/15-minute recovery. Native and
  native_public passed 559/559 tests, all four ESP8266/ESP32 DEVELOPMENT/PUBLIC
  builds succeeded, deterministic gzip verification passed, and Firefox IDLE
  navigation passed 5/5 checks. Dashboard, Configuration, JSON APIs, gzip
  delivery and HTTP reopening after natural TX were exercised. The largest
  observed HTTP write duration after the fix was approximately 6–9 ms, not a
  formal worst-case guarantee.
- Five NTP synchronizations, including four renewals, reset the reference age.
  `NTP_EXPERIMENTAL` authority remained available at the 60-minute 26-second
  checkpoint. Fifteen natural frames started and all fifteen completed, with
  zero interrupted frames and no unexpected reboot observed in that run.
- The 2026-11-05 20 m on-air session for `IW2NNV` recorded 166 WSPRnet spots
  from 38 distinct receiving callsigns, a maximum reported distance of 1,675
  km, and a last listed spot at 11:28 UTC with 10 dBm declared WSPR power. These
  reports demonstrate decodable frames, not calibrated RF performance. Two
  operational stops were observed. The first is consistent with the later
  demonstrated mismatch between the 60-minute application reference lifetime
  and the former 180-minute ESP32 SNTP interval; the available evidence does
  not establish that the second stop had the same cause.
- **2026-11-06 — ESP32 extended endurance evidence:** A user-observed live
  DEVELOPMENT Dashboard snapshot at 188 minutes 5 seconds uptime showed NORMAL
  mode, scheduler IDLE, NTP `SYNCHRONIZED` with `NTP_EXPERIMENTAL` authority
  and an 8-minute reference age. The latest frame was completed at 13:00:00 UTC;
  47 frames had started and all 47 had completed with zero interruptions, while
  the next slot was reported for 13:04 UTC. Wi-Fi was connected at
  `192.168.1.84`, RF was ready with inhibit `NONE`, GPS NMEA was receiving with
  no fix or PPS, and free heap was 236,024 bytes. This extends the previous
  60-minute hardware checkpoint beyond three hours and showed no current
  `WAITING_FOR_TIME` or operational stop. A single snapshot does not prove
  uninterrupted authority or absence of earlier resets, identify the number of
  renewals in this session, verify deliberate NTP fault recovery or OLED visual
  cycling, measure RF output or spectrum, or establish external reception of
  every completed frame. Earlier NTP success/renewal counters are not attributed
  to this session without continuity evidence.
- Remaining work includes a continuously logged endurance run, recovery from a
  real NTP network/server failure, prolonged visual OLED-rotation observation,
  complete Firefox navigation coverage in protected TX windows, the ESP8266
  hardware matrix for the new navigation scripts, and instrumental RF power,
  frequency, harmonics and spectral validation. Public firmware distribution
  remains unauthorized.

## POWER-SAVE-001 — Power Save v1 (in review)

- Adds persistent Power Save through WSPR payload v3 with v1/v2 read
  compatibility, unchanged A/B layout and explicit-save-only migration.
- Reuses the guarded candidate/active/saved pipeline for strict JSON,
  Configuration UI and serial configuration; candidate edits have no immediate
  visual effect.
- Keeps LED diagnostics running while suppressing physical outputs and uses
  cumulative, presentation-aware OLED timing with RF-critical DISPLAYOFF/ON
  deferral. DEVELOPMENT and PUBLIC native suites pass 529/529; ESP8266 and
  ESP32 DEVELOPMENT/PUBLIC builds, deterministic gzip regeneration and
  `git diff --check` pass. Controlled ESP8266 verification confirms runtime
  Power Save enable/disable, Hardware Check LED behavior, OLED suppression and
  preservation of the existing RF-critical protections.
- Hardware review requested extending only the cumulative STA IPv4 presentation
  from 5 to 15 seconds; AP Provisioning/AP Operational remain 15 seconds and AP
  START ERROR remains 10 seconds. Updated native verification and all four
  firmware builds pass; controlled ESP8266 observation confirms the 15-second
  cumulative STA IPv4 presentation. ESP32 hardware remains unvalidated.

## WEB-RECOVERY-001 — Persistent Dashboard status recovery (in review)

- Removes the terminal browser-side state that stopped `/api/status` polling
  after three recovery attempts. Three bounded fast retries are followed by a
  30-second retry cadence that continues until a validated status is accepted.
- A long foreground/background timer discontinuity restarts recovery promptly;
  known estimated TX windows still suppress requests, polling remains serial,
  and every existing response-age, schema and deadline-shift rejection remains
  fail-closed. HTTP server, RF gate, scheduler and time authority are unchanged.
- Deterministic policy tests cover continued slow retry, full reset on accepted
  status, known-window suppression and request serialization. Both 524/524
  native profile suites pass, deterministic gzip generation/round-trip passes,
  and all four ESP8266/ESP32 DEVELOPMENT/PUBLIC builds pass. Compared with the
  documented WEB-NAV-001 checkpoint, RAM is unchanged and flash increases by
  112 bytes on ESP8266 and 120 bytes on ESP32. Controlled ESP8266/Firefox
  validation confirms automatic recovery after TX and HTTP timeout sequences. The
  ESP8266 DEVELOPMENT image uploads successfully to the identified NodeMCU;
  read-only HTTP verification confirms NORMAL/STA operation, the preserved
  SAVED v2 WSPR configuration and the new recovery JavaScript in the served
  gzip asset. No RF test, configuration mutation, commit or push occurs.

## WEB-NAV-001 — Request-free current-page navigation (in review)

- Replaces each primary-navigation link to its already displayed Dashboard or
  Configuration page with a native keyboard-accessible button. Activating the
  current item, including with Enter, cannot issue an HTTP request or reload.
- Cross-page navigation retains the existing Dashboard RF-window gate and the
  Configuration preflight/session-release flow. HTTP server, RF gate,
  scheduler, timing and safety behavior are unchanged.
- Deterministic identity/gzip asset checks and both 521/521 native profile
  suites pass. All four ESP8266/ESP32 DEVELOPMENT/PUBLIC builds pass; RAM is
  unchanged and flash increases by 576 bytes in each environment relative to
  commit `665b34530515d5908a1830c64317f20c48d38c47`. The ESP8266 DEVELOPMENT
  image uploads successfully to the identified NodeMCU without erasing saved
  configuration; the device restarts in STA mode and serves Dashboard/status
  with the prior valid operating identity. Subsequent manual Firefox checks
  passed all eight same-page and cross-page paths in IDLE and TX, including
  keyboard activation. No commit or push occurs.

## RC-ESP8266-001 — Final release-candidate preparation (in review)

- Introduces provisional package identity `2.0.0-rc.1` while retaining embedded
  firmware version `dev`; no tag or runtime metadata changes are made.
- Extends ESP8266 PUBLIC staging with license/copyright/notices, installation
  instructions, release notes, targeted license review and a deterministic
  exact-commit corresponding-source archive. Schema-2 verification rejects
  missing or altered material, unsafe paths, legacy sketches, DEVELOPMENT and
  ESP32 RC input.
- Technical package eligibility is independent of legal authorization. Public
  distribution remains false and status remains
  `NOT_APPROVED_FOR_PUBLIC_DISTRIBUTION` pending LGPL relinking and NONOS
  SDK/GCC conclusions.
- Verification: 26/26 host packaging/validation tests and both 481/481 native
  profile regressions pass. ESP8266 DEVELOPMENT builds at 42,924 B RAM /
  484,603 B flash and PUBLIC at 42,912 B / 484,575 B, unchanged from the
  consolidated baseline. No hardware, tag, publication, commit or push occurs.

## LICENSE-001C.2 — GPL license and copyright formalization (in review)

- Formally adopts GPL-3.0-or-later for original modern project material using
  `Copyright (C) 2026 IW2NNV`; developer credits remain
  `Developed by IW2NNV / AC2WZ`, and `zampro79` is documented only as the
  public GitHub identifier.
- Adds the complete GPLv3 text at repository root and a central scope notice.
  No executable source headers or generated gzip data are modified.
- Third-party licenses remain independent, removed historical sketches are not
  relicensed, and LGPL relinking plus NONOS SDK/GCC questions remain open.
  No public binary distribution is authorized.

## LICENSE-001C.1 — ESP8266 license materials (consolidated)

- Adds an ESP8266 third-party inventory, exact locally verified license texts,
  historical-sketch provenance and a future corresponding-source checklist.
- At that checkpoint GPL-3.0-or-later was recorded only as the intended project
  license and no root `LICENSE` or repository-wide SPDX declaration was
  introduced. LICENSE-001C.2 subsequently formalizes the project grant and
  holder identifier without changing this task's historical evidence.
- LGPL static-link relinking materials and the ESP8266 binary SDK/GCC runtime
  exception analysis remain release blockers requiring legal review. No public
  distribution is authorized.
- The first binary release candidate remains ESP8266-only. ESP32 is a primary,
  actively developed platform but is outside this binary packaging phase pending
  hardware validation; firmware and build configuration are unchanged. LEGACY-001
  excludes the five obsolete root-level sketches without asserting ownership of
  their contents; their predecessor Git history was not imported into the
  consolidated repository.
- LEGACY-001 verification confirms that neither ESP8266 profile depends on the
  removed sketches: DEVELOPMENT builds at 42,924 B RAM / 484,603 B flash and
  PUBLIC at 42,912 B RAM / 484,575 B flash. Native regression and ESP32 builds
  were omitted because no firmware, test or build configuration changed.

## DIAG-CONFIG-001B — Boot configuration warning correction (in review)

- Adds a pure boot-diagnostic classifier that distinguishes recovery of a
  valid SAVED record after rejection of another A/B slot from fallback to
  DEFAULTS when no valid record remains. Absent slots emit no warning and
  Factory Reset fail-closed retains its dedicated error.
- Boot ordering, persistence formats, decoding, A/B selection, Factory Reset,
  scheduler and RF behavior are unchanged. No rejected slot is rewritten.
- Focused native coverage exercises exact WSPR/Wi-Fi messages, fresh storage,
  two valid records, one zeroed plus one valid record, both invalid records,
  interrupted writes and the existing Factory Reset recovery/fail-closed paths.
- Verification: 52/52 focused cases and 481/481 cases in each full native
  profile passed; all four DEVELOPMENT/PUBLIC ESP8266/ESP32 builds passed.

## RELEASE-PKG-001C — Package verification and flashing procedures (in review)

- Adds a host-only PUBLIC package validator for manifest schema, identity,
  component inventory, safe filenames, offsets, sizes, SHA-256 data and ESP32
  partition identity. It never accesses serial hardware.
- Documents target-specific standalone workflows using the locally verified
  esptool 3.0 syntax for ESP8266 and 4.11.0 syntax for experimental ESP32.
- Separates first installation, application-only update, Factory Reset and
  destructive recovery. ESP8266 persistence regions and ESP32 NVS/OTA limits
  are explicit; `erase_flash`, `--erase-all`, `--force` and merged images are
  excluded from normal procedures.
- Linux, macOS and Windows host instructions distinguish documentation/tool
  verification from hardware validation. No binary publication or hardware
  operation is authorized.
- Verification: all 22 host packaging/validation tests pass; deterministic web
  assets, Markdown links, esptool 3.0/4.11.0 command options, whitespace and
  the approved eight-file scope were checked. Firmware/native results remain
  the unchanged RELEASE-PKG-001B consolidated baseline because this task does
  not modify firmware or build configuration.

## RELEASE-PKG-001D — Autonomous packaging metadata (in review)

- Keeps manifest schema 2 and introduces packaging revision 3 with exact board
  and target-specific flash parameters. Revision 1 and 2 packages remain
  verifiable under their historical rules.
- Bundles Installation, Building, User and Hardware guides at package root and
  validates their local Markdown links, while preserving the existing source,
  license, provenance, component and checksum inventories.
- Retains the ESP8266 single image at `0x0` and the ESP32 four-component layout;
  it changes neither PlatformIO configuration nor application firmware.
- Verification: 38/38 host packaging tests, both 529/529 native profile suites,
  all four firmware builds and all four temporary revision 3 packages pass.
  Repeated ESP8266 staging is byte-identical, and all 24 retained revision 1/2
  local packages remain valid under their historical rules.

## RELEASE-PKG-001B — Secure packaging implementation (in review)

- Public staging now requires a clean known commit and builds the requested
  PUBLIC environment in a detached temporary worktree at that exact revision.
- PlatformIO emits SHA-256-bound provenance evidence for each firmware build;
  staging verifies project, version, environment, profile, target, commit,
  firmware hash and embedded identity before accepting artifacts.
- Package publication uses a temporary sibling directory and atomic rename;
  non-empty destinations are rejected and failure cleanup preserves unrelated
  user files.
- Deterministic manifests inventory every component and `SHA256SUMS` covers all
  components plus the manifest. ESP32 also records the verified partition-table
  hash/layout and NVS region.
- Verification: 13/13 packaging tests and both 472/472 native profile suites
  pass; all four firmware environments build successfully. Local provenance-
  checked PUBLIC staging succeeds for ESP8266 and ESP32 and records the ESP32
  layout; the deliberately dirty review tree is correctly ineligible for the
  controlled `--public-release` path. That final clean-commit exercise is
  deferred until consolidation. No firmware upload, public distribution,
  commit or push is authorized by this task.

## Current Project Status

- The historical DOC-001C.2 checkpoint was recorded as `7f5cf3d` in the
  predecessor repository; that commit is not present in the consolidated Git
  history. Use Git HEAD for the current baseline.
- ESP8266 NodeMCU and ESP32 are primary development platforms sharing one core.
  ESP8266 has prior hardware evidence and is the planned first binary-release target;
  ESP32 remains software-verified with hardware validation pending.
- BUILD-001B.1 adds explicit DEVELOPMENT/PUBLIC PlatformIO environments,
  compile-time profile guards and exact pins for the previously resolved
  platforms and direct libraries. ESP8266 DEVELOPMENT is now the default.
  BUILD-001B.2 gives PUBLIC an empty callsign plus `JN45`, while DEVELOPMENT
  retains `IW2NNV`/`JN45`; valid saved records continue to override defaults.
  PUBLIC remains TX-inhibited until a valid identity is saved and is not yet a
  release publication. Final verification results are recorded in its review.
- BUILD-001B.3 added centralized `dev` firmware identity for serial and status
  JSON plus deterministic artifact staging. RELEASE-PKG-001B supersedes its
  reuse of existing build outputs: public staging now performs a controlled
  clean build and validates SHA-256-bound provenance before packaging. It keeps
  DEVELOPMENT/PUBLIC and ESP8266/ESP32 distinct and rejects dirty or
  DEVELOPMENT public-release input. Publication, final versioning and licensing
  remain out of scope.
- Nano Every is historical/frozen and is not an official release target; Nano
  33 IoT is unsupported.
- Seven bands exist in `BandPlan` and are manually software-selectable; 20 m
  remains the default. Selection is available through CLI/web; physical-button
  band switching is not implemented. Multiband hardware/RF validation is pending.
- `FIXED` and per-frame `RANDOM` are implemented and stored in WSPR payload v2;
  v1 records remain readable.
- RELEASE-ID-001B adds runtime support for an explicitly unconfigured operating
  callsign with fail-closed TX behavior. DEVELOPMENT remains `IW2NNV`/`JN45`;
  PUBLIC now starts with an empty callsign and `JN45`.
- RF-CTRL-001B adds shared RAM-only manual TX permission for CLI and Dashboard,
  with deliberate abort telemetry and all existing safety gates retained.
  RF-CTRL-001F supersedes its boot policy: permission defaults ON, while OFF
  remains RAM-only until explicit ON or reboot.
- FACTORY-001B.1 adds the transactional Factory Reset core. FACTORY-001B.2
  exposes it through the separately confirmed CLI and protected Configuration
  page while retaining one shared service, session invalidation, TX inhibit and
  mandatory manual reboot. Hardware validation remains pending.
- OLED startup, Hardware Check, operational rotation, `PREPARING` and `TX ON`
  are implemented. The serial CLI/monitor and persistence are operational.
- Functional NodeMCU/IC-705 observations are positive; quantitative frequency,
  power, transient, harmonic and filter measurements remain open.
- WEB-001 Dashboard, WEB-002 interactive WSPR configuration, WEB-003 AP
  Provisioning/network scan and WEB-004 AP Operational are implemented. Secure
  web sessions support candidate editing, discard and explicit WSPR-only
  persistence/application. HTTP remains experimental and best-effort; ESP32
  hardware and final AP Operational RF validation remain pending.

## Conventions

Statuses: `TODO`, `IN ANALYSIS`, `IN DEVELOPMENT`, `IN REVIEW`, `COMPLETED`,
`PARTIAL`, `SUPERSEDED`, `BLOCKED`. Priorities: P0 essential, P1 first complete
release, P2 later. Compilation alone never completes a task.

Short commit hashes and tags recorded in historical task entries may refer to
the predecessor repository and are archival identifiers only. Unless the object
is present in the current repository, they must not be treated as resolvable Git
references.

## Documentation

### DOC-001 — Operational documentation [P1 · COMPLETED]
Created the original user manual and hardware guide, linked them from README,
and established mandatory operational-documentation maintenance in AGENTS.

### DOC-001B — English User Guide [P1 · COMPLETED]
Created `docs/USER_GUIDE.md`, redirected `USER_MANUAL.md`, and verified CLI,
OLED/RF behavior, links and whitespace. Consolidated at `a00097e`.

### DOC-001C.1 — English technical documentation alignment [P1 · COMPLETED]
Aligned README, PROJECT and HARDWARE in English, separated current behavior
from superseded decisions, and verified links. The predecessor-repository
checkpoint was recorded as `7f5cf3d`; it is not in the current Git history.

### DOC-001C.2 — English development instructions and task history [P1 · COMPLETED]
Translate and align AGENTS/TASKS without changing development policy, losing
safeguards or removing historical evidence.

### DOC-INSTALL-001B — Documentation and cross-platform installation [P1 · IN REVIEW]
Restructures README as the project entry point, retains the User Guide as the
authoritative manual, adds Linux/macOS/Windows source and provisional binary
installation guidance, and extends PROJECT with compact boot, eligibility,
persistence, Factory Reset, HTTP-profile and OLED-priority references. It also
records the unresolved license/public-build blockers and corrects only the
current project-status summary; firmware, PlatformIO environments and historical
records are unchanged. Hardware validation of macOS, Windows, ESP32 and the
published-binary workflows remains pending.

## Console

### CONSOLE-002A — Status and serial interface [P1 · COMPLETED]
Added compact snapshots, structured status views, prompt/help and readable
errors. Evidence: 126 native tests; ESP8266/ESP32 builds; ESP8266 33,044 B RAM /
328,615 B flash, ESP32 48,636 B / 833,037 B. NodeMCU upload succeeded; the
interactive check was still pending at closure.

### CONSOLE-002B — Guided menu [P1 · COMPLETED]
Added non-blocking numeric menus using shared validation and persistence.
Evidence: 132 tests and both builds; ESP8266 33,120/332,775 B, ESP32
48,636/836,721 B. NodeMCU menu, masked password, candidate retention and error
handling verified without reset.

### CONSOLE-002C — TX telemetry [P1 · COMPLETED]
Centralized started/completed/interrupted frames and inhibit reasons; added
`tx status`/`tx last`. Evidence: 139 tests; both builds; ESP8266
33,364/334,787 B, ESP32 48,716/838,693 B. NodeMCU serial verification passed;
no RF test was performed.

### CONSOLE-003 — Echo and line editing [P1 · COMPLETED]
Added single echo, Backspace/DEL, CR/LF/CRLF handling, blind passwords and TX
suspension. Evidence: 145 tests; both builds; RAM unchanged, flash +356 B
ESP8266/+364 B ESP32. NodeMCU interaction passed; no persistent write or RF test.

### CONSOLE-004A — Readable serial monitor [P1 · COMPLETED]
Added aligned sections and flash-based ESP8266 formatting without snapshot
changes. Evidence: 216 tests; both builds. Versus `00b47ba`: ESP8266 +20 B RAM,
+9,068 B flash (34,520/352,763 B); ESP32 RAM unchanged, +3,064 B flash
(48,748/844,785 B). Optimized NodeMCU reports, monitor control and post-TX
recovery verified; no persistent change or manual TX.

### CONSOLE-004B — Correct frequency-mode CLI help [P1 · COMPLETED]
Replace the obsolete `(RAM only)` help suffix with explicit guidance that the
RF-safe mode change is applied in RAM and requires `config save` to persist.
Command syntax, execution, persistence and response tokens remain unchanged.
Evidence: 217 native tests and ESP8266/ESP32 builds passed. Versus CONSOLE-004A,
RAM is unchanged; flash is +32 B on each target (ESP8266 34,520/352,795 B,
ESP32 48,748/844,817 B). No hardware upload was required.

## OLED and RF architecture

### OLED-001B — Startup banner [P1 · COMPLETED]
Added the tower/banner for five seconds. Evidence: 148 tests/both builds;
31×57 px graphic; ESP8266 33,440/335,979 B, ESP32 48,732/839,841 B; NodeMCU
visual verification passed.

### OLED-001C — Controlled startup and LED test [P1 · COMPLETED]
Added non-blocking five-second TX inhibition and solid LED. Evidence: 150 tests,
both builds; ESP8266 33,432/335,947 B, ESP32 48,724/839,817 B; physical LED
verification passed.

### OLED-001D — Persistent AP Provisioning credentials [P1 · COMPLETED]
Gives the AP Provisioning credentials page priority over Hardware Check OLED
rendering while the checks continue internally. Provisioning error remains a
separate fail-closed page; AP Operational, NORMAL, configuration and RF-critical
screens are unchanged. Evidence: 447/447 native tests and successful ESP8266/
ESP32 builds; ESP8266 uses 42,572 B RAM and 482,091 B flash, while ESP32 uses
52,700 B RAM and 984,849 B flash. Controlled NodeMCU verification confirmed
successful upload, credentials visible for at least 30 seconds, provisioning
page access at `192.168.4.1` and no unexpected reset during a 45-second serial
observation. Hardware Check completion was not directly inspected on hardware.

### OLED-002B — Hardware Check [P1 · COMPLETED]
Added sequential OLED/GPS/Si5351A/LED checks and three-second result hold.
Evidence: 155 tests/both builds; ESP8266 33,504/336,771 B, ESP32
48,748/840,617 B; NodeMCU verification passed.

### OLED-002C — GPS/Wi-Fi/Time screen [P1 · COMPLETED]
Added five-line live status with lost-fix, locator-source and Wi-Fi transitions.
Evidence: 161 tests/both builds; ESP8266 33,884/337,947 B, ESP32
48,868/841,921 B; visual/functional NodeMCU verification passed.

### OLED-002D — IDLE screen and rotation [P1 · COMPLETED]
Added WSPR state page and deterministic five-second rotation. Evidence: 173
tests/both builds; ESP8266 34,156/339,531 B, ESP32 48,996/843,525 B; hardware
verification passed.

### OLED-002E — Safe TX screen [P1 · COMPLETED]
Introduced immutable `PREPARING` snapshot and one-time `TX ON` rendering with
500 ms margin. Evidence: 185 tests/both builds; ESP8266 34,260/340,523 B, ESP32
49,020/844,473 B; NodeMCU visual verification passed; render duration was not
instrumented.

### RF-ARCH-002 — Remove RF warm-up [P0 · COMPLETED]
Established `IDLE → PREPARING (RF OFF) → TRANSMITTING → IDLE`, preprogrammed
symbol 0 with CLK0 disabled, and enabled only CLK0 at the slot. Evidence: 193
tests/both builds; ESP8266 34,308/341,375 B, ESP32 49,036/845,273 B. NodeMCU
display sequence and functional IC-705 observation passed. Quantitative RF
frequency, level, transient and early-emission measurements remain open.

## Band and persistence evolution

### RF-BAND-002A — BandPlan and multiband validation [P1 · COMPLETED]
Defined seven bands and four-tone validation; only 20 m TX-enabled. Evidence:
199 tests/both builds; versus `47c3112`, ESP8266 +128 B RAM/+408 B flash,
ESP32 RAM unchanged/+488 B. WSPR v1 remained unchanged; hardware check pending.

### RF-BAND-002B — Pseudorandom WSPR frequency [P1 · COMPLETED]
Added RAM-only-at-that-stage RANDOM selection, once per frame, including zero-
seed protection. Evidence: 207 tests/both builds; ESP8266 34,496/342,491 B;
ESP32 48,748/840,533 B. Boot FIXED and console RANDOM verified with OLED and
Si5351A intentionally disconnected; RF RANDOM verification remained pending.

### RF-BAND-002C — Frequency mode status [P1 · COMPLETED]
Aligned OLED, console and telemetry without changing RF. Evidence: 208 tests/
both builds; ESP8266 34,500/342,687 B, ESP32 48,748/840,665 B; no hardware test.

### RF-BAND-002D.1 — Unified band/mode configuration [P1 · COMPLETED]
Made SAVED/CANDIDATE/ACTIVE the shared model while v1 still could not persist
band/mode. Evidence: 210 tests/both builds; versus `99da6d7`, RAM unchanged,
flash +924 B ESP8266/+960 B ESP32; no upload.

### RF-BAND-002D.2 — WSPR v2 persistence [P1 · COMPLETED]
Added band/mode in the unchanged 22-byte payload/40-byte record, v1 reading and
explicit migration. Evidence: 213 tests/both builds; versus `b794160`, RAM
unchanged, flash +84 B ESP8266/+96 B ESP32. Native tests verified v1→v2;
NodeMCU verified DEFAULT→v2, RANDOM/FIXED saves and reboots while preserving
WSPR/Wi-Fi. No extra persistent writes followed.

### RF-BAND-003B — Manual multiband software selection [P1 · IN REVIEW]
Enables the seven existing stable BandPlan IDs for manual CLI/web selection,
keeps 20 m as the default, resets a changed band's candidate base atomically to
its nominal frequency, makes legacy validation and RANDOM band-aware, and
retains the WSPR v2 22-byte payload/40-byte record. Hardware upload, RF output,
filter validation and regulatory authorization are outside this task. Evidence:
397/397 native tests and ESP8266/ESP32 builds passed. Versus `19b5278`, ESP8266
uses 41,096 B RAM and 451,579 B flash (−4/+808 B), while ESP32 uses 52,524 B
RAM and 952,449 B flash (+0/+872 B). The deterministic Configuration gzip asset
is 8,726 B for a 30,966 B source. No hardware upload or RF test was performed.

## Baseline and core history

### T-001 — Consolidate Git baseline [P0 · COMPLETED]
Consolidated documentation and `nodemcuv2`; ESP8266 build passed. Hardware/RF
validation was deferred to dedicated tasks.

### T-002 — Targeted architecture review [P0 · COMPLETED]
Approved incremental common core, separate UTC/location, monotonic timing,
compact snapshots, uniform diagnostics and conditional platform adapters.

### T-003 — Hardware-independent logic tests [P0 · COMPLETED]
Extracted NMEA/Maidenhead without Arduino dependencies. Evidence: 9 tests and
Nano Every/ESP8266/ESP32 builds; RAM unchanged; flash −22/+160/+124 B. NodeMCU
boot passed; physical peripherals/RF were then unverified.

### T-004A — Central diagnostics registry [P0 · COMPLETED]
Added 72 B registry/snapshot. Evidence: 9 diagnostic tests plus NMEA regression;
three builds; 72 B RAM and 208/96/100 B flash cost.

### T-004B — Diagnostic/runtime console integration [P0 · COMPLETED]
Evidence: 19 tests/three builds; NodeMCU without peripherals correctly reported
waiting/not detected/disabled, with no reset in 30 s. Flash deltas:
Nano +2,098 B, ESP8266 +2,420 B, ESP32 +2,260 B.

### T-004C — Nano Every memory analysis [P0 · COMPLETED]
Measured 1,539 B static RAM, 44,525 B flash and 4,115 B remaining; identified
the Adafruit splash as a low-risk saving. OLED framebuffer is dynamic 1,024 B.

### T-004D — Minimal Nano Every flash optimization [P0 · COMPLETED]
Disabled only the Adafruit splash. Evidence: 19 tests/three builds; Nano flash
42,959 B (−1,566), RAM 1,539 B, 5,681 B remaining.

### T-007A.1 — Si5351A fault TX inhibition [P0 · COMPLETED]
Added RF safety and explicit `rf recover`. Evidence: 75 tests/both builds;
NodeMCU without Si5351A confirmed safe `NOT_DETECTED`, `RF INHIBITED` and failed
recovery. Positive software state remains `DEGRADED/RF_UNVERIFIED`.

### T-007A.2 — Status LED controller [P0 · COMPLETED]
Added prioritized non-blocking patterns. Evidence: 81 tests/both builds;
ESP8266/ESP32 +16 B RAM, +592/+696 B flash; NodeMCU RF-error pattern verified.

### LED-001 — External ESP8266 status LED [P1 · SUPERSEDED]
The uncommitted active-high external LED proposal was replaced by LED-001B
after software/build verification and a NodeMCU upload. It was never
consolidated and is not the intended final behavior.

### LED-001B — Dual onboard ESP8266 status indicators [P1 · COMPLETED]
Keep the active-low D4/GPIO2 primary status patterns and use the active-low
onboard D0/GPIO16 LED for qualified time authority: off without authority,
slow blink for NTP, steady for GPS/PPS, shared Hardware Check flashes, and no
GPIO transitions during `PREPARING`/TX. Other platforms retain one status LED.
Evidence: 225/225 native tests and both platform builds passed; ESP8266 uses
34,528 B RAM and 353,227 B flash (+8/+432 B versus `e55971c`), while ESP32 is
unchanged at 48,748/844,817 B. Physical verification of both onboard LEDs on
the NodeMCU passed.

## Time, location and platform history

### T-005A — Hardware-independent TimeService [P0 · COMPLETED]
Added 12 B snapshots and configurable timeouts. Evidence: 28 tests/three builds;
no linked footprint at that stage because the service was not instantiated.

### T-005B — Parallel GPS/TimeService integration [P0 · COMPLETED]
Evidence: 34 tests/three builds; Nano +37 B RAM/+1,316 B flash, ESP8266
+40/+1,400 B, ESP32 +40/+1,424 B. NodeMCU no-source diagnostics passed; real
GPS/PPS remained unverified.

### T-005C.1 — Initial SNTP provider [P0 · COMPLETED]
Added non-blocking native SNTP, diagnostic-only at that stage. Evidence: 42
tests/both builds; ESP8266 +232 B/+2,640 B, ESP32 +1,616 B/+22,124 B. Later
tasks superseded the diagnostic-only limitation.

### T-005D.1 — Scheduler migration to TimeService [P0 · COMPLETED]
Moved slot arming to TimeService and monotonic deadlines; GPS-only authority was
historical. Evidence: 110 tests/both builds; ESP8266 +20 B/+392 B, ESP32
+8 B/+472 B; no hardware/RF test.

### T-005D.2 — GPS locator behavior [P0 · COMPLETED]
Added `FIXED`, `GPS_LIVE`, `GPS_LAST_KNOWN` and immutable frame locator.
Evidence: 116 tests/both builds; ESP8266 −4 B RAM/+396 B flash, ESP32
+16 B/+352 B; no real GPS test.

### T-005D.3 — Experimental NTP TX [P0 · COMPLETED]
Added GPS-priority/NTP fallback and late-slot rejection. Evidence: 122 tests/
both builds; flash +108 B ESP8266/+92 B ESP32, RAM unchanged. NodeMCU showed
`NTP_EXPERIMENTAL` and correct RF inhibition without Si5351A; no RF test.

### T-005 — Scheduler and RF safety umbrella [P0 · PARTIAL]
Superseded as an implementation umbrella by T-005D.*, T-007A.1 and RF-ARCH-002.
Software sequencing is implemented; quantitative timing and RF acceptance
remain open under T-012.

### T-006 — Time/Location Service umbrella [P0 · PARTIAL]
Core behavior is implemented by T-005A–D.3. Remaining scope is physical GPS/PPS
and cross-platform validation, not a second service implementation.

### T-007 — Multiplatform verification [P0 · PARTIAL]
ESP8266/ESP32 builds are routine and both are primary development targets;
ESP32 hardware and Nano 33 IoT remain open. Nano Every is frozen and not a gate
for new features.

## Configuration history

### T-006A.1 — Volatile WSPR console [P0 · COMPLETED]
Added parser/candidates/validation. Evidence: 58 tests/both builds. RAM-only and
non-applied limitations were historical and superseded by persistence tasks.

### T-006A.2 — Volatile Wi-Fi console [P0 · COMPLETED]
Added asynchronous scan/connect and blind passwords. Evidence: 64 tests/both
builds; NodeMCU scan, RAM credentials, connection and SNTP passed. Persistence
and NTP TX were added later.

### T-006A.3 — MONITOR/INTERACTIVE modes [P0 · COMPLETED]
Added immediate mode switch, explicit monitor commands and TX suspension.
Evidence: 68 tests/both builds; NodeMCU input/password/mode behavior passed.

### T-006B.1 — Persistent configuration core [P1 · COMPLETED]
Added explicit versioned records, CRC32, sequence, separate types and manager.
Evidence: 92 tests (11 persistence cases), both builds; ESP8266
32,080/317,555 B, ESP32 47,868 B RAM and +344 B flash. No runtime backend then.

### T-006B.2 — ESP8266/ESP32 storage backends [P1 · COMPLETED]
Evidence: 93 tests/both builds; +16 B/+48 B ESP8266, ESP32 RAM unchanged/
+1,292 B flash. NodeMCU write/read/CRC/sequence/corrupt-slot recovery passed and
original EEPROM was restored. ESP32 physical verification remains open.

### T-006B.3A — Runtime, console and Wi-Fi persistence [P1 · COMPLETED]
Evidence: 96 tests/both builds; ESP8266 33,020/324,243 B, ESP32
48,604/828,553 B. NodeMCU credentials, reboot persistence, reconnect, SNTP,
show/discard/no-op save passed. Nano Every exceeded flash by 3,300 B.

### T-006B.3B — Active WSPR settings [P1 · COMPLETED]
Evidence: 101 tests/both builds; ESP8266 33,008/325,155 B, ESP32
48,612/829,477 B. NodeMCU SAVED/CANDIDATE/ACTIVE, persistent Wi-Fi, NTP and safe
Si5351A-absent inhibition passed; no RF test.

### T-008 — Configuration model and USB console umbrella [P0 · SUPERSEDED]
Implemented incrementally by T-006A.*, T-006B.*, CONSOLE-002* and CONSOLE-003.
Universal parity on frozen/future platforms is not claimed.

### T-009 — Versioned persistence umbrella [P1 · PARTIAL]
Implemented on ESP8266/ESP32 through T-006B.* and RF-BAND-002D.2. ESP32 physical
verification and storage for future platforms remain open.

### FACTORY-002B — Post-reset Wi-Fi recovery [P1 · IN REVIEW]
Allows a valid Wi-Fi record written after a completed Factory Reset to load on
the next boot while WSPR remains explicitly unconfigured and the completion
marker remains present. In-progress or ambiguous recovery still loads neither
domain; existing A/B formats, CRC, offsets, sequence policy and marker clearing
are unchanged. Evidence: 21/21 focused Factory Reset tests within 452/452
passing native tests and successful ESP8266/ESP32 builds. ESP8266 uses 42,572 B
RAM and 482,235 B flash; ESP32 uses 52,700 B RAM and 984,969 B flash. Controlled
NodeMCU verification retained the completed-reset marker, recovered the saved
post-reset Wi-Fi record, connected through STA at `192.168.1.71`, synchronized
NTP and served the HTTP API while the WSPR identity remained unconfigured and
TX remained inhibited with zero frames started. The flash digest matched and no
unexpected reset occurred during 50 seconds of observation with the physical RF
path isolated. No new Wi-Fi credential save was performed during that hardware
test, and ESP32 hardware validation remains pending.

## Network and RF roadmap

### UI-LANG-001B — English web interface and project identity [P1 · IN REVIEW]
Separates the `WSPR Beacon 2.0` title from the `Developed by IW2NNV / AC2WZ`
credit across Dashboard, Configuration and AP Provisioning. User-visible web
text is consistently English, manual TX terminology is aligned, and the AP
Operational explanation states its qualified GPS/PPS and RF-safety gates
without implying automatic transmission. No API, persistence, scheduler or RF
safety behavior is changed. Hardware validation remains pending.

### UI-CLI-001C.1 — Shared TX eligibility reporting [P1 · IN REVIEW]
Uses one read-only eligibility calculation for Dashboard/API status, CLI
`tx status` and immediate TX mutation responses. The calculation combines
manual permission, RF-capable mode, configured identity, RF readiness,
Configuration-session and resume-guard inhibition, plus the existing
mode-aware time qualification (including `GPS_PRECISE` in AP Operational).
Actual transmitter state remains separate and no RF admission, scheduler,
timing or persistence behavior is changed. Evidence: 80/80 focused cases and
463/463 complete native cases passed; ESP8266 uses 42,572 B RAM and 483,195 B
flash, while ESP32 uses 52,700 B RAM and 985,997 B flash. Hardware validation
remains pending.

### UI-CLI-001C.2 — Guided-menu frequency mode [P1 · IN REVIEW]
Adds FIXED/RANDOM selection to the guided serial WSPR menu through the existing
frequency-mode command and `ConfigurationService`. A successful selection
updates ACTIVE and CANDIDATE in RAM, leaves SAVED unchanged until `config save`,
and does not perform the per-frame RANDOM selection before `PREPARING`.
Configuration ownership and RF-busy rejection remain authoritative. Evidence:
89/89 focused cases and 465/465 complete native cases passed; ESP8266 uses
42,604 B RAM and 483,823 B flash, while ESP32 uses 52,700 B RAM and 986,693 B
flash. Hardware validation remains pending.

### T-011 / WEB-001 — Read-only HTTP dashboard [P1 · COMPLETED]
The RF-safe transport, schema-v1 JSON API and responsive dashboard are
implemented by WEB-001B through WEB-001D. The service is read-only,
experimental and best-effort; RF isolation has priority over HTTP delivery.
HTTP configuration is tracked separately by WEB-002.

### WEB-001B — Bounded HTTP transport and RF-safe lifecycle [P1 · COMPLETED]
Added a dependency-free, single-client HTTP transport for ESP8266/ESP32 with
fixed request buffers, incremental input/output, 600 ms client lifetime and a
750 us application service budget. Preliminary read-only `GET /` and
`GET /api/status` responses are available only outside the scheduler-derived
T−7 s guard, `PREPARING` and TX; this is transport scaffolding, not the final
dashboard. Evidence: 21 focused transport cases within 246/246 passing native
tests; both platform builds passed. ESP8266 uses 35,704 B RAM and 364,431 B
flash (+1,176/+11,204 B versus LED-001B); ESP32 uses 49,204 B RAM and
864,653 B flash (+456/+19,836 B). Hardware verification confirmed upload,
routes, bounded clients, slow-client recovery and one completed scheduled frame;
post-TX HTTP recovery was observed only after an approximately 10 s interval
and led to the internal diagnostic, hardening and simplification stages below.
The consolidated service is experimental and best-effort: RF isolation and
recovery take priority over perfect HTTP delivery. Precise timing remains
pending.

### WEB-001B.1 — HTTP lifecycle diagnostics [P1 · COMPLETED, internal stage]
Added passive, saturating diagnostics for HTTP gate reasons/transitions,
listener start/stop intent, client outcomes, first completed response after a
restart, and separately observed physical/tracked Wi-Fi state. The compact
`[HTTP]` console section remains suppressed during `PREPARING`/TX. No HTTP,
Wi-Fi retry or RF policy changed. Evidence: 251/251 native tests and both
platform builds passed; versus WEB-001B, ESP8266 uses 35,608 B RAM and 366,639 B
flash (−96/+2,208 B), while ESP32 uses 49,268 B RAM and 866,633 B flash
(+64/+1,980 B). Hardware diagnosis remains pending.

### WEB-001B.3 — TCP transport hardening [P1 · SUPERSEDED, internal stage]
Added an explicit response `DRAINING` state, separate queued/drained/closed/abort
diagnostics, verified listener startup with bounded retry, backlog 1 and pending
client disposal. The ESP8266 adapter bypasses the core's blocking implicit
flush on forced abort and uses incremental send-buffer acknowledgement checks;
the 600 ms lifetime, 750 us application budget, T−7 s guard and RF gate remain
unchanged. ESP32 preserves the state machine but cannot claim ACK-level drain
verification through its current socket wrapper. Evidence: 256/256 native
tests and both platform builds passed. Versus WEB-001B.1, ESP8266 uses
35,632 B RAM and 367,703 B flash (+24/+1,064 B); ESP32 uses 49,292 B RAM and
867,473 B flash (+24/+840 B). Later NodeMCU verification completed 17/30
sequential requests successfully, with 10 connection timeouts, two response
timeouts and one reset after a complete response; fragmented/incomplete request
tests also caused resets, while the scheduled WSPR frame completed and HTTP
recovered after TX. These mechanisms were superseded by WEB-001B.4/B.5; this
evidence remains historical context.

### WEB-001B.4 — HTTP architecture simplification review [P1 · COMPLETED]
Compared a reduced custom `WiFiServer` transport with the framework standard
HTTP servers. Recommended retaining the fixed-buffer single-client transport
and T−7 RF gate while removing ACK-level delivery claims, private ESP8266 core
access, raw ESP32 socket handling and PCB-oriented diagnostics. The dashboard
is explicitly best-effort; RF isolation and recovery take priority over perfect
HTTP delivery.

### WEB-001B.5 — Simplified HTTP transport [P1 · COMPLETED, internal stage]
Replaced the B.3 TCP close/drain machinery with ordinary portable
`WiFiServer`/`WiFiClient` operations. The core retains fixed buffers,
incremental I/O, one client, the 600 ms lifetime and the scheduler-derived T−7
gate. Diagnostics are limited to gate/listener intent, accepted clients, served
responses, timeouts, failed requests and forced closures. A served response
means bytes were submitted to the network stack, not browser receipt. Software
verification: 252/252 native tests and both platform builds passed. Versus
WEB-001B.3, ESP8266 uses 35,576 B RAM and 365,607 B flash (−56/−2,096 B);
ESP32 uses 49,236 B RAM and 865,809 B flash (−56/−1,664 B). Hardware
verification found correct upload/read-back, timeout recovery and a completed
scheduled frame, but only 19/30 HTTP requests succeeded with periodic serial
monitoring. With `monitor off`, the same test improved to 26/30, with four TCP
connection timeouts and no response timeouts. The simplified transport is the
consolidated implementation; its reliability remains explicitly experimental
and best-effort.

### CONSOLE-WEB-001 — Default monitor off and HTTP coordination [P1 · COMPLETED]
Makes `INTERACTIVE` the startup default, prints the `wspr> ` prompt and requires
an explicit `monitor on` for five-second reports. When monitoring is enabled,
the main loop services HTTP before the periodic report and defers that report
while a client was or remains active; interactive command responses are never
deferred. No persistence, TCP transport or RF policy changes. Software
verification: 254/254 native tests and successful ESP8266/ESP32 builds.
Relative to WEB-001B.5, RAM is unchanged on both targets; flash changes by
+32 B on ESP8266 (365,639 B) and +44 B on ESP32 (865,853 B). NodeMCU hardware
verification confirmed default `INTERACTIVE`, automatic prompt, monitor
control, one completed natural frame and HTTP recovery after TX. The final
30-request runs completed 23/30 with monitoring OFF and 27/30 with monitoring
ON; successful responses had valid JSON and matching `Content-Length`.

### WEB-001B.6 — HTTP Gate Age diagnostic fix [P1 · COMPLETED, internal stage]
Corrects the diagnostic-only unsigned age wrap observed in the first report
after TX. The HTTP transition can be recorded with a later `millis()` sample
than the loop timestamp previously passed to the console. Diagnostic capture
now samples after HTTP service; transition-timestamp validity is explicit,
including a valid timestamp of zero, and age formatting rejects a timestamp
that is later than the observation while preserving normal `millis()`
wraparound. HTTP authorization, transport and RF behavior are unchanged.
Software verification: 257/257 native tests and successful ESP8266/ESP32
builds. Relative to CONSOLE-WEB-001, RAM is unchanged on both targets; flash
changes by +64 B on ESP8266 (365,703 B) and +84 B on ESP32 (865,937 B).
Hardware confirmation of the corrected first post-TX report remains pending.

### WEB-001C — Application status snapshot and JSON API [P1 · COMPLETED]
Replaces the provisional `/api/status` body with schema version 1 composed on
demand from authoritative time, locator, transmitter, telemetry, GPS, Wi-Fi
and module-diagnostic snapshots. Serialization uses a fixed 2048-byte buffer,
escapes strings, emits explicit units and `null` values, excludes credentials
and returns a complete 503 JSON error rather than truncating on overflow.
Collection remains behind the existing T−7/PREPARING/TX gate; `/` remains the
placeholder page and no configuration route or dashboard is added. Software,
build and memory evidence: 266/266 native tests and successful ESP8266/ESP32
builds. Relative to WEB-001B, ESP8266 uses 39,312 B RAM and 371,175 B flash
(+3,736/+5,472 B); ESP32 uses 51,308 B RAM and 872,249 B flash
(+2,072/+6,312 B). The fixed response buffer accounts for 2,049 B per Wi-Fi
target; additional ESP8266 static growth comes primarily from JSON literals.
NodeMCU verification passed: upload/hash verification, startup, console and
Wi-Fi were normal; eight IDLE requests returned complete parseable schema-v1
JSON with matching `Content-Length` and no credential fields. A natural frame
completed 1/1 with no interruption, last-frame RANDOM frequency 14,097,186 Hz,
and HTTP resumed automatically afterward. HTTP remains experimental and
best-effort; no deliberate requests were sent during PREPARING/TX and precise
RF timing remains outside HTTP/serial verification.

### WEB-001C.1 — ESP8266 JSON flash-string optimization [P1 · COMPLETED]
Moves the WEB-001C JSON keys, enumerated labels and module names to ESP8266
program flash through a portable flash-aware writer path. The schema, fixed
2049-byte response storage, application snapshot and HTTP/RF lifecycle remain
unchanged. Native tests retain a byte-level output fingerprint and cover every
diagnostic state/reason. Evidence: 267/267 native tests and both builds passed.
Relative to the unoptimized WEB-001C build, ESP8266 saves 1,600 B RAM and adds
400 B flash (37,712/371,575 B); ESP32 RAM is unchanged and flash adds 100 B
(51,308/872,349 B). Flash-string output was exercised successfully by the
NodeMCU `/api/status` verification above.

### WEB-001D — Responsive web dashboard [P1 · COMPLETED]
Replaces the root placeholder with a self-contained, mobile-first read-only
dashboard backed exclusively by schema-v1 `GET /api/status`. The page shows
identity, TX state, scheduler-derived next slot and estimated countdown,
configured/selected frequency semantics, last-frame telemetry, time, network,
RF status and expandable module diagnostics. Polls run serially every three
seconds with a bounded browser timeout; retained values are explicitly marked
`STALE` after about six seconds and `UNAVAILABLE` after prolonged loss. API
values are assigned through `textContent`, no external assets or controls are
present, and the browser never participates in scheduling or RF decisions.
The HTML/CSS/JavaScript asset is streamed from program flash without a full-page
RAM copy. The existing one-client transport, 600 ms lifetime, T−7 gate and
`PREPARING`/TX exclusion remain unchanged. Software evidence: 277/277 native
tests and successful ESP8266/ESP32 builds. The page is 10,730 B. ESP8266 uses
37,504 B RAM and 382,223 B flash (−208/+10,648 B versus WEB-001C.1); ESP32 uses
51,308 B RAM and 883,025 B flash (+0/+10,676 B). NodeMCU verification confirmed
complete page delivery, 320/360 px headless layout, intentional HTTP suspension,
a completed natural frame and automatic post-TX recovery; physical smartphone,
touch and dark-theme confirmation remain pending. HTTP delivery retains the
documented experimental best-effort limitation.

Final consolidated verification after D.1–D.9: 296/296 native tests and both
platform builds passed; ESP8266 uses 38,080 B RAM and 386,275 B flash, ESP32
uses 51,348 B RAM and 887,117 B flash, and the dashboard asset is 14,389 B.
NodeMCU upload/read-back passed. Five of five dashboard responses were complete
and matched SHA-256
`efe004fe29eb7aaae76e51f2bccf12dc3d1d49fbc581e8755cc9fe3f74900a00`;
ten of ten JSON responses were complete, parseable schema 1. Browser checks
confirmed the responsive dashboard, last-frame identity/frequency and GPS
card. The estimated countdown was observed during the earlier D.1 browser
verification and is covered by native tests; D.9 did not instrument RF timing,
and physical-smartphone confirmation remains pending. A natural WSPR frame
completed without reset, watchdog or RF error; HTTP stopped before the
critical interval and resumed automatically afterward. HTTP delivery remains
best-effort, and precise CLK0/symbol timing still requires RF instrumentation.

### WEB-001D.1 — Dashboard UI polish and estimated TX countdown [P1 · COMPLETED, internal stage]
Keeps the API and RF lifecycle unchanged while translating
`COMPLETED_BY_FIRMWARE` to the display-only label `Completed`, presenting
RANDOM as the primary frequency mode and keeping configured, selected-frame and
last-frame frequencies distinct. A browser-only estimate uses the last eligible
scheduler snapshot and `performance.now()` to show `RF window approaching`,
`Preparing expected`, `TX expected` and `Awaiting telemetry`. During the nominal
110.6 s frame window it displays estimated remaining time and bounded progress;
it never infers firmware completion or calculates another slot. Estimation is
suppressed for missing/inhibited/unqualified snapshots and after monotonic timer
discontinuity. Evidence: 281/281 native tests and successful ESP8266/ESP32
builds. The page is 12,745 B. RAM is unchanged; versus WEB-001D, flash increases
by 2,016 B on both ESP8266 (37,504/384,239 B) and ESP32
(51,308/885,041 B). NodeMCU/browser verification observed the local countdown,
progress, stale/unavailable treatment, completed frames, authoritative recovery
and last RANDOM frequency without inventing TX state. The short `Preparing
expected` and `Awaiting telemetry` phases were not directly sampled; physical
smartphone and dark-theme checks remain pending. Best-effort HTTP still
occasionally delivered an incomplete response and recovered without reload.

### WEB-001D.2 — Dashboard identity, WSPR frame and GPS information [P1 · COMPLETED, internal stage]
Refines only the flash-resident dashboard: the fixed project heading is
`WSPR Beacon 2.0 — IW2NNV / AC2WZ`, while operational callsign, locator and
band remain schema-v1 data. The frequency card distinguishes configured and
last-frame frequency and removes the prepared/current selection row. A compact
GPS card consumes existing `gps`, `identity` and `modules.gps` fields to keep
NMEA, fix, satellites, coordinates, PPS presence/association, locator source
and diagnostics semantically separate. Schema v1 does not contain the complete
immutable last transmitted message: `Last frame` therefore remains `--` rather
than being reconstructed from mutable current configuration. No API, snapshot,
telemetry or RF-critical code is changed. Evidence: 284/284 native tests and
successful ESP8266/ESP32 builds. The page is 14,162 B. Compared with WEB-001D.1,
RAM is unchanged; flash grows by 1,424 B on ESP8266 (37,504/385,663 B) and ESP32
(51,308/886,465 B). Hardware verification is pending.

### WEB-001D.3 — Last WSPR frame telemetry [P1 · COMPLETED, internal stage]
Extends the existing last-frame telemetry with the callsign, locator and
declared power copied from the immutable prepared message before TX starts.
Completed and interrupted frames retain the same identity/frequency association;
later configuration or preparation cannot rewrite the previous frame. Schema 1
is extended additively with `tx.last.callsign`, `locator` and `power_dbm`, using
`null` before a frame exists. The dashboard formats only those authoritative
fields as `CALLSIGN LOCATOR POWER`; no history, persistence or RF-loop work is
added. Evidence: 285/285 native tests and successful ESP8266/ESP32 builds.
`TxFrameSnapshot` grows from 20 to 28 B, `TxTelemetrySnapshot` from 36 to 44 B,
the transmitter's `TxTelemetry` storage from 76 to 100 B, and the on-demand
`ApplicationStatusSnapshot` from 220 to 228 B. Versus WEB-001D.2, ESP8266 uses
37,528 B RAM and 386,047 B flash (+24/+384 B); ESP32 uses 51,332 B RAM and
886,885 B flash (+24/+420 B). The dashboard asset is 14,389 B. Hardware
verification is pending.

### WEB-001D.4 — HTTP response truncation analysis [P1 · COMPLETED]
Identified that the 600 ms total client lifetime and 200 ms inactivity timeout
were also applied while submitting responses. The 14,389-byte dashboard needs
at least 113 body-write iterations at the 128-byte per-update limit; observed
truncation sizes at varying 128-byte boundaries and timeout-heavy diagnostics
support premature application timeout rather than a fixed 6,016-byte TCP
limit. The analysis recommended separate bounded request and response clocks
while retaining unconditional T−7 gate precedence.

### WEB-001D.5 — Separate HTTP request and response deadlines [P1 · COMPLETED, internal stage]
Retains the existing 600 ms total and 200 ms inactivity limits only for request
reception. Response submission starts independent wrap-safe clocks: 2,000 ms
for JSON, 5,000 ms for the flash-resident dashboard and 1,000 ms without
successfully submitted bytes. Partial writes reset only the no-progress clock;
they never extend the total response deadline. Request and response timeout
counts are distinguished while preserving the aggregate counter. The T−7 gate
still closes clients immediately and overrides every transport deadline.
Evidence: 292/292 native tests and successful ESP8266/ESP32 builds. Versus
WEB-001D.3, ESP8266 uses 37,544 B RAM and 386,227 B flash (+16/+180 B); ESP32
uses 51,348 B RAM and 887,049 B flash (+16/+164 B). Hardware response-delivery
verification remains pending.

### WEB-001D.6 — HTTP flash delivery architecture review [P1 · COMPLETED]
The installed ESP8266 Core 3.1.2 implementation showed that `write_P()` routes
flash data through a generic stream helper using small temporary copies, while
`availableForWrite()` reports only the TCP send-buffer byte count. The review
recommended retaining the existing RF-gated transport but replacing that path
with one explicit MSS-sized flash-to-RAM staging buffer. It also confirmed that
the installed ESP32 `WiFiClient` does not expose meaningful send capacity and
that its public write API can still block internally under backpressure.

### WEB-001D.7 — Bounded flash-to-TCP staging [P1 · COMPLETED, internal stage]
Replaces ESP8266 dashboard `write_P()` submission with one reusable 536-byte
buffer populated by `memcpy_P()`, followed by at most one ordinary TCP write per
main-loop service step. Send capacity is checked before copying; zero or
insufficient capacity causes no busy wait, and the response offset advances
only by the byte count accepted by the framework. A 50 ms ESP8266 client write
timeout bounds unexpected API waiting, normal TCP coalescing is retained and
all response/RF-gate deadlines remain unchanged. ESP32 uses the same 536-byte
incremental limit without treating its non-informative zero
`availableForWrite()` result as permanent backpressure; its framework write can
still block internally and remains a documented platform limitation. Software
evidence: 44/44 focused transport cases within 296/296 passing native tests and
successful ESP8266/ESP32 builds. Hardware page-delivery verification is
not accepted: only 1/5 HTML and 9/10 JSON responses were complete, although the
complete HTML body matched the embedded SHA-256. The T−7 gate, two natural
completed frames and post-TX recovery behaved correctly. Relative to
WEB-001D.5, ESP8266 uses 38,080 B RAM and 386,259 B flash (+536/+32 B); ESP32
uses 51,348 B RAM and 887,133 B flash (+0/+84 B).

### WEB-001D.8 — HTTP response closure analysis [P1 · COMPLETED]
Matched the observed 2.005 s JSON truncation and several 1.5–6 s HTML
truncations to the response total/no-progress deadlines. A fully submitted
response is marked served before `WiFiClient::stop()`, but ESP8266 Core 3.1.2
already gives `stop()` an ACK-oriented flush grace before `tcp_close()`; the
close result is not recorded. The primary correction recommended longer
bounded response deadlines, not another TCP state machine. T−7 remains the
unconditional upper-priority interruption.

### WEB-001D.9 — HTTP response timeout adjustment [P1 · COMPLETED]
Extends only response policy limits: JSON to 5,000 ms, flash HTML to 15,000 ms
and no-progress to 3,000 ms. Request total/inactivity remain 600/200 ms and the
ESP8266 individual write timeout remains 50 ms. All arithmetic and T−7 gate
precedence are unchanged; no transport, dashboard, schema, RF or diagnostic
state-machine behavior is added. Evidence: 44/44 focused transport cases within
296/296 passing native tests and successful ESP8266/ESP32 builds. Relative to
WEB-001D.7, ESP8266 remains at 38,080 B RAM and uses 386,275 B flash (+0/+16 B);
ESP32 remains at 51,348 B RAM and uses 887,117 B flash (+0/−16 B). Final
NodeMCU verification completed 5/5 full 14,389-byte HTML transfers with the
expected SHA-256 and 10/10 complete schema-v1 JSON responses. Browser layout,
last-frame telemetry and GPS presentation passed; HTTP suspended across one
natural frame and recovered automatically after its successful completion.

### WEB-002A — Web configuration architecture analysis [P1 · COMPLETED]
Verified the exact ACTIVE/CANDIDATE/SAVED and CLI semantics, identified the
stale parser-owned WSPR candidate risk, and recommended one shared application
service before any writable HTTP route. The proposed web scope is WSPR-only;
Wi-Fi credentials and AP provisioning remain reserved for WEB-003. No files,
builds, uploads or persistent records were changed by the analysis.

### WEB-002B — Shared configuration service [P1 · COMPLETED]
Introduces one revisioned application-level service for atomic WSPR candidate
staging, the existing immediate RF-safe CLI frequency-mode selection, discard,
defaults and explicit verified save/application. The CLI refreshes its WSPR
working copy from the authoritative candidate before each command, preventing a
future web edit from being overwritten by stale parser state; incomplete Wi-Fi
input remains local to preserve the existing two-step secret-entry workflow.
The RAM-only revision rejects stale mutations and wraps naturally. Persistence
and runtime-application failures remain distinct, including rollback to the
previous ACTIVE WSPR settings after a verified save cannot be applied. No HTTP
configuration route, persistent-format change or scheduler/RF policy change is
included. Evidence: 13 focused service cases within 309/309 passing native
tests and successful ESP8266/ESP32 builds. Relative to WEB-001D, ESP8266 uses
38,312 B RAM and 387,939 B flash (+232/+1,664 B); ESP32 uses 51,372 B RAM and
888,637 B flash (+24/+1,520 B). NodeMCU verification confirmed that a
candidate-only power edit changed neither ACTIVE nor SAVED and that
`config discard` restored CANDIDATE from SAVED. The existing WSPR v2 record,
saved Wi-Fi connection, one natural WSPR frame and post-TX HTTP recovery all
remained operational. No `config save` or other persistent hardware write was
performed.

### WEB-002C — Read-only WSPR configuration API [P1 · COMPLETED]
Adds experimental `GET /api/config/v1` using the existing bounded HTTP server
and the coherent `ConfigurationService` snapshot. Schema 1 reports ACTIVE,
CANDIDATE and SAVED WSPR settings, RAM-only revision, dirty/ACTIVE-versus-SAVED
state, persistence origin/version, migration and runtime synchronization. Its
capabilities list is derived from `BandPlan` and `ConfigValidator`; only the
TX-enabled 20 m band is advertised. The endpoint shares the existing 2,049-byte
JSON buffer, exposes no Wi-Fi fields or credentials, performs no mutation and
remains unavailable under the existing RF-critical HTTP gate. Writable routes
remain pending for WEB-002D. Evidence: 318/318 native tests and successful
ESP8266/ESP32 builds. Relative to WEB-002B, ESP8266 uses 38,376 B RAM and
391,379 B flash (+64/+3,440 B); ESP32 uses 51,388 B RAM and 891,957 B flash
(+16/+3,320 B). NodeMCU upload and flash verification succeeded. The API and
read-only CLI reported identical WSPR values and configuration state before
and after one naturally scheduled, successfully completed frame; no persistent
write was performed. Four of five initial sequential requests completed, with
one empty best-effort HTTP response followed by immediate recovery; all three
post-TX requests completed with matching 1,190-byte schema-1 JSON. Writable
HTTP configuration operations remain pending for WEB-002D.

### WEB-002D — Secure HTTP candidate configuration [P1 · COMPLETED]
Adds bounded, non-persistent `PUT /api/config/v1/candidate` and
`POST /api/config/v1/discard` operations through `ConfigurationService`.
Requests use a complete fixed-buffer JSON object and expected RAM revision;
stale, malformed, invalid or RF-unsafe requests leave configuration unchanged.
Candidate staging does not apply frequency mode, while web discard restores
only WSPR CANDIDATE and preserves ACTIVE, SAVED and Wi-Fi candidates. Mutation
requests require the device IPv4 Host, exact same-origin Origin and a per-boot
entropy-derived token from `GET /api/config/v1/token`. This is CSRF/DNS-
rebinding mitigation for a trusted LAN, not client authentication. No save,
persistent write, Wi-Fi mutation or dashboard form is included; apply/save
operations remain pending. Evidence: 326/326 native tests and successful
ESP8266/ESP32 builds. Relative to WEB-002C, ESP8266 uses 40,136 B RAM and
396,275 B flash (+1,760/+4,896 B); ESP32 uses 52,316 B RAM and 896,633 B flash
(+928/+4,676 B). NodeMCU upload and flash verification succeeded. Hardware
testing staged a complete power change from 10 to 13 dBm at revision 0, leaving
ACTIVE and SAVED at 10 dBm, then authenticated discard restored CANDIDATE and
advanced revisions 0→1→2 without `config save` or another persistent action.
Missing/invalid CSRF, wrong/missing Origin and wrong Host were rejected with
403 (one initial wrong-Host response was empty under best-effort HTTP, followed
by a verified 403); oversized, malformed, stale and invalid-value requests
returned 413, 400, 409 and 422. Rejected-request snapshots remained identical.
One natural frame completed without abort and HTTP resumed after TX. The
Host/Origin/token controls remain trusted-LAN CSRF/rebinding mitigations, not
authentication of clients already on that LAN. WEB-002E, WEB-002F and WEB-003
remain pending.

### CFG-MODE-001B — Configuration session controller and TX inhibit [P1 · COMPLETED]
Adds one fixed-size, RAM-only firmware controller with independent CLI and web
owners, simultaneous ownership, a 32-character/128-bit web session identifier,
5-second expected heartbeat, 25-second expiry, revision tracking and a scoped
configuration-operation guard. Ownership changes are accepted only while the
transmitter is quiescent with RF output disabled. Active ownership prevents
scheduler arming, frame preparation, RANDOM selection and telemetry changes;
admission is checked again before RF output can be enabled.

The final owner release or expiry activates a resume guard. Admission resumes
only after qualified time, operational RF and a scheduler-derived future slot
more than seven seconds away are observed, so skipped slots are not recovered.
No CLI command, HTTP endpoint, dashboard/OLED state, persistence change or
hardware action is included. Evidence: 12 focused controller cases within
338/338 passing native tests and successful ESP8266/ESP32 builds. Relative to
WEB-002D, ESP8266 uses 40,184 B RAM and 396,675 B flash (+48/+400 B); ESP32 uses
52,364 B RAM and 897,325 B flash (+48/+692 B). The controller safeguards were
subsequently exercised through the combined CFG-MODE-001C/E NodeMCU tests.

### CFG-MODE-001C — CLI configuration session and menu [P1 · COMPLETED]
Integrates CLI ownership with `ConfigurationSessionController`. Bare `config`
enters only after TX admission is inhibited, shows a compact command-oriented
menu and changes the prompt to `config>`. `exit`, `config exit` and `0` release
only CLI ownership, retain candidates without an automatic save/discard and
leave final-owner TX recovery to the existing resume guard. Existing numeric
menu navigation remains available.

`wspr set`, CLI FIXED/RANDOM selection, `config save`, `config discard` and
`config defaults` now require active CLI ownership and otherwise return
`ERR CONFIG_MODE_REQUIRED`; read-only commands and Wi-Fi command semantics are
unchanged. Rejected PREPARING/TX acquisition tests now explicitly verify owner,
revision, RF-disable count and armed preparation state. Evidence: 340/340 native
tests and successful ESP8266/ESP32 builds. Combined CFG-MODE-001B/C usage is
40,212 B RAM / 398,015 B flash on ESP8266 and 52,372 B / 898,641 B on ESP32;
relative to CFG-MODE-001B this is +28/+1,340 B and +8/+1,316 B respectively.
NodeMCU verification passed: entry/menu/idempotence, candidate-only 10→13 dBm
and discard were confirmed without persistence; the 11:28 UTC slot was skipped
with no preparation, RANDOM selection or counter change, and after exit the
11:32 frame completed normally at 14,097,107 Hz with no catch-up or abort. No
HTTP/OLED session control is added.

### CFG-MODE-001D — Operating mode visibility and HTTP gate [P1 · COMPLETED]
Propagates the authoritative `ConfigurationSessionController` snapshot to the
schema-v1 status API, compact console summary, OLED and dashboard without a
second mode flag or exposing session identifiers/tokens. The OLED identifies
CLI, web or simultaneous ownership and the post-session resume guard, while RF
faults and actual `PREPARING`/TX retain priority. The dashboard suppresses its
browser-side TX estimate during configuration and resume guard.

The HTTP gate now evaluates Wi-Fi, actual RF-critical state, active
configuration ownership and then the existing NORMAL T−7 policy. Thus HTTP
remains available across nominal slots only while CONFIGURATION inhibits TX;
resume guard alone does not bypass normal policy. Evidence: 348/348 native
tests and successful ESP8266/ESP32 builds. Relative to CFG-MODE-001B/C,
ESP8266 uses 40,396 B RAM and 399,983 B flash (+184/+1,968 B); ESP32 uses
52,444 B RAM and 900,557 B flash (+72/+1,916 B). NodeMCU verification confirmed
NORMAL/CONFIGURATION JSON state, uninterrupted HTTP across inhibited nominal
slots, resume without catch-up and a later natural completed frame. OLED and
browser rendering were not directly observed in that run. No HTTP session
mutation endpoint is included in this stage.

### CFG-MODE-001E — Secure web configuration sessions [P1 · COMPLETED]
Connects the existing authoritative `ConfigurationSessionController` to
bounded read-only status plus protected HTTP enter, heartbeat and exit routes.
One browser-owned 128-bit lowercase hexadecimal identifier is accepted at a
time; same-owner enter is idempotent, heartbeat renews the existing 25-second
expiry without changing mode revision, and exit releases only web ownership.
CLI and web ownership can coexist. Final-owner release or expiry retains the
existing resume guard and never triggers catch-up TX.

All mutations retain exact IPv4 Host, same-origin Origin and per-boot CSRF
checks. Candidate update and discard additionally require the matching
`X-WSPR-Session`; a CLI session alone cannot authorize an HTTP mutation.
Read-only session state exposes mode, revision, owner presence, expiry
remaining, resume guard and effective TX admission without identifiers,
tokens or credentials. No dashboard form, HTTP save/apply, persistence change
or RF timing change is included. Evidence: 354/354 native tests and successful
ESP8266/ESP32 builds. Relative to CFG-MODE-001D, ESP8266 uses 40,900 B RAM and
402,679 B flash (+504/+2,696 B); ESP32 uses 52,500 B RAM and 903,429 B flash
(+56/+2,872 B).

NodeMCU verification confirmed CLI entry/explicit exit, protected web Enter,
5-second heartbeat policy, 25-second automatic expiry, independent CLI/web
ownership and rejection of wrong-owner, unauthenticated and sessionless
mutation requests. A maintained web session crossed the 12:36 UTC nominal slot
with no `PREPARING`, RANDOM selection or frame-counter change. Expiry and final
release returned to NORMAL without catch-up; the resume guard itself was too
brief for direct HTTP observation, while its detailed timing remains covered
by native tests. A later natural 12:40 UTC frame completed at 14,097,043 Hz,
and the NORMAL T−7 HTTP gate closed and recovered without reset, watchdog or RF
error. Intermittent reset/truncated HTTP reads recovered on retry and remain
within the documented best-effort limitation. No `config save`, persistent
write or manual TX was performed.

### WEB-002 — HTTP WSPR configuration and AP Setup entry [P1 · IN REVIEW]
WSPR-only HTTP save/apply remains implemented. Configuration now offers a
session-owned, revisioned and CSRF-protected request to persist a one-time AP
Setup marker and restart into the existing provisioning portal. The former
operational Wi-Fi scan/editor/save routes have been removed; provisioning
retains its manual scan, OPEN/PROTECTED input and verified Wi-Fi A/B save.
Factory Reset has precedence over AP Setup. Software validation covers both
ESP8266 and ESP32 profiles. A NodeMCU ESP8266EX (4 MB flash, CP2102) running
DEVELOPMENT commit `606e63cd4b5882388c6aa36435072dcc1643e669` passed the
controlled AP Setup path: saved-credential STA, Configuration page and session,
confirmed button request, reboot into AP Provisioning, portal access, and
return to the original STA network after manual RST without changing credentials.
This verifies that scenario on ESP8266 only; new-credential saving, error and
power-loss paths, Factory Reset interaction, electrical CLK0 state and RF
behavior remain unverified. ESP32 hardware validation remains pending. See
`docs/HARDWARE.md` for the procedure and limits. The earlier
operational Wi-Fi editor's test and memory figures described a superseded
implementation, not the current Configuration page.

### WEB-002E.2 — WSPR-only save and runtime reconciliation [P1 · COMPLETED]
Adds an internal owner-checked, revisioned and RF-guarded WSPR-only save path
to `ConfigurationService`. It reuses WSPR v2 A/B persistence, CRC/read-back
verification and migration while leaving Wi-Fi candidates, records and runtime
state untouched. Unchanged synchronized state causes no write, apply or
revision increment; persisted/runtime divergence is reconciled explicitly and
post-persistence apply failures preserve truthful SAVED/ACTIVE state with a
best-effort runtime rollback. The existing CLI combined `config save` behavior
is unchanged. No HTTP endpoint or dashboard control exposes this capability in
this stage.
Software evidence: 361/361 native tests and successful ESP8266/ESP32 builds.
Compared with CFG-MODE-001, ESP8266 remains at 40,900 B RAM and uses 402,711 B
flash (+0/+32 B); ESP32 remains at 52,500 B RAM and uses 903,597 B flash
(+0/+168 B). NodeMCU persistence verification completed with WEB-002E.3.

### WEB-002E.3 — Secure HTTP WSPR save and apply [P1 · COMPLETED]
Adds experimental `POST /api/config/v1/save` with strict bounded JSON
(`expected_revision` plus `confirm: save_wspr`), exact Host/Origin, per-boot
CSRF and matching `X-WSPR-Session` ownership. The route invokes only the
WEB-002E.2 guarded WSPR save/reconciliation service and reports explicit
saved, unchanged, stale, session, RF, validation, persistence, verification
and runtime-apply outcomes. Wi-Fi state and records remain isolated. Request
reception and security validation precede execution; after execution begins,
loss of the HTTP response does not roll back verified persistence. No UI,
automatic retry/save, hardware upload or persistent hardware test is included.
Software evidence: 367/367 native tests and successful ESP8266/ESP32 builds.
Relative to WEB-002E.2, ESP8266 uses 40,996 B RAM and 404,923 B flash
(+96/+2,212 B); ESP32 uses 52,500 B RAM and 905,689 B flash (+0/+2,092 B).
Save-result strings are flash-resident on ESP8266 and reuse the existing HTTP
request and JSON response buffers. Controlled NodeMCU verification saved the
WSPR-only power change from 10 to 13 dBm in 786 ms, confirmed it after reboot,
then restored 10 dBm in 930 ms and confirmed the original WSPR v2 configuration
after a final reboot. Wi-Fi remained isolated and reconnected normally. A
subsequent natural frame completed, with HTTP closing and recovering around the
RF-critical interval and no unexpected reset or watchdog event. HTTP remains a
best-effort service: transient reset/empty responses were resolved by rereading
authoritative state rather than blindly repeating persistent operations.
Physical RF power and symbol timing were not instrumentally verified.

### WEB-002F.2 — Configuration page foundation [P1 · COMPLETED]
Adds a self-contained, flash-resident `GET /config` page with reciprocal
dashboard navigation and the existing responsive visual language. It polls
only `GET /api/config/v1` and `GET /api/config/v1/session`, serially and with
bounded timeouts, to present operating mode, owners, ACTIVE/CANDIDATE/SAVED
state, WSPR fields, revisions and synchronization status. Controls are visibly
disabled placeholders: the page does not fetch CSRF material, generate a web
owner, acquire a configuration session or issue any mutation. Interactive
configuration remains pending a separately reviewed WEB-002F stage. Software
evidence: 374/374 native tests and successful ESP8266/ESP32 builds; ESP8266
used 40,996 B RAM and 415,499 B flash, while ESP32 used 52,500 B RAM and
916,265 B flash. Controlled NodeMCU verification delivered the 10,416-byte
asset intact in 5/5 requests, confirmed both read-only APIs and unchanged
configuration/session state, and observed normal T−7 closure, one completed
natural frame and automatic HTTP recovery. Firefox checks covered 320 px,
360 px and desktop layouts; physical smartphone verification was unavailable.

### WEB-002F.3 — Interactive web configuration session [P1 · COMPLETED]
Adds explicit Enter and Exit controls to `/config` using the existing secure
web-session API. A browser-generated 128-bit owner remains RAM-only, heartbeat
requests are serialized at about five-second intervals, and uncertain Enter,
heartbeat or Exit results are reconciled against authoritative session state.
Opening the page remains read-only; candidate editing, discard and save/apply
controls stay disabled. Navigation attempts an explicit release and warns when
release cannot be confirmed, while unload cleanup remains best-effort. Software
evidence: 379/379 native tests and successful ESP8266/ESP32 builds. The
configuration asset is 17,041 bytes. ESP8266 uses 40,996 B RAM and 422,123 B
flash; ESP32 uses 52,500 B RAM and 922,889 B flash. This is no RAM change and
+6,624 B flash on both targets versus WEB-002F.2. Controlled NodeMCU/browser
verification confirmed explicit Enter, five-second renewal, Exit, navigation
release, CLI/web coexistence, skipped-slot TX inhibition and a later completed
natural frame. One transient HTTP heartbeat failure stopped renewal in the
original frontend and allowed the web owner to expire safely while CLI
ownership remained active; WEB-002F.3.1 addresses that reliability gap.

### WEB-002F.3.1 — Web heartbeat recovery [P1 · COMPLETED]
Preserves the current browser-RAM session identifier after a transient
heartbeat failure, marks ownership uncertain and keeps session-dependent
mutation controls disabled. Recovery reuses the authenticated heartbeat route
and serialized request queue for at most two retries, after 1.5 and 3 seconds;
a successful renewal resumes the normal five-second heartbeat. Confirmed
expiry or wrong ownership clears local ownership and requires explicit Enter.
Exit, Dashboard navigation and `pagehide` cancel pending recovery by advancing
a local generation guard. Firmware expiry, RF admission, scheduler behavior and
session APIs are unchanged. Controlled NodeMCU verification used Firefox 158:
one and two injected heartbeat failures recovered with the original owner and
zero automatic Enter calls; confirmed expiry required explicit re-entry, Exit
cancelled pending recovery, navigation released ownership, a nominal slot was
skipped under TX inhibition and a later natural frame completed 4/4 with no
abort. The 17,448-byte asset was delivered intact in 5/5 requests. Physical
visible-browser rendering was not repeated because Firefox ran headless.

### WEB-002F.4 — Candidate editing and discard UI [P1 · COMPLETED]
Adds nine capability-derived WSPR fields to the existing configuration page.
Editing requires confirmed WEB ownership and authoritative CONFIGURATION/TX
inhibition. Browser `localDirty`, server `candidate_dirty`, ACTIVE/SAVED
difference and runtime synchronization remain distinct. `Update candidate`
validates basic advertised constraints and submits one complete, revisioned
PUT without automatic retry; an uncertain result is reconciled by GET while
preserving local values. `Reset form` is browser-only. Authenticated Discard
requires explicit confirmation and restores CANDIDATE from SAVED without
changing ACTIVE or persistence. `Save and apply` remains disabled and the page
does not invoke its endpoint. Controlled NodeMCU verification delivered the
26,391-byte page intact in 5/5 requests with matching SHA-256. Firefox 158
confirmed read-only startup, explicit WEB ownership, browser-only Reset,
CANDIDATE power staging 10→13 dBm at revision 0→1 and authenticated Discard
back to 10 dBm at revision 1→2. ACTIVE/SAVED and WSPR v2 persistence stayed at
10 dBm. After Exit, a later natural frame completed without abort and HTTP
recovered; no persistent write was performed. The browser was headless, so
physical visible-browser rendering was not repeated.

### WEB-002F.5 — Save and apply configuration UI [P1 · COMPLETED]
Enables the existing Save and apply control only for a confirmed owning WEB
session, inhibited TX, current authoritative revision, a form exactly matching
CANDIDATE and actual persistence/runtime reconciliation work. Local unstaged
edits must first use Update candidate. The page requires explicit confirmation
and submits only `expected_revision` plus `confirm: save_wspr` through the
existing authenticated, serialized POST endpoint. Success and unchanged
results are verified by rereading ACTIVE/CANDIDATE/SAVED while retaining the
session. Lost or failed responses are never retried automatically; read-only
reconciliation distinguishes confirmed completion, persistence with runtime
apply failure and unresolved outcomes. Wi-Fi state is never submitted or
modified. Firefox hardware verification completed the WSPR-only 10→13 dBm
save/apply in 404 ms, retained the active WEB session, confirmed persistence
after reboot, then restored 13→10 dBm in 314 ms and confirmed the complete
original configuration after a final reboot. Wi-Fi configuration remained
unchanged and no unexpected reset or watchdog event occurred. Initial delivery
of the 30,659-byte page completed in 4/5 attempts, followed by a successful
complete recovery request, consistent with the documented best-effort HTTP
policy.

The Si5351A `NOT_DETECTED` condition observed after the final reboot cleared
after a power cycle without a firmware change, and an RF signal was subsequently
received with an IC-705; the transient detection failure remains unexplained.
WSPR message decoding is deferred, and this activity did not instrument RF
frequency, level, spectrum or symbol timing. The originally requested complete
post-recovery natural-TX telemetry sequence was not captured, so neither RF
decoding nor full lifecycle verification is claimed.

### WEB-002F.6 — Final UI and regression review [P1 · COMPLETED]
Final read-only review found no functional defect requiring correction. UI,
navigation, session ownership and recovery, configuration-state separation,
mutation safeguards, trusted-LAN request protections, flash delivery and RF/
persistence isolation were reviewed against source, tests and prior Firefox
hardware evidence. All 388 native tests and both builds passed: ESP8266 used
40,996 B RAM and 435,739 B flash; ESP32 used 52,500 B RAM and 936,505 B flash.
The 30,659-byte configuration asset matched SHA-256
`7958ab02f1c4a56481daa4dd0182a8db9e1152114ce7b481eb55f048a5b71fa9`.
The review retained the best-effort HTTP limitation, the unexplained transient
Si5351A detection failure, deferred WSPR decoding and instrumental RF
validation, and the incomplete final post-recovery natural-TX telemetry
capture. No additional functional, hardware or persistent action was taken.

### WEB-002F.7 — Header and navigation consistency [P1 · COMPLETED]
Aligns the Dashboard and Configuration headers on the exact project identity
`WSPR Beacon 2.0 — IW2NNV / AC2WZ` and the same responsive two-button
Dashboard/Configuration navigation, with the current page identified by
`aria-current`. The former operational subtitle is removed from both headers;
the Dashboard retains callsign and locator in the Frequency card and band in
the existing mode/band field. Configuration-to-Dashboard navigation continues
to use the existing explicit session-release and uncertain-release workflow,
while opening Configuration from the Dashboard remains read-only. No API,
session, configuration, HTTP-gate or RF behavior changes. Software evidence:
388/388 native tests and successful ESP8266/ESP32 builds. RAM is unchanged at
40,996 B on ESP8266 and 52,500 B on ESP32; flash is 436,123 B and 936,889 B
respectively (+384 B on each target versus WEB-002F.6). Dashboard is 15,549 B
(+455 B) and Configuration is 30,590 B (-69 B). Hardware/browser verification
confirmed upload, startup, Wi-Fi/NTP, one live 320 px Firefox Dashboard render
and two completed natural frames with normal HTTP gate recovery. Delivery did
not meet acceptance: two separate runs completed only 3/5 Dashboard and 2/5
Configuration responses, with variable truncation, TCP resets and response
timeouts despite matching hashes for every complete body. The Configuration
render and active-session browser navigation therefore remain unverified.
The later F.7C/F hardware run completed both Firefox rendering and session
verification; the earlier failures above are retained as transport-history
evidence rather than the final result.

### WEB-002F.7B — Partial TCP capacity utilization [P1 · COMPLETED]
Corrects only the ESP8266 write planner so one bounded service step submits
`min(remaining, 536, availableForWrite())` bytes instead of rejecting all
positive TCP capacity below 536 bytes. Zero capacity remains non-blocking,
the existing 536-byte flash staging buffer and partial-write offset accounting
are reused, and ESP32 retains its capacity-independent 536-byte planning.
Request/response deadlines, 50 ms ESP8266 write timeout, close behavior, HTTP
gate, frontend and RF paths are unchanged. A new variable-backpressure test
covers zero, sub-chunk, exact-chunk and larger capacity while verifying the
complete flash response byte-for-byte. Software evidence: 56/56 focused HTTP
tests within 389/389 passing native tests and successful ESP8266/ESP32 builds.
Memory is unchanged from WEB-002F.7: ESP8266 uses 40,996 B RAM and 436,123 B
flash; ESP32 uses 52,500 B RAM and 936,889 B flash. Controlled NodeMCU
verification showed partial improvement but did not meet acceptance: Dashboard
delivery completed 3/5 and Configuration 1/5, with correct hashes for complete
bodies, variable truncation, additional response timeouts and one heartbeat
failure during prolonged delivery. Two natural frames completed without abort,
HTTP recovered after TX and no reset or watchdog occurred. Live Configuration
rendering and 60-second browser-session stability therefore remained blocked.

### WEB-002F.7C — Static gzip HTTP assets [P1 · COMPLETED]
Adds deterministic, precompressed gzip representations of the Dashboard and
Configuration assets without runtime compression or full-page RAM allocation.
The bounded request parser recognizes an explicit `gzip` token in
`Accept-Encoding`; `gzip;q=0`, unsupported encodings and absent negotiation use
the existing identity representation. Gzip responses add `Content-Encoding:
gzip`, preserve HTML content type, exact compressed Content-Length,
`Vary: Accept-Encoding`, `Cache-Control: no-store` and `Connection: close`, and
reuse the existing flash staging, write planner and response deadlines. JSON
remains uncompressed. The
stdlib-only generation script fixes gzip time to zero, emits stable include
files and verifies that decompression exactly reproduces each source asset;
developers regenerate or verify them with
`python3 tools/generate_web_assets.py [--check]`. Dashboard compresses from
15,549 to 5,353 bytes (65.6% smaller) and Configuration from 30,590 to 8,667
bytes (71.7% smaller). Evidence: 392/392 native tests and successful
ESP8266/ESP32 builds. Versus WEB-002F.7B, ESP8266 uses 41,108 B RAM and
450,803 B flash (+112/+14,680 B); ESP32 uses 52,532 B RAM and 951,613 B flash
(+32/+14,724 B). Final controlled hardware verification is recorded with F.7F.

### WEB-002F.7D/E — HTTP timeout root cause and isolated TCP capture [P1 · COMPLETED, analysis]
Read-only source review and filtered NodeMCU packet capture identified two
separate effects. Firefox configuration polling explained previously excess
accepted clients, but isolated single-client transfers still reproduced
truncation. The trace showed bidirectional TCP loss, sequence gaps and
retransmissions without a zero-window condition. In the counter-correlated
sample, four requests produced three served responses and exactly one response
timeout/forced closure; the failed transfer remained below the 15-second HTML
deadline and aligned with the independent three-second no-progress abort. No
firmware, network configuration, persistence or RF behavior was changed.

### WEB-002F.7F — Remove premature HTTP response no-progress abort [P1 · COMPLETED]
Removes only the independent three-second response no-progress timeout so lwIP
can recover from temporary retransmission stalls. Absolute response deadlines
remain 5,000 ms for JSON and 15,000 ms for static HTML; request total/inactivity
remain 600/200 ms, and the ESP8266 individual write timeout remains 50 ms. The
T−7 gate, PREPARING/TX exclusion, one bounded write per loop, TCP close policy,
HTTP counters, gzip negotiation and frontend behavior are unchanged. Software
evidence: 60/60 focused transport cases within 393/393 passing native tests
and successful ESP8266/ESP32 builds. Versus WEB-002F.7C, removing the obsolete
progress timestamp/state reduces ESP8266 to 41,100 B RAM and 450,771 B flash
(-8/-32 B) and ESP32 to 52,524 B RAM and 951,577 B flash (-8/-36 B).

Final NodeMCU verification delivered 5/5 Dashboard gzip responses (5,353 B)
and 5/5 Configuration gzip responses (8,667 B). Every compressed hash matched
its embedded asset and every decompressed hash matched the corresponding
15,549 B or 30,590 B identity source. Complete transfers included temporary
stalls up to 10.270 s without a response-timeout increment. A real Firefox
engine confirmed gzip decompression, the unified header and navigation,
responsive 320/360 px layouts, light/dark presentation, and a stable 60-second
web session with explicit Enter, heartbeat and Exit. One naturally scheduled
WSPR frame completed without abort and HTTP recovered after TX.

The single identity-fallback check remained incomplete at 9,466/15,549 B and
produced one absolute response timeout and forced closure. HTTP therefore
remains experimental and best-effort; gzip is the preferred static-page
delivery path, and identity delivery is not guaranteed under adverse TCP
conditions. No physical RF frequency, level, spectrum or symbol timing was
instrumentally verified, and no WSPR decode was performed in this activity.

### T-010 / WEB-003 — First-start and long-press Wi-Fi portal [P1 · TODO]
Temporary AP without affecting GPS or TX; no automatic AP when a saved network
is merely unavailable. Long press is reserved; button behavior requires design.

### WEB-003B.1 — Automatic AP Wi-Fi provisioning [P1 · COMPLETED]
Adds the reviewed first-phase boot controller, one 15-second initial STA
attempt, latched WPA2 AP fallback, per-boot random numeric password, isolated
provisioning HTTP page, verified Wi-Fi-only save, provisioning-safe console and
independent TX admission/execution barriers. Manual SSID entry remains
available; Wi-Fi scanning is explicitly deferred to WEB-003B.2. Software and
build evidence: 404/404 native tests passed; ESP8266 used 41,508 B RAM and
460,003 B flash, while ESP32 used 52,596 B RAM and 961,665 B flash. Subsequent
controlled NodeMCU verification confirmed saved-credential STA boot, the
15-second fallback AP, smartphone access to the provisioning page, AP
persistence after the original network returned, and STA recovery after a
manual restart. No new credentials were saved and no RF measurement was made.

### WEB-003B.2 — Manual provisioning Wi-Fi scan [P1 · COMPLETED]
Adds an explicit provisioning-only asynchronous scan through the existing
`WifiManager`, a fixed ten-entry RSSI-sorted snapshot, SSID/protection
deduplication, logical timeout with deferred late-driver cleanup, bounded JSON
endpoints and a resilient manual-selection UI. AP+STA never invokes station
association or persistence; manual SSID entry and the WEB-003B.1 RF barriers
remain authoritative. Software/build evidence is recorded in the review
report: 409/409 native tests passed; ESP8266 used 41,856 B RAM and 466,655 B
flash, while ESP32 used 52,652 B RAM and 968,673 B flash. The provisioning
page is 5,408 B identity / 2,306 B deterministic gzip. Controlled NodeMCU and
smartphone verification confirmed manual scan, SSID selection and stable AP
operation without observed resets or watchdog events. ESP32 hardware, actual
new-credential saving, detailed heap/watchdog measurement and instrumental
proof of absent RF output remain pending.

### WEB-004B.1 — AP Operational without TX [P1 · IN REVIEW]
Adds an explicit boot-only transition from AP Provisioning to a separate
AP Operational HTTP profile while retaining the same SSID, password, client
limit and `192.168.4.1`. The bounded listener completes the accepted response,
closes the client, discards backlog, rotates CSRF and then exposes existing
Dashboard, Configuration and operational APIs. Provisioning/save/scan routes,
STA operations and automatic persistence are unavailable after transition.
CLI/OLED distinguish the mode and retain credential recovery. RF operation is
still allowed only in `NORMAL`: AP Operational keeps admission false, never
calls the transmitter update path and cannot select or prepare a frame.
WEB-004B.2 TX enablement is not implemented or authorized in this phase.
Software evidence: 412/412 native tests and both ESP8266/ESP32 builds passed.
ESP8266 uses 42,064 B RAM and 469,727 B flash (+208/+3,072 B versus
`c26c103`); ESP32 uses 52,652 B RAM and 971,757 B flash (+0/+3,084 B).
Provisioning is 6,493 B identity / 2,579 B deterministic gzip; Dashboard is
15,673 B / 5,397 B. No upload, hardware action, persistent write or RF test was
performed.

### WEB-004B.2 — AP Operational protected TX [P1 · IN REVIEW]
Extends the centralized RF-mode criterion to NORMAL and AP Operational while
keeping boot decision, initial STA, provisioning, transition-pending and error
states TX-inhibited. AP Operational accepts only precise GPS/PPS authorization;
a retained NTP reference, coarse/stale/invalid time, RF failure, invalid
configuration or an active Configuration session remains fail-closed. The
completed HTTP transition activates the existing no-catch-up resume guard, and
the normal T−7/PREPARING/TX HTTP policy uses the active AP as network presence.
Dashboard, OLED and serial diagnostics remain available outside RF-critical
windows. Evidence: 414/414 native tests and successful ESP8266/ESP32 builds.
ESP8266 uses 42,080 B RAM and 470,019 B flash (+16/+292 B versus WEB-004B.1;
+224/+3,364 B versus `c26c103`). ESP32 uses 52,652 B RAM and 972,061 B flash
(+0/+304 B versus WEB-004B.1; +0/+3,388 B versus `c26c103`). Deterministic gzip
verification passed. Hardware RF behavior remains unverified and no upload,
persistence action or RF test is part of this phase.

WEB-004C NodeMCU verification subsequently confirmed saved-credential STA,
fallback provisioning, the explicit AP Operational transition, continuous AP,
Dashboard/Configuration navigation, Configuration-session heartbeat and Exit
without saving. AP Operational remained latched when the original network
returned and a manual restart restored STA. With no qualified GPS/PPS, the
Dashboard reported `TX RESUME GUARD`, `NONE / UNKNOWN`, TX authority `NONE`,
`TIME_UNAVAILABLE` and zero completed frames. No reset was observed and no
Wi-Fi/WSPR persistence action occurred. Real AP Operational TX, GPS/PPS-loss
behavior, live T−7 closure, instrumental RF verification, detailed heap/
watchdog margins, ESP32 hardware and AP Operational Save/apply remain pending.

### T-005E — PPS qualification hardening [P0 · IN REVIEW]
Adds a shared deterministic PPS qualifier for ESP8266 and ESP32. A first edge
is only a candidate; three consecutive 900–1100 ms periods are required, while
glitches, wrong or missing periods, multiple ISR edges awaiting consumption and
incoherent NMEA UTC revoke qualification and require a complete new sequence.
The adapter propagates `PRECISE` to `COARSE` without refreshing the reference,
so existing NTP selection and scheduler authorization remain fail-closed.
Evidence: 543/543 native and native_public tests passed; both ESP8266 and both
ESP32 profiles built successfully.
Versus `e363b249`, ESP8266 DEVELOPMENT/PUBLIC use +16 B RAM and +576 B flash;
ESP32 DEVELOPMENT uses +16 B/+616 B and PUBLIC +16 B/+620 B. No upload or
physical PPS timing/signal-integrity measurement was performed.

### T-012 — RF calibration and validation [P1 · TODO]
Measure frequency, stability, power, transients, harmonics, filtering and WSPR
decode with documented instruments and dummy-load/on-air distinction.

## Deferred work and next action

Automatic multiband scheduling, band-specific RF filtering, OTA and advanced
history/export are deferred. The next task requires explicit authorization; do
not start firmware work from this roadmap alone.
