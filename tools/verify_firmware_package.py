#!/usr/bin/env python3
"""Validate a staged technical firmware package without accessing hardware."""

from __future__ import annotations

import argparse
import hashlib
import gzip
import json
from pathlib import Path
import re
import sys
import tarfile


PROJECT = "WSPR Beacon 2.0"
SCHEMA = 2
RELEASE_CANDIDATE = "2.0.0-rc.1"
LEGAL_REVIEW_STATUS = "NOT_APPROVED_FOR_PUBLIC_DISTRIBUTION"
REQUIRED_PACKAGE_FILES = {
    "LICENSE": "project-license",
    "COPYRIGHT.md": "project-copyright",
    "THIRD_PARTY_NOTICES.md": "third-party-notices",
    "GPL-3.0.txt": "third-party-license",
    "LGPL-2.1.txt": "third-party-license",
    "BSD-2-Clause-Adafruit-GFX.txt": "third-party-license",
    "BSD-3-Clause-Adafruit-SSD1306.txt": "third-party-license",
    "MIT-Adafruit-BusIO.txt": "third-party-license",
    "ESPRESSIF-NONOS-SDK.txt": "third-party-license",
    "GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt": "third-party-license",
    "INSTALLATION.md": "installation",
    "RELEASE_NOTES.md": "release-notes",
    "SOURCE_DISTRIBUTION.md": "source-distribution",
    "ESP8266_LICENSE_REVIEW.md": "license-review",
}
REVISION_3_DOCUMENTATION = {
    "BUILDING.md": "building",
    "USER_GUIDE.md": "user-guide",
    "HARDWARE.md": "hardware-guide",
}
ESP32_EXCLUDED_FILES = {"ESPRESSIF-NONOS-SDK.txt",
                        "ESP8266_LICENSE_REVIEW.md", "RELEASE_NOTES.md",
                        "THIRD_PARTY_NOTICES.md", "SOURCE_DISTRIBUTION.md"}
LEGACY_SKETCHES = {
    "wspr_5.ino", "FrequencySelection.ino", "Wifi_utility.ino",
    "display.ino", "gps.ino",
}
OFFSETS = {
    "ESP8266": {"firmware.bin": "0x0"},
    "ESP32": {
        "bootloader.bin": "0x1000",
        "partitions.bin": "0x8000",
        "boot_app0.bin": "0xE000",
        "firmware.bin": "0x10000",
    },
}
TARGET_BOARDS = {"ESP8266": "nodemcuv2", "ESP32": "esp32dev"}
FLASH_PARAMETERS = {
    "ESP8266": {"mode": "dio", "frequency": "40m", "size": "4MB",
                "recommended_baud": 115200},
    "ESP32": {"mode": "dio", "frequency": "40m", "size": "4MB",
              "recommended_baud": 460800},
}
ESP32_LAYOUT = (
    ("nvs", 0x9000, 0x5000),
    ("otadata", 0xE000, 0x2000),
    ("app0", 0x10000, 0x140000),
    ("app1", 0x150000, 0x140000),
    ("spiffs", 0x290000, 0x160000),
    ("coredump", 0x3F0000, 0x10000),
)
SAFE_NAME = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]*")
SHA256 = re.compile(r"[0-9a-f]{64}")
COMMIT = re.compile(r"[0-9a-f]{7,40}")


class VerificationError(RuntimeError):
    pass


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def safe_file(package: Path, name: object) -> Path:
    if not isinstance(name, str) or not SAFE_NAME.fullmatch(name):
        raise VerificationError("unsafe package filename")
    path = package / name
    if path.is_symlink() or not path.is_file():
        raise VerificationError(f"missing or unsafe package file: {name}")
    return path


def component_kind(filename: str) -> str:
    for kind in ("bootloader.bin", "partitions.bin", "boot_app0.bin",
                 "firmware.bin"):
        if filename.endswith("-" + kind):
            return kind
    raise VerificationError(f"unsupported component filename: {filename}")


