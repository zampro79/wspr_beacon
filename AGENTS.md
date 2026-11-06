# AGENTS.md — WSPR Beacon 2.0

## Authority and required reading

Before any work, read `PROJECT.md`, `TASKS.md`, `README.md`,
`docs/USER_GUIDE.md` and `docs/HARDWARE.md`. `PROJECT.md` defines the current
architecture; `TASKS.md` records scope and evidence. If code and documentation
disagree, report the conflict and request a decision rather than silently
choosing one interpretation.

The active PlatformIO implementation is under `src/` and `include/`. Obsolete
root-level Arduino `.ino` sketches were removed from the current tree and remain
available only through Git history; they were not a second implementation.

## Development workflow

1. Check branch, `git status`, HEAD and local changes before every task. Preserve
   user changes and never reset or overwrite them without explicit approval.
2. Work on one narrowly scoped task at a time. State objective, affected files,
   risks and planned verification before implementation.
3. Request approval before architectural changes, pinout/hardware changes,
   fundamental library replacement, protocols, frequencies, RF behavior,
   scheduling, time policy, storage, network services, Git workflow changes or
   work outside the mandate.
4. Avoid unrelated rewrites, premature abstractions and new dependencies.
5. Run applicable native tests and ESP8266/ESP32 builds. Compare RAM/flash for
   firmware changes and run `git diff --check`. Hardware verification is
   required when relevant and available, but must be reported separately from
   compilation and software tests.
6. Present the task as **IN REVIEW** with changed files, results, memory impact,
   hardware evidence, limitations and remaining risks. Wait for explicit user
   approval before commit or push.
7. After approval, commit only the reviewed scope and perform an ordinary push
   to `origin/main`. Never force-push. Verify remote alignment and a clean tree.
   Never commit, push, reset, rebase or perform destructive operations without
   explicit authorization.
8. Do not upload firmware, write persistent device configuration or perform RF
   tests unless the current mandate explicitly authorizes them. A general
   approval to commit does not imply hardware authorization.
9. Do not perform additional hardware monitoring after consolidation unless
   requested.

## Documentation policy

Maintained repository documentation is written in English. Preserve literal
firmware commands, status tokens, identifiers and task IDs. User-visible changes
to commands, configuration, OLED/LED behavior or diagnostics must update
`docs/USER_GUIDE.md` in the same task. `docs/USER_GUIDE.md` is the authoritative
operational manual; `docs/USER_MANUAL.md` is only a compatibility redirect.

Update `TASKS.md` only to reflect evidence actually obtained. Historical task
results must remain traceable and must not be rewritten as current guarantees.

## Platform priorities

- ESP8266 NodeMCU is the primary platform.
- ESP32 is the active secondary build-supported platform.
- Nano Every support is retained but frozen for new features because of flash
  limits.
- Nano 33 IoT is a future integration.
- The common WSPR core must remain independent of ESP-specific services.

## Functional and RF invariants

- GPS/PPS is the preferred time authority; synchronized NTP authorization is
  explicitly experimental. Time and position remain independent.
- Never start a new TX without qualified time, valid configuration and
  operational RF. Prefer disabled RF on uncertainty or error.
- Seven defined WSPR bands are software-selectable. Validate the full four-tone
  set before RF operation; software support does not authorize transmission or
  replace band-appropriate filtering and physical RF verification.
- Freeze frame inputs during `PREPARING`; changes apply between frames. Critical
  RF errors may abort immediately.
- Keep CLK0 disabled until the scheduled slot. Do not add intentional early
  emission or alter WSPR symbol count, cadence or absolute deadlines.
- Do not let console, OLED, web, Wi-Fi or diagnostics block `PREPARING`/TX or
  introduce I²C work in the RF-critical window.
- Never equate declared WSPR dBm with measured RF power or software diagnostics
  with calibrated RF validation.
- Never expose credentials in source, logs, output, commits or reports.

## Verification standards

Prefer hardware-independent tests for NMEA parsing, locator generation, time
arbitration, configuration, persistence, scheduling and WSPR encoding. State
exactly which targets were built and which boards/peripherals were physically
tested. Before any RF test, confirm filter, load/antenna, frequency, declared
power and measurement setup with the user.

Stop and request direction if required authority, hardware safety information
or a decision affecting architecture or observable behavior is missing.
