# Historical Arduino sketch provenance

The five root-level Arduino sketches predated the active PlatformIO firmware.
They were excluded from the consolidated source tree by LEGACY-001 after this
provenance record was prepared. They were not compiled by the ESP8266 or ESP32
environments. Private predecessor commit and blob identifiers are intentionally
omitted from this public-facing record and are not present or recoverable from
the current repository history.

| File | Existing attribution or notice | Known relationship to current firmware | Unresolved issue |
| --- | --- | --- | --- |
| `wspr_5.ino` | Contains its own MIT-like permission text and names NT7S, K6HX, AK6L and LA3PNA in comments | Historical example of beacon structure and WSPR-related integration; no claim is made that it is the source of the modern implementation | The precise origin and authority for all combined portions are not established |
| `FrequencySelection.ino` | No autonomous license notice found | Historical frequency-selection experiment | Author and license provenance are unconfirmed |
| `Wifi_utility.ino` | No autonomous license notice found | Historical Wi-Fi utility experiment | Author and license provenance are unconfirmed |
| `display.ino` | No autonomous license notice found | Historical display experiment | Author and license provenance are unconfirmed |
| `gps.ino` | Contains external reference URLs but no autonomous license notice | Historical GPS experiment | URLs are contextual evidence, not a license grant |

All five files were recorded together in the predecessor history before their
exclusion. Their exact private object identifiers are not required to interpret
this public inventory and would not establish ownership or redistribution
rights. Maintainers should retain the corresponding private provenance evidence
separately; it is not a dependency or a resolvable reference of this repository.

An apparently related public sketch family exists at
<https://sites.google.com/site/ik1lbw/home/sketch-wspr-per-3-tx>. No exact
source/version correspondence has been proven. Consequently this project does
not assert that IW5EJM, WB2CBA, or any other person associated with that page is
the source of these exact files.

The modern firmware uses a different PlatformIO architecture and has extensive
new components and tests. Similar APIs, radio algorithms, or device roles do
not by themselves prove derivation or transfer rights. Removal from the current
tree does not transfer copyright, erase the original attributions retained in
Git, or prove that every possible third-party provenance question is resolved.

The GPL-3.0-or-later license adopted for original modern project material is
not applied retroactively to these sketches. Their historical embedded notices
remain attributable to their original sources in the predecessor history. The current
release-source plan excludes the removed files; any separate redistribution
recovered from history remains blocked until its provenance and third-party
permissions are resolved. No authorship is attributed to IW2NNV/AC2WZ without
evidence.
