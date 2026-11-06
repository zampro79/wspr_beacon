# ESP8266 NONOS SDK focused review

Status: `UNRESOLVED_LEGAL_COMPATIBILITY`.

Arduino ESP8266 Core 3.1.2 uses `NONOSDK22x_190703`. The applicable local SDK
text is the **ESPRESSIF MIT License**, whose operative grant begins:

> Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only

It otherwise grants use, copying, modification, merging, publication,
distribution, sublicensing and sale, and requires preservation of the copyright
and permission notice.

## Narrow conclusion

- Distribution for use on ESP8266 is expressly contemplated.
- The hardware limitation means this is not the standard MIT license.
- GPL-3.0-or-later generally disallows imposing additional restrictions on the
  rights it grants for the covered work.
- A possible System Library characterization depends on legal facts not proven
  by the build and does not automatically follow from the SDK being supplied by
  a chip vendor.
- No alternative license or authentic additional Espressif exception covering
  this exact combined GPLv3 distribution was found in the examined package.
- Arduino ESP8266 and Tasmota publicly distribute ESP8266 firmware, but their
  practice is not an additional permission granted to this project.

The current evidence therefore supports **compatibility plausible but not
demonstrated**, not a proven incompatibility and not release authorization.

## Proposed question to Espressif

> WSPR Beacon 2.0 is GPL-3.0-or-later firmware built with Arduino ESP8266 Core
> 3.1.2 and NONOSDK22x_190703. The resulting binary incorporates SDK archives
> distributed under the ESPRESSIF MIT License containing the condition
> “Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only.” May
> the combined firmware binary be redistributed under GPL-3.0-or-later for use
> exclusively on ESP8266 hardware while the SDK portions remain under the
> ESPRESSIF MIT License and its notice? If so, does Espressif grant an explicit
> linking/distribution exception or other permission resolving GPLv3's
> additional-restrictions concern, and what exact notice/source materials must
> accompany the binary?

Do not send this question without separate owner authorization. Until an
authentic answer or a documented legal conclusion exists, ESP8266 binary
publication remains `GO WITH CONDITIONS` and unauthorized.
