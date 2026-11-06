# Future corresponding-source distribution model

No official source archive or firmware release exists today. This document is
a checklist for a future ESP8266-only release, not an authorization to publish
one. Public visibility of the source repository requires separate, explicit
owner authorization. If granted, repository visibility alone would not create
an official source release, authorize a firmware archive/package or authorize
distribution of precompiled firmware. Original modern project material is
licensed under GPL-3.0-or-later; this does not by itself clear third-party
binary redistribution. Existing package status remains
`public_distribution_authorized: false` and
`NOT_APPROVED_FOR_PUBLIC_DISTRIBUTION`.

## Binary-to-source identity

Every firmware binary should identify and be accompanied by:

- the exact full Git commit and an approved immutable release tag;
- a source archive produced from that commit, including subcomponents required
  to rebuild the binary;
- the exact `platformio.ini`, build profile and pinned platform/library versions;
- build, web-asset generation, firmware-identity, staging and package-validation
  scripts used for the binary;
- deterministic manifest, component offsets and SHA-256 checksums;
- build instructions sufficient to reproduce the controlled build; and
- `THIRD_PARTY_NOTICES.md`, the relevant exact license texts, and retained
  source-file notices.

The RC tooling prepares a deterministic `2.0.0-rc.1` source archive directly
from the exact clean Git commit. It normalizes archive metadata, adds a
`SOURCE_COMMIT` marker, rejects the removed legacy sketches and includes the
project source, `platformio.ini`, documentation, identity/asset scripts,
packaging/verification tools and tests present at that commit. The package
verifier checks the marker and required source paths without extracting files.

A Git hosting link alone is not assumed to satisfy every corresponding-source
or written-offer obligation. The source offered must remain matched to each
distributed binary and available for the legally required period.

## Dependency source and modifications

The archive or durable accompanying offer must cover the exact GPL/LGPL library
versions linked into the binary, including any local modifications. Upstream
URLs are useful provenance but do not replace preservation of the exact source
used. Generated web assets must be accompanied by their preferred editable
source and deterministic generation procedure.

Because the firmware is statically linked, recipients must receive a practical
way to modify and relink LGPL-covered components. LGPL-2.1 section 6a permits
the complete work that uses the library in object-code and/or source form. A
source-based route therefore requires retained exact dependency/framework
source, the matched project source, build scripts and a tested relink procedure;
an upstream URL alone is insufficient. The RC currently lacks the retained
dependency source set, so no speculative object bundle has been added and this
remains a release blocker.

Pinned direct dependency declarations and resolved package metadata provide
reliable acquisition instructions for rebuilding. They are not durable copies
of every dependency source. See [the targeted ESP8266 license
review](ESP8266_LICENSE_REVIEW.md) for the missing LGPL/SDK/GCC evidence.

The GCC Runtime Library Exception 3.1 is preserved for the linked libgcc and
libstdc++ runtime portions. It does not apply to the ESP8266 SDK. Binary SDK
components require their own verified notices and redistribution analysis. Items classified
`REQUIRES_LEGAL_REVIEW` in the third-party inventory cannot be treated as
cleared by inclusion of project source alone.

## Release checklist

1. Start from a clean, reviewed commit and approved immutable tag.
2. Confirm the GPL-3.0-or-later project license, copyright notice and scope are
   included unchanged.
3. Re-resolve dependencies without upgrades and record their exact sources.
4. Build only `esp8266_public` in an isolated checkout.
5. Verify embedded identity, profile, target, manifest and all hashes.
6. Assemble project source, dependency source, build inputs, notices and exact
   license texts; add LGPL relinking materials once their form is approved.
7. Verify the archive contains no credentials, private paths, DEVELOPMENT
   identity defaults, stale binaries or unrelated artifacts.
8. Independently verify the binary package and corresponding-source archive.
9. Record retention location and duration before publishing anything.

The first release candidate is ESP8266-only. ESP32 remains experimental and is
outside this distribution plan until its hardware and licensing/package
evidence receives a separate review.
