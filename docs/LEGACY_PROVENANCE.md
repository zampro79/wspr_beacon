# Historical Arduino sketch provenance

The five root-level Arduino sketches predated the active PlatformIO firmware.
They were excluded from the consolidated source tree by LEGACY-001 after this
provenance record was prepared. They were not compiled by the ESP8266 or ESP32
environments. Their former object identifiers are retained below as provenance
notes, but the predecessor Git objects are not present or recoverable from the
current repository history.

| File | Existing attribution or notice | Known relationship to current firmware | Unresolved issue |
| --- | --- | --- | --- |
| `wspr_5.ino` | Contains its own MIT-like permission text and names NT7S, K6HX, AK6L and LA3PNA in comments | Historical example of beacon structure and WSPR-related integration; no claim is made that it is the source of the modern implementation | The precise origin and authority for all combined portions are not established |
| `FrequencySelection.ino` | No autonomous license notice found | Historical frequency-selection experiment | Author and license provenance are unconfirmed |
| `Wifi_utility.ino` | No autonomous license notice found | Historical Wi-Fi utility experiment | Author and license provenance are unconfirmed |
| `display.ino` | No autonomous license notice found | Historical display experiment | Author and license provenance are unconfirmed |
| `gps.ino` | Contains external reference URLs but no autonomous license notice | Historical GPS experiment | URLs are contextual evidence, not a license grant |

In the predecessor repository all five files were recorded together in commit
`36edfc7fd0cb0bc847182190aa603d86f8282865`. That commit and the historical
blobs listed below are not present in the current consolidated repository:

| File | Blob in the pre-removal tree |
| --- | --- |
| `wspr_5.ino` | `d978e3780432e4e53915228903cfccf7c4e4519d` |
| `FrequencySelection.ino` | `96987029e33aafa76bbb6944d99a6eaa95e6ea61` |
| `Wifi_utility.ino` | `430fc56ee7463cd7599eb8796d07ceadf0708d8b` |
| `display.ino` | `7b6879ab1b3b4158f25d97d459ef708aa413851c` |
| `gps.ino` | `1b1cf416ba609794a99bde15adb4f5362c085f32` |

In a checkout that still contains the predecessor history, a file could be
inspected without restoring it using `git show 36edfc7:<filename>`. This command
does not work in the current repository. The archived identifiers document the
former import, not ownership or redistribution rights.

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
