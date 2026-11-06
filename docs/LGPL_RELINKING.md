# LGPL relinking materials

Status: Phase 2 autonomous local kit technically verified; legal/package review
still required before distribution.

ESP8266 and ESP32 firmware are statically linked. Recipients must have a
practical way to replace LGPL-covered code and produce a new firmware image.
Upstream URLs alone are not retained corresponding source.

## Demonstrated procedure

For ESP8266, an isolated `esp8266_public` build was followed by a controlled
change to EspSoftwareSerial 8.2.0. The next PlatformIO pass recompiled only the
library object, rebuilt its archive, relinked the existing application objects
and emitted a changed, structurally valid BIN.

For ESP32 Phase 1 the representative LGPL component is Arduino ESP32 Core. On
2026-11-06 the following procedure was completed in disposable `/tmp`
directories, without changing the repository or globally installed framework:

1. build `esp32_public` in an isolated exact-source checkout;
2. record hashes and timestamps for `src/*.o`;
3. make a harmless, recognizable modification to an actually linked Arduino
   Core function in the temporary framework copy;
4. rebuild the affected core object/archive and link without rebuilding project
   objects;
5. verify unchanged project-object hashes;
6. validate the resulting ELF and application image with esptool `image_info`;
7. generate the four-component and factory layouts; and
8. retain the complete command log and hashes.

The isolated baseline used PlatformIO espressif32 7.1.3, Arduino ESP32 Core
`4.20017.260907+sha.dcc1105b`, toolchain
`8.4.0+2021r2-patch5` and esptool 4.11.0. A controlled change to the linked
`esp32-hal-misc.c` `yield()` implementation caused PlatformIO to compile only
`esp32-hal-misc.c.o`, rebuild `libFrameworkArduino.a`, relink and regenerate
the application BIN. The aggregate SHA-256 of every project `src/*.o` remained
`4eb7d1ce32a40229619cc033316f5c5e1921b315a0e5618409133c5e12b4f904`.
The application BIN changed from
`83bcd9fa587c6edc2d2de0816b6b8f5ed7af1c6f1ea30341dbd85f0a904026ea`
to `e7b4020855b5ca409e16d02b68d1d980c01f25a22e7ab72a30dc424eddeff233`.
Esptool `image_info` accepted the new 1,017,776-byte ESP32 image with valid
checksum and validation hash. The experimental image was neither flashed nor
retained as a repository artifact.

No modification may be made to the installed global framework or repository.
Use an isolated PlatformIO package directory or disposable host environment.

## Phase 2 local relink kit

`tools/create_esp32_relink_kit.py` creates a path-independent local kit from the
verified `esp32_public` build and a verbose build log. The kit contains:

- exact WSPR Beacon source and source commit;
- exact Arduino core and board-variant LGPL source from the official 2.0.17
  source archive (commit `5e19e086c43d0fa5e5a596497ff8f11a0a43f6c2`);
- unstripped project and non-LGPL application objects;
- project/direct-library static archives and `libFrameworkArduino.a`; ESP-IDF
  SDK archives remain an explicitly declared framework-package prerequisite;
- corresponding source for the four linked Arduino libraries and all five
  direct Etherkit/Adafruit archives included in the kit;
- the exact link command, with linker scripts supplied by the pinned framework
  package declared as a prerequisite;
- compilation definitions and board configuration;
- exact PlatformIO, platform, framework and toolchain identifiers;
- conversion commands from ELF to target BINs;
- license texts and notices;
- a manifest that checks every included input and LGPL source hash.

The generated command file has placeholders for the kit, framework, toolchain,
package root, Python and esptool paths and contains no private host path. The
declared prerequisites are PlatformIO espressif32 7.1.3, the exact resolved
Arduino package, Xtensa toolchain 8.4.0+2021r2-patch5 and esptool 4.11.0. The
official Arduino 2.0.17 source archive SHA-256 is
`fb4024408494d6c45701dc8e1c595f84af543e66e06b00a21435a632d577fa61`.

On 2026-11-06 a fresh kit was copied to a separate `/tmp` tree with isolated
copies of the declared framework, toolchain and esptool dependencies. A `nop`
marker was added to `esp32-hal-misc.c::__yield()`, only that core object was
compiled and replaced in `libFrameworkArduino.a`, and the kit's direct link and
`elf2image` commands were run. All 79 project `src/*.o` hashes remained
unchanged, the Arduino archive changed, and esptool accepted the resulting
1,017,760-byte image with a valid checksum and validation hash. This was a
relink, not a full project rebuild.

The local test does not authorize publication. Phase 2 closure resolved
`libesp_diagnostics.a` provenance and produced a complete technical license
inventory for all 65 incorporated archives. Legal review of the combined
GPL/LGPL/Apache/runtime distribution model is still required. Packages
therefore remain `COMPLIANCE_INCOMPLETE`, `release_ready: false` and not
authorized for public distribution.

## Acceptance test for a future autonomous kit

On a clean machine, without the original build directory, a reviewer must be
able to modify an LGPL component, rebuild that component, relink unchanged
application objects and create a valid target image using only the kit and its
documented prerequisites. The resulting image need not be byte-identical after
the deliberate modification, but all inputs and changes must be auditable.