def linked_archives(map_text: str) -> list[dict[str, str]]:
    heading = "Archive member included to satisfy reference by file (symbol)"
    if not map_text.startswith(heading):
        return []
    archives = set()
    for line in map_text[len(heading):].lstrip("\n").splitlines():
        if not line.strip() and not archives:
            continue
        if not line.strip():
            break
        if line[0].isspace():
            continue
        match = re.match(r"(.+?\.a)\([^()]+\)", line)
        if match:
            archives.add(match.group(1))
    return [{"archive": archive, "evidence": "link-map-archive-member"}
            for archive in sorted(archives)]


def read_checksums(package: Path) -> dict[str, str]:
    path = safe_file(package, "SHA256SUMS")
    checksums = {}
    try:
        for line in path.read_text(encoding="ascii").splitlines():
            match = re.fullmatch(r"([0-9a-f]{64})  ([A-Za-z0-9][A-Za-z0-9._-]*)", line)
            if not match or match.group(2) in checksums:
                raise VerificationError("malformed or duplicate SHA256SUMS entry")
            checksums[match.group(2)] = match.group(1)
    except UnicodeError as error:
        raise VerificationError("SHA256SUMS is not ASCII") from error
    if not checksums:
        raise VerificationError("empty SHA256SUMS")
    return checksums


def verify_source_archive(path: Path, commit: str) -> None:
    try:
        with tarfile.open(path, "r:gz") as archive:
            members = archive.getmembers()
            if not members:
                raise VerificationError("empty corresponding-source archive")
            names = [member.name for member in members]
            if any(member.issym() or member.islnk() or member.name.startswith("/") or
                   ".." in Path(member.name).parts for member in members):
                raise VerificationError("unsafe corresponding-source archive")
            roots = {Path(name).parts[0] for name in names}
            if len(roots) != 1:
                raise VerificationError("invalid corresponding-source root")
            root = next(iter(roots))
            relative = {str(Path(name).relative_to(root)) for name in names}
            required = {"SOURCE_COMMIT", "LICENSE", "COPYRIGHT.md",
                        "platformio.ini", "tools/stage_firmware.py",
                        "tools/verify_firmware_package.py",
                        "tools/generate_web_assets.py"}
            if not required.issubset(relative) or LEGACY_SKETCHES & relative:
                raise VerificationError("incomplete corresponding source")
            marker = archive.extractfile(f"{root}/SOURCE_COMMIT")
            if marker is None or marker.read().decode("ascii") != commit + "\n":
                raise VerificationError("source archive commit mismatch")
    except (OSError, tarfile.TarError, UnicodeError) as error:
        raise VerificationError("invalid corresponding-source archive") from error


def verify_document_links(package: Path, filenames: set[str]) -> None:
    """Require every relative Markdown link in packaged operator docs to resolve."""
    for filename in sorted(filenames):
        path = safe_file(package, filename)
        try:
            contents = path.read_text(encoding="utf-8")
        except UnicodeError as error:
            raise VerificationError("packaged documentation is not UTF-8") from error
        for match in re.finditer(r"\[[^\]]+\]\(([^)]+)\)", contents):
            target = match.group(1).split("#", 1)[0]
            if not target or re.match(r"[A-Za-z][A-Za-z0-9+.-]*:", target):
                continue
            if target not in filenames:
                raise VerificationError(
                    f"unresolved packaged documentation link: {filename} -> {target}")


