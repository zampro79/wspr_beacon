# LGPL relinking materials

Status: Phase 1 feasibility evidence; not yet an autonomous distribution kit.

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

## Candidate relink-kit contents

- exact WSPR Beacon source and source commit;
- exact LGPL library/framework source and any patches;
- unstripped project and non-LGPL application objects;
- permitted static archives required by the final link;
- linker scripts, response files, map and exact link command;
- compilation definitions and board configuration;
- exact PlatformIO, platform, framework and toolchain identifiers;
- conversion commands from ELF to target BINs;
- license texts and notices;
- a verifier that checks input hashes and proves project objects were reused.

ESP-IDF or binary archives must not be copied into a public kit until their
redistribution terms are classified. Consequently, the Phase 1 package carries
link-map/ELF evidence and this procedure but marks the kit
`FEASIBILITY_EVIDENCE_ONLY_NOT_AUTONOMOUS`.

## Acceptance test for a future autonomous kit

On a clean machine, without the original build directory, a reviewer must be
able to modify an LGPL component, rebuild that component, relink unchanged
application objects and create a valid target image using only the kit and its
documented prerequisites. The resulting image need not be byte-identical after
the deliberate modification, but all inputs and changes must be auditable.
