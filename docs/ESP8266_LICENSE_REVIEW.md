# ESP8266 runtime license review for release-candidate preparation

Status: `NOT_APPROVED_FOR_PUBLIC_DISTRIBUTION`.

This is a technical inventory, not legal advice. Project GPL adoption does not
by itself resolve the obligations of linked third-party components.

## LGPL-covered runtime components

The pinned ESP8266 build statically links or may select code from:

- EspSoftwareSerial 8.2.0 — LGPL-2.1-or-later;
- ghostl 1.0.1 — LGPL-2.1-or-later, transitively used by EspSoftwareSerial;
- Arduino ESP8266 Core 3.1.2 — predominantly LGPL-2.1-or-later with
  independently licensed bundled components.

The inspected PUBLIC ELF contains `EspSoftwareSerial::UARTBase` and ghostl
template symbols, so these are not merely selectable dependencies. No project
patch mechanism or patch files for either package were found. The framework's LGPL-covered
core objects are also part of the static link.

LGPL-2.1 section 6a permits supplying the complete machine-readable work that
uses the Library as object code **and/or source code**, so a complete matched
source set and working relink procedure can be a technically viable route; an
object-file bundle is not automatically the only route. The current archive
contains the project source and pinned declarations but not durable copies of
the exact library/framework sources. Reacquisition through PlatformIO was
demonstrated, but an upstream URL is not a retained corresponding-source copy.

The proposed corresponding-source archive contains project source and exact
dependency declarations, while the manifest records resolved packages. It does
not currently contain durable copies of every dependency source, project object
files, or a separately validated equivalent relinking mechanism. The required
form and retention of the exact dependency source, and legal confirmation that
the proposed source-based relink route satisfies the distribution model, remain
`REQUIRES_LEGAL_REVIEW`. Object files were deliberately not added without that
decision.

## ESP8266 NONOS SDK

Arduino ESP8266 Core 3.1.2 selects `NONOSDK22x_190703` and links against binary
SDK archives. The locally preserved Espressif license restricts SDK use to
ESP8266 hardware. Complete corresponding source for those archives is not in
the installed framework package.

The exact link offers 15 SDK/framework archives. Comparing globally defined
archive symbols with the final ELF confirms contributions attributable to
`libhal`, `libphy`, `libpp`, `libnet80211`, `liblwip2-536-feat`, `libwpa`,
`libcrypto` and `libmain`. It found no contributed symbols from `libwps`,
`libbearssl`, `libespnow`, `libsmartconfig`, `libairkiss` or `libwpa2`; those
remain link inputs, not demonstrated firmware content.

The local and upstream license permits copying and distribution only for use on
ESP8266 hardware and requires preservation of its notice. Source for all linked
archives is not available. The GPLv3 System Library definition depends on facts
and legal characterization not established by the build alone; the SDK is
therefore not assumed to qualify. Compatibility of the hardware restriction
with conveyance of the GPLv3-covered combined firmware remains
`UNRESOLVED_LEGAL_COMPATIBILITY`.

## GCC runtime

The pinned Xtensa GCC toolchain is 2.100300.220621 (GCC 10.3.0). The actual link
command selects both `-lstdc++` and `-lgcc`, and the ELF contains GCC runtime
symbols including `_Unwind_Find_FDE`. GCC's authoritative Runtime Library
Exception 3.1 covers files bearing its notice, including covered libgcc and
libstdc++ runtime files, when Target Code is produced by an Eligible Compilation
Process. This build uses GCC without non-GPL intermediate-representation
plugins, so the technical applicability condition is met. The verbatim
exception is now preserved in the package. This conclusion does not extend the
exception to unrelated toolchain or SDK components.

## Package consequence

The GCC evidence is complete, but LGPL source/relink retention and NONOS SDK
compatibility remain unresolved. The package may be technically reproducible
and internally verifiable while remaining legally unauthorized. The manifest therefore records technical
eligibility separately from `public_distribution_authorized: false` and this
legal-review status. No package produced during this review may be published.