def verify_package(package: Path) -> dict:
    if not package.is_dir() or package.is_symlink():
        raise VerificationError("package path is not a directory")
    manifests = [path for path in package.iterdir()
                 if path.is_file() and path.name.endswith("-manifest.json")]
    if len(manifests) != 1:
        raise VerificationError("package must contain exactly one manifest")
    manifest_path = manifests[0]
    if manifest_path.is_symlink():
        raise VerificationError("manifest must be a regular package file")
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise VerificationError("malformed manifest JSON") from error
    if not isinstance(manifest, dict) or manifest.get("manifest_schema") != SCHEMA:
        raise VerificationError("unsupported manifest schema")
    if manifest.get("project") != PROJECT or \
            manifest.get("version") != RELEASE_CANDIDATE or \
            manifest.get("firmware_version") != "dev":
        raise VerificationError("unexpected project or version")
    target = manifest.get("target")
    revision = manifest.get("packaging_revision", 1)
    profile = manifest.get("profile")
    if type(revision) is not int or revision not in (1, 2, 3) or \
            target not in OFFSETS or \
            profile not in ("PUBLIC", "DEVELOPMENT") or \
            (revision == 1 and (target != "ESP8266" or profile != "PUBLIC")):
        raise VerificationError("unsupported target or packaging revision")
    expected_environment = target.lower() + ("_public" if profile == "PUBLIC" else "")
    if manifest.get("build_environment") != expected_environment:
        raise VerificationError("environment and target mismatch")
    clean = manifest.get("git_worktree") == "clean"
    public_eligible = clean and profile == "PUBLIC" and target == "ESP8266"
    technical_eligible = clean if revision >= 2 else clean and profile == "PUBLIC"
    if manifest.get("git_worktree") not in ("clean", "dirty") or \
            manifest.get("public_release_eligible") is not public_eligible or \
            manifest.get("technical_package_eligible") is not technical_eligible:
        raise VerificationError("invalid package eligibility")
    if revision == 1 and not clean:
        raise VerificationError("legacy package must have a clean worktree")
    if manifest.get("public_distribution_authorized") is not False or \
            manifest.get("legal_review_status") != LEGAL_REVIEW_STATUS:
        raise VerificationError("invalid release or legal-review status")
    commit = manifest.get("source_commit")
    if not isinstance(commit, str) or not COMMIT.fullmatch(commit):
        raise VerificationError("invalid source commit")
    firmware_commit = manifest.get("firmware_source_commit")
    if not isinstance(firmware_commit, str) or not COMMIT.fullmatch(firmware_commit) or \
            not commit.startswith(firmware_commit):
        raise VerificationError("firmware and source commit mismatch")
    metadata = manifest.get("platformio")
    if not isinstance(metadata, dict) or not metadata.get("core_version") or \
            not metadata.get("resolved_packages"):
        raise VerificationError("missing build metadata")

    expected_names = {"SHA256SUMS", manifest_path.name}
    package_files = manifest.get("package_files")
    if not isinstance(package_files, list):
        raise VerificationError("missing package material inventory")
    expected_roles = dict(REQUIRED_PACKAGE_FILES)
    if target == "ESP32":
        for name in ESP32_EXCLUDED_FILES:
            expected_roles.pop(name)
    if revision >= 3:
        expected_roles.update(REVISION_3_DOCUMENTATION)
    source_name = f"wspr-beacon-{RELEASE_CANDIDATE}-source.tar.gz"
    expected_roles[source_name] = "corresponding-source"
    if revision >= 2:
        stem = f"wspr-beacon-{RELEASE_CANDIDATE}-{profile.lower()}-{target.lower()}-{firmware_commit}"
        if not clean:
            stem += "-dirty"
        technical_names = {
            f"{stem}-firmware.elf": "firmware-elf",
            f"{stem}-firmware.map": "link-map",
            f"{stem}-firmware.map.gz": "compressed-link-map",
            f"{stem}-dependencies.json": "dependency-inventory",
        }
        expected_roles.update(technical_names)
    seen_package_files = set()
    for item in package_files:
        if not isinstance(item, dict):
            raise VerificationError("invalid package material entry")
        filename = item.get("filename")
        if filename in seen_package_files or expected_roles.get(filename) != item.get("role"):
            raise VerificationError("unexpected package material")
        path = safe_file(package, filename)
        digest = item.get("sha256")
        if not isinstance(digest, str) or not SHA256.fullmatch(digest) or \
                digest != sha256_file(path) or item.get("size_bytes") != path.stat().st_size:
            raise VerificationError("package material integrity mismatch")
        seen_package_files.add(filename)
        expected_names.add(filename)
    if seen_package_files != set(expected_roles):
        raise VerificationError("missing required package material")
    verify_source_archive(package / source_name, commit)
    if revision >= 3:
        documentation = {"INSTALLATION.md", *REVISION_3_DOCUMENTATION}
        verify_document_links(package, documentation)

        flash_parameters = manifest.get("flash_parameters")
        if type(flash_parameters) is not dict or \
                type(flash_parameters.get("recommended_baud")) is not int or \
                flash_parameters != FLASH_PARAMETERS[target]:
            raise VerificationError("invalid or mismatched flash parameters")
        if manifest.get("board") != TARGET_BOARDS[target]:
            raise VerificationError("invalid or mismatched target board")
    elif "flash_parameters" in manifest or "board" in manifest:
        raise VerificationError("unexpected revision-3 target metadata")

    if revision >= 2:
        if manifest.get("elf_processing") != "strip-debug":
            raise VerificationError("unexpected ELF packaging method")
        summaries = (("elf", "firmware.elf"),
                     ("link_map", "firmware.map"),
                     ("link_map_gzip", "firmware.map.gz"),
                     ("dependency_inventory", "dependencies.json"))
        for field, suffix in summaries:
            name = f"{stem}-{suffix}"
            if manifest.get(f"{field}_filename") != name or \
                    manifest.get(f"{field}_sha256") != sha256_file(package / name):
                raise VerificationError(f"{field} summary mismatch")
        map_path = package / f"{stem}-firmware.map"
        try:
            map_bytes = map_path.read_bytes()
            with gzip.open(package / f"{stem}-firmware.map.gz", "rb") as source:
                if source.read() != map_bytes:
                    raise VerificationError("compressed link map mismatch")
            map_text = map_bytes.decode("utf-8")
            inventory = json.loads((package / f"{stem}-dependencies.json").read_text())
        except (OSError, UnicodeError, ValueError, gzip.BadGzipFile) as error:
            raise VerificationError("invalid map or dependency inventory") from error
        if not map_text.startswith("Archive member included to satisfy reference") or \
                re.search(r"(?<![A-Za-z0-9._-])/(?:home|Users|tmp|opt|usr|var|workspace|mnt|run|private|root)/[^\s():]+", map_text):
            raise VerificationError("invalid or nonportable linker map")
        if not isinstance(inventory, dict) or inventory.get("schema") != 1 or \
                inventory.get("target") != target or \
                inventory.get("profile") != profile or \
                inventory.get("build_environment") != expected_environment or \
                inventory.get("source_commit") != commit or \
                inventory.get("third_party_source_archives_included") is not False or \
                inventory.get("lgpl_relink_materials_included") is not False:
            raise VerificationError("dependency inventory identity mismatch")
        declared = inventory.get("declared", {})
        resolved = inventory.get("resolved", {})
        expected_platform = "platformio/espressif8266@4.2.1" if target == "ESP8266" else \
                            "platformio/espressif32@7.1.3"
        framework_name = "framework-arduinoespressif8266" if target == "ESP8266" else \
                         "framework-arduinoespressif32"
        if declared.get("platform") != expected_platform or \
                declared.get("framework") != "arduino" or \
                declared.get("libraries") != manifest.get("direct_dependencies") or \
                not isinstance(resolved, dict) or \
                (resolved.get("platform") or {}).get("name") != (
                    "Platform espressif8266" if target == "ESP8266" else
                    "Platform espressif32") or \
                (resolved.get("framework") or {}).get("name") != framework_name or \
                not resolved.get("toolchains"):
            raise VerificationError("dependency inventory does not match target")
        metadata_packages = {(item.get("name"), item.get("version"))
                             for item in metadata["resolved_packages"]
                             if isinstance(item, dict)}
        for kind in ("platform", "framework"):
            item = resolved[kind]
            if (item.get("name"), item.get("version")) not in metadata_packages:
                raise VerificationError("resolved package disagrees with manifest")
        for kind in ("toolchains", "libraries"):
            if not isinstance(resolved.get(kind), list) or any(
                    not isinstance(item, dict) or
                    (item.get("name"), item.get("version")) not in metadata_packages
                    for item in resolved[kind]):
                raise VerificationError("resolved dependency disagrees with manifest")
        direct_names = {entry.split("/", 1)[1].rsplit("@", 1)[0]
                        for entry in declared["libraries"]}
        if any(item.get("relationship") != (
                "direct" if item["name"] in direct_names else "transitive")
                for item in resolved["libraries"]):
            raise VerificationError("incorrect direct or transitive dependency")
        archives = inventory.get("linked_archive_members")
        if not isinstance(archives, list) or not archives or \
                archives != linked_archives(map_text):
            raise VerificationError("unsupported linked archive evidence")
        materials = inventory.get("archived_materials")
        expected_materials = {
            item["filename"]: item["sha256"] for item in package_files
            if item["role"] in ("firmware-elf", "link-map", "compressed-link-map")
        }
        expected_materials.update({item["filename"]: item["sha256"]
                                   for item in manifest.get("components", [])})
        if not isinstance(materials, list) or {
                item.get("filename"): item.get("sha256") for item in materials
                if isinstance(item, dict)} != expected_materials or \
                len(materials) != len(expected_materials):
            raise VerificationError("archived dependency material mismatch")
        inventory_text = json.dumps(inventory)
        if re.search(r"(?i)(?:/home/|/Users/|[A-Z]:\\|password|token|secret|api[_-]?key)",
                     inventory_text):
            raise VerificationError("sensitive or local dependency inventory data")

    components = manifest.get("components")
    if not isinstance(components, list) or len(components) != len(OFFSETS[target]):
        raise VerificationError("incorrect component inventory")
    seen_kinds = set()
    for component in components:
        if not isinstance(component, dict):
            raise VerificationError("invalid component entry")
        filename = component.get("filename")
        path = safe_file(package, filename)
        kind = component_kind(filename)
        if kind in seen_kinds or component.get("offset") != OFFSETS[target].get(kind):
            raise VerificationError("duplicate component or invalid flash offset")
        seen_kinds.add(kind)
        digest = component.get("sha256")
        if not isinstance(digest, str) or not SHA256.fullmatch(digest):
            raise VerificationError("invalid component SHA-256")
        if component.get("size_bytes") != path.stat().st_size:
            raise VerificationError("component size mismatch")
        if sha256_file(path) != digest:
            raise VerificationError("component SHA-256 mismatch")
        expected_names.add(filename)
    if seen_kinds != set(OFFSETS[target]):
        raise VerificationError("missing required component")

    firmware = next(item for item in components
                    if component_kind(item["filename"]) == "firmware.bin")
    if manifest.get("firmware_filename") != firmware["filename"] or \
            manifest.get("firmware_sha256") != firmware["sha256"] or \
            manifest.get("firmware_size_bytes") != firmware["size_bytes"]:
        raise VerificationError("firmware summary mismatch")

    if revision >= 2:
        for name in (firmware["filename"], manifest["elf_filename"]):
            binary = (package / name).read_bytes()
            if re.search(rb"/(?:home|Users|tmp|workspace|mnt|private|root)/[^\x00\s]+",
                         binary) or re.search(rb"[A-Z]:\\(?:Users|Documents)\\", binary):
                raise VerificationError("local absolute path in firmware artifact")

    if target == "ESP32":
        partition = manifest.get("partition_table")
        layout = partition.get("layout") if isinstance(partition, dict) else None
        try:
            decoded = tuple((row["name"], int(row["offset"], 0),
                             int(row["size"], 0)) for row in layout)
        except (TypeError, KeyError, ValueError):
            raise VerificationError("invalid ESP32 partition layout") from None
        partition_component = next(item for item in components
                                   if component_kind(item["filename"]) ==
                                   "partitions.bin")
        if decoded != ESP32_LAYOUT or partition.get("nvs_range") != {
                "offset": "0x9000", "size": "0x5000"} or \
                partition.get("sha256") != partition_component["sha256"]:
            raise VerificationError("ESP32 partition identity mismatch")
    elif "partition_table" in manifest:
        raise VerificationError("unexpected partition metadata")

    checksums = read_checksums(package)
    if set(checksums) != expected_names - {"SHA256SUMS"}:
        raise VerificationError("SHA256SUMS inventory mismatch")
    for filename, digest in checksums.items():
        if sha256_file(safe_file(package, filename)) != digest:
            raise VerificationError("SHA256SUMS mismatch")
    actual_names = {path.name for path in package.iterdir()}
    if actual_names != expected_names:
        raise VerificationError("unexpected package file")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate a WSPR Beacon PUBLIC firmware package")
    parser.add_argument("package", type=Path)
    arguments = parser.parse_args()
    try:
        manifest = verify_package(arguments.package.resolve())
    except VerificationError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    print(
        f"verified technical package: {manifest['target']} {manifest['profile']} "
        f"commit {manifest['source_commit']}; "
        f"legal status {manifest['legal_review_status']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
