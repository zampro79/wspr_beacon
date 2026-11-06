#!/usr/bin/env python3
"""Stage deterministic firmware artifacts without modifying PlatformIO outputs."""

from __future__ import annotations

import argparse
from contextlib import contextmanager
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile

PROJECT = "WSPR Beacon 2.0"
FIRMWARE_VERSION = "dev"
RELEASE_CANDIDATE = "2.0.0-rc.1"
MANIFEST_SCHEMA = 2
PACKAGING_REVISION = 3
LEGAL_REVIEW_STATUS = "NOT_APPROVED_FOR_PUBLIC_DISTRIBUTION"
ENVIRONMENTS = {
    "esp8266": ("DEVELOPMENT", "ESP8266"),
    "esp8266_public": ("PUBLIC", "ESP8266"),
    "esp32": ("DEVELOPMENT", "ESP32"),
    "esp32_public": ("PUBLIC", "ESP32"),
}
OFFSETS = {
    "ESP8266": (("firmware.bin", "0x0"),),
    "ESP32": (
        ("bootloader.bin", "0x1000"),
        ("partitions.bin", "0x8000"),
        ("boot_app0.bin", "0xE000"),
        ("firmware.bin", "0x10000"),
    ),
}
TARGET_BOARDS = {"ESP8266": "nodemcuv2", "ESP32": "esp32dev"}
FLASH_PARAMETERS = {
    "ESP8266": {"mode": "dio", "frequency": "40m", "size": "4MB",
                "recommended_baud": 115200},
    "ESP32": {"mode": "dio", "frequency": "40m", "size": "4MB",
              "recommended_baud": 460800},
}
DIRECT_DEPENDENCIES = {
    "ESP8266": [
        "adafruit/Adafruit GFX Library@1.12.6",
        "adafruit/Adafruit SSD1306@2.5.17",
        "etherkit/Etherkit JTEncode@1.3.1",
        "etherkit/Etherkit Si5351@2.2.0",
        "plerup/EspSoftwareSerial@8.2.0",
    ],
    "ESP32": [
        "adafruit/Adafruit GFX Library@1.12.6",
        "adafruit/Adafruit SSD1306@2.5.17",
        "etherkit/Etherkit JTEncode@1.3.1",
        "etherkit/Etherkit Si5351@2.2.0",
    ],
}
PROVENANCE_FILE = "firmware.provenance.json"
PACKAGE_MATERIALS = (
    ("LICENSE", "LICENSE", "project-license"),
    ("COPYRIGHT.md", "COPYRIGHT.md", "project-copyright"),
    ("THIRD_PARTY_NOTICES.md", "THIRD_PARTY_NOTICES.md", "third-party-notices"),
    ("licenses/GPL-3.0.txt", "GPL-3.0.txt", "third-party-license"),
    ("licenses/LGPL-2.1.txt", "LGPL-2.1.txt", "third-party-license"),
    ("licenses/BSD-2-Clause-Adafruit-GFX.txt",
     "BSD-2-Clause-Adafruit-GFX.txt", "third-party-license"),
    ("licenses/BSD-3-Clause-Adafruit-SSD1306.txt",
     "BSD-3-Clause-Adafruit-SSD1306.txt", "third-party-license"),
    ("licenses/MIT-Adafruit-BusIO.txt", "MIT-Adafruit-BusIO.txt",
     "third-party-license"),
    ("licenses/ESPRESSIF-NONOS-SDK.txt", "ESPRESSIF-NONOS-SDK.txt",
     "third-party-license"),
    ("licenses/GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt",
     "GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt", "third-party-license"),
    ("docs/INSTALLATION.md", "INSTALLATION.md", "installation"),
    ("docs/BUILDING.md", "BUILDING.md", "building"),
    ("docs/USER_GUIDE.md", "USER_GUIDE.md", "user-guide"),
    ("docs/HARDWARE.md", "HARDWARE.md", "hardware-guide"),
    ("docs/RELEASE_NOTES_ESP8266_RC.md", "RELEASE_NOTES.md", "release-notes"),
    ("docs/SOURCE_DISTRIBUTION.md", "SOURCE_DISTRIBUTION.md",
     "source-distribution"),
    ("docs/ESP8266_LICENSE_REVIEW.md", "ESP8266_LICENSE_REVIEW.md",
     "license-review"),
)
LEGACY_SKETCHES = {
    "wspr_5.ino", "FrequencySelection.ino", "Wifi_utility.ino",
    "display.ino", "gps.ino",
}
PARTITION_LAYOUT = (
    ("nvs", "0x9000", "0x5000"),
    ("otadata", "0xe000", "0x2000"),
    ("app0", "0x10000", "0x140000"),
    ("app1", "0x150000", "0x140000"),
    ("spiffs", "0x290000", "0x160000"),
    ("coredump", "0x3F0000", "0x10000"),
)


class StagingError(RuntimeError):
    pass


def run(repo: Path, *command: str) -> str:
    return subprocess.check_output(
        command, cwd=repo, stderr=subprocess.DEVNULL, text=True
    ).strip()


def git_identity(repo: Path) -> tuple[str, bool]:
    try:
        commit = run(repo, "git", "rev-parse", "--short=7", "HEAD")
        dirty = bool(run(repo, "git", "status", "--porcelain"))
    except (OSError, subprocess.CalledProcessError):
        return "unknown", True
    if not re.fullmatch(r"[0-9a-f]{7,40}", commit):
        return "unknown", True
    return commit, dirty


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def artifact_stem(profile: str, target: str, commit: str, dirty: bool) -> str:
    suffix = "-dirty" if dirty else ""
    return (
        f"wspr-beacon-{RELEASE_CANDIDATE}-{profile.lower()}-"
        f"{target.lower()}-{commit}{suffix}"
    )


def parse_packages(text: str) -> list[dict[str, str]]:
    packages = {}
    pattern = re.compile(r"^[├└│ ]*(?:── )?([^@\n]+?) @ ([^ ]+)")
    for line in text.splitlines():
        match = pattern.match(line.strip())
        if match:
            packages[match.group(1).strip()] = match.group(2).strip()
    return [
        {"name": name, "version": packages[name]}
        for name in sorted(packages)
    ]


def platformio_metadata(repo: Path, environment: str) -> dict:
    try:
        core = run(repo, "pio", "--version").replace("PlatformIO Core, version ", "")
        listing = run(repo, "pio", "pkg", "list", "-e", environment)
    except (OSError, subprocess.CalledProcessError):
        return {"core_version": "unknown", "resolved_packages": []}
    return {"core_version": core, "resolved_packages": parse_packages(listing),
            "package_sources": parse_package_sources(listing)}


def parse_package_sources(listing: str) -> dict[str, str]:
    """Keep registry requirements, without exposing local package locations."""
    sources = {}
    for line in listing.splitlines():
        match = re.match(r"^[├└│ ]*(?:── )?([^@\n]+?) @ [^ ]+ \(required: ([^)]+)\)",
                         line.strip())
        if match:
            sources[match.group(1).strip()] = match.group(2).strip()
    return sources


def portable_map(contents: str, repo: Path) -> str:
    """Preserve linker evidence while removing host absolute paths."""
    def replace(match: re.Match) -> str:
        path = match.group()
        repository = str(repo.resolve()) + "/"
        if path.startswith(repository):
            return "project/" + path[len(repository):]
        if "/.platformio/packages/" in path:
            return "platformio/packages/" + path.split("/.platformio/packages/", 1)[1]
        return "external/" + Path(path).name
    return re.sub(
        r"(?<![A-Za-z0-9._-])/(?:home|Users|tmp|opt|usr|var|workspace|mnt|run|private|root)/[^\s():]+",
        replace, contents)


def linked_archives(map_text: str) -> list[dict[str, str]]:
    """Only the GNU ld archive-member inclusion table proves incorporation."""
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


def dependency_inventory(environment: str, target: str, profile: str,
                         commit: str, metadata: dict, map_text: str,
                         materials: list[dict]) -> dict:
    platform = "platformio/espressif8266@4.2.1" if target == "ESP8266" else \
               "platformio/espressif32@7.1.3"
    framework = "framework-arduinoespressif8266" if target == "ESP8266" else \
                "framework-arduinoespressif32"
    resolved = metadata["resolved_packages"]
    sources = metadata.get("package_sources", {})
    direct_names = {entry.split("/", 1)[1].rsplit("@", 1)[0]
                    for entry in DIRECT_DEPENDENCIES[target]}
    return {
        "schema": 1, "target": target, "build_environment": environment,
        "profile": profile, "source_commit": commit,
        "declared": {"platform": platform, "framework": "arduino",
                     "libraries": DIRECT_DEPENDENCIES[target]},
        "resolved": {
            "platform": next((dict(item, source=sources.get(item["name"], platform))
                              for item in resolved if item["name"] ==
                              ("Platform espressif8266" if target == "ESP8266" else
                               "Platform espressif32")), None),
            "framework": next((dict(item, source=sources.get(item["name"], "platformio"))
                               for item in resolved if item["name"] == framework), None),
            "toolchains": [dict(item, source=sources.get(item["name"], "platformio"))
                           for item in resolved if item["name"].startswith("toolchain-")],
            "libraries": [dict(item, source=sources.get(item["name"], "PlatformIO registry"),
                               relationship="direct" if item["name"] in direct_names
                               else "transitive")
                          for item in resolved if item["name"] not in
                          {"Platform espressif8266", "Platform espressif32", framework}
                          and not item["name"].startswith(("tool-", "toolchain-"))],
        },
        "linked_archive_members": linked_archives(map_text),
        "linked_archive_evidence": "GNU ld archive-member inclusion table",
        "archived_materials": materials,
        "third_party_source_archives_included": False,
        "lgpl_relink_materials_included": False,
    }


def require_metadata(metadata: dict) -> None:
    if metadata.get("core_version") in (None, "", "unknown"):
        raise StagingError("missing PlatformIO Core metadata")
    packages = metadata.get("resolved_packages")
    if not isinstance(packages, list) or not packages:
        raise StagingError("missing resolved package metadata")


def boot_app0_path(packages_dir: Path | None = None) -> Path:
    packages = packages_dir or Path(os.environ.get(
        "PLATFORMIO_PACKAGES_DIR", Path.home() / ".platformio" / "packages"))
    return packages / "framework-arduinoespressif32" / "tools" / "partitions" / "boot_app0.bin"


def objcopy_path(target: str, packages_dir: Path | None = None) -> Path:
    packages = packages_dir or Path(os.environ.get(
        "PLATFORMIO_PACKAGES_DIR", Path.home() / ".platformio" / "packages"))
    if target == "ESP32":
        return packages / "toolchain-xtensa-esp32" / "bin" / "xtensa-esp32-elf-objcopy"
    return packages / "toolchain-xtensa" / "bin" / "xtensa-lx106-elf-objcopy"


def package_elf(source: Path, destination: Path, target: str,
                packages_dir: Path | None = None) -> None:
    tool = objcopy_path(target, packages_dir)
    try:
        subprocess.run([str(tool), "--strip-debug", str(source), str(destination)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except (OSError, subprocess.CalledProcessError) as error:
        raise StagingError("cannot prepare portable firmware ELF") from error


def component_sources(repo: Path, environment: str, target: str,
                      packages_dir: Path | None = None) -> list[tuple[Path, str, str]]:
    build = repo / ".pio" / "build" / environment
    sources = []
    for name, offset in OFFSETS[target]:
        source = boot_app0_path(packages_dir) if name == "boot_app0.bin" else build / name
        if not source.is_file():
            raise StagingError(f"missing build component: {name}")
        sources.append((source, name, offset))
    return sources


def load_provenance(build_repo: Path, environment: str, expected_commit: str,
                    profile: str, target: str) -> dict:
    path = build_repo / ".pio" / "build" / environment / PROVENANCE_FILE
    try:
        evidence = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise StagingError("missing or invalid firmware provenance") from error
    expected = {
        "project": PROJECT, "version": FIRMWARE_VERSION, "profile": profile,
        "target": target, "build_environment": environment,
        "source_commit": expected_commit,
    }
    for key, value in expected.items():
        if evidence.get(key) != value:
            raise StagingError(f"firmware provenance mismatch: {key}")
    firmware = build_repo / ".pio" / "build" / environment / "firmware.bin"
    if not firmware.is_file():
        raise StagingError("missing build component: firmware.bin")
    if evidence.get("firmware_sha256") != sha256_file(firmware):
        raise StagingError("stale or mismatched firmware binary")
    for name, field in (("firmware.elf", "elf_sha256"),
                        ("firmware.map", "link_map_sha256")):
        path = firmware.parent / name
        if not path.is_file() or evidence.get(field) != sha256_file(path):
            raise StagingError(f"stale or mismatched {name}")
    binary = firmware.read_bytes()
    identity_values = (PROJECT, FIRMWARE_VERSION, profile, target,
                       expected_commit)
    if any(value.encode("ascii") not in binary for value in identity_values):
        raise StagingError("embedded firmware identity mismatch")
    return evidence


def verify_partition_table(build_repo: Path, environment: str) -> dict:
    csv_path = Path.home() / ".platformio" / "packages" / \
        "framework-arduinoespressif32" / "tools" / "partitions" / "default.csv"
    try:
        rows = []
        for raw in csv_path.read_text(encoding="utf-8").splitlines():
            if not raw.strip() or raw.lstrip().startswith("#"):
                continue
            fields = [field.strip() for field in raw.split(",")]
            rows.append((fields[0], fields[3], fields[4]))
    except (OSError, IndexError) as error:
        raise StagingError("missing ESP32 partition metadata") from error
    numeric_rows = tuple((name, int(offset, 0), int(size, 0))
                         for name, offset, size in rows)
    numeric_expected = tuple((name, int(offset, 0), int(size, 0))
                             for name, offset, size in PARTITION_LAYOUT)
    if numeric_rows != numeric_expected:
        raise StagingError("unexpected ESP32 partition layout")
    table = build_repo / ".pio" / "build" / environment / "partitions.bin"
    return {
        "layout": [{"name": name, "offset": offset, "size": size}
                   for name, offset, size in PARTITION_LAYOUT],
        "nvs_range": {"offset": "0x9000", "size": "0x5000"},
        "sha256": sha256_file(table),
    }


@contextmanager
def controlled_checkout(repo: Path, commit: str, environment: str):
    parent = repo.parent
    temporary = Path(tempfile.mkdtemp(prefix=".wspr-release-build-", dir=parent))
    added = False
    try:
        subprocess.check_call(
            ["git", "worktree", "add", "--detach", str(temporary), commit],
            cwd=repo, stdout=subprocess.DEVNULL)
        added = True
        subprocess.check_call(["pio", "run", "-e", environment], cwd=temporary)
        yield temporary
    except (OSError, subprocess.CalledProcessError) as error:
        raise StagingError("controlled clean build failed") from error
    finally:
        if added:
            subprocess.run(["git", "worktree", "remove", "--force", str(temporary)],
                           cwd=repo, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, check=False)
        if temporary.exists():
            shutil.rmtree(temporary)


def ensure_destination_available(output: Path) -> None:
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        raise StagingError("destination exists and is not an empty directory")


def git_archive_entries(repo: Path, commit: str) -> list[tuple[str, int, bytes]]:
    try:
        listing = subprocess.check_output(
            ["git", "ls-tree", "-r", commit], cwd=repo, text=True)
    except (OSError, subprocess.CalledProcessError) as error:
        raise StagingError("cannot enumerate corresponding source") from error
    entries = []
    for line in listing.splitlines():
        metadata, name = line.split("\t", 1)
        mode, kind, _ = metadata.split()
        path = Path(name)
        if kind != "blob" or mode not in ("100644", "100755") or \
                path.is_absolute() or ".." in path.parts:
            raise StagingError("unsupported or unsafe source entry")
        try:
            content = subprocess.check_output(
                ["git", "show", f"{commit}:{name}"], cwd=repo)
        except (OSError, subprocess.CalledProcessError) as error:
            raise StagingError("cannot read corresponding source") from error
        entries.append((name, 0o755 if mode == "100755" else 0o644, content))
    if not entries:
        raise StagingError("empty corresponding source")
    return sorted(entries)


def write_source_archive(repo: Path, commit: str, destination: Path) -> None:
    prefix = f"wspr-beacon-{RELEASE_CANDIDATE}-source"
    entries = git_archive_entries(repo, commit)
    if any(name in LEGACY_SKETCHES for name, _, _ in entries):
        raise StagingError("legacy sketch present in corresponding source")
    with destination.open("wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.GNU_FORMAT) as archive:
                for name, mode, content in entries + [
                        ("SOURCE_COMMIT", 0o644, (commit + "\n").encode("ascii"))]:
                    info = tarfile.TarInfo(f"{prefix}/{name}")
                    info.size = len(content)
                    info.mode = mode
                    info.mtime = 0
                    info.uid = info.gid = 0
                    info.uname = info.gname = ""
                    archive.addfile(info, io.BytesIO(content))


def package_materials(target: str) -> tuple:
    if target == "ESP8266":
        return PACKAGE_MATERIALS
    return tuple(item for item in PACKAGE_MATERIALS if item[1] not in {
        "ESPRESSIF-NONOS-SDK.txt", "ESP8266_LICENSE_REVIEW.md",
        "RELEASE_NOTES.md", "THIRD_PARTY_NOTICES.md",
        "SOURCE_DISTRIBUTION.md"})


def copy_package_materials(repo: Path, output: Path,
                           source_commit: str, target: str = "ESP8266") -> list[dict[str, object]]:
    packaged = []
    for source_name, destination_name, role in package_materials(target):
        source = repo / source_name
        if not source.is_file():
            raise StagingError(f"missing package material: {source_name}")
        destination = output / destination_name
        shutil.copyfile(source, destination)
        packaged.append({"filename": destination_name, "role": role,
                         "sha256": sha256_file(destination),
                         "size_bytes": destination.stat().st_size})
    archive_name = f"wspr-beacon-{RELEASE_CANDIDATE}-source.tar.gz"
    archive = output / archive_name
    write_source_archive(repo, source_commit, archive)
    packaged.append({"filename": archive_name, "role": "corresponding-source",
                     "sha256": sha256_file(archive),
                     "size_bytes": archive.stat().st_size})
    return packaged


def write_package(build_repo: Path, environment: str, output: Path,
                  commit: str, dirty: bool, metadata: dict,
                  packages_dir: Path | None = None,
                  source_commit: str | None = None) -> str:
    profile, target = ENVIRONMENTS[environment]
    require_metadata(metadata)
    load_provenance(build_repo, environment, commit, profile, target)
    sources = component_sources(build_repo, environment, target, packages_dir)
    build = build_repo / ".pio" / "build" / environment
    elf_source = build / "firmware.elf"
    map_source = build / "firmware.map"
    if not elf_source.is_file() or not map_source.is_file():
        raise StagingError("missing firmware ELF or linker map")
    exact_commit = source_commit or commit
    stem = artifact_stem(profile, target, commit, dirty)
    components = []
    checksum_lines = []
    for source, component, offset in sources:
        filename = f"{stem}-{component}"
        destination = output / filename
        shutil.copyfile(source, destination)
        digest = sha256_file(destination)
        components.append({"filename": filename, "offset": offset,
                           "sha256": digest,
                           "size_bytes": destination.stat().st_size})
        checksum_lines.append(f"{digest}  {filename}")
    firmware = next(item for item in components
                    if item["filename"].endswith("-firmware.bin"))
    elf_name = f"{stem}-firmware.elf"
    map_name = f"{stem}-firmware.map"
    gzip_name = map_name + ".gz"
    package_elf(elf_source, output / elf_name, target, packages_dir)
    try:
        map_text = portable_map(map_source.read_text(encoding="utf-8"), build_repo)
    except (OSError, UnicodeError) as error:
        raise StagingError("cannot read linker map") from error
    if not map_text.startswith("Archive member included to satisfy reference"):
        raise StagingError("unsupported or incomplete linker map")
    (output / map_name).write_text(map_text, encoding="utf-8")
    with (output / gzip_name).open("wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as zipped:
            zipped.write(map_text.encode("utf-8"))
    technical = []
    for filename, role in ((elf_name, "firmware-elf"),
                           (map_name, "link-map"),
                           (gzip_name, "compressed-link-map")):
        path = output / filename
        technical.append({"filename": filename, "role": role,
                          "sha256": sha256_file(path),
                          "size_bytes": path.stat().st_size})
    archived_materials = [
        {"filename": item["filename"], "sha256": item["sha256"]}
        for item in components + technical
    ]
    inventory_name = f"{stem}-dependencies.json"
    inventory_path = output / inventory_name
    inventory = dependency_inventory(environment, target, profile, exact_commit,
                                     metadata, map_text, archived_materials)
    inventory_path.write_text(json.dumps(inventory, indent=2, sort_keys=True) + "\n",
                              encoding="utf-8")
    technical.append({"filename": inventory_name, "role": "dependency-inventory",
                      "sha256": sha256_file(inventory_path),
                      "size_bytes": inventory_path.stat().st_size})
    package_files = copy_package_materials(build_repo, output, exact_commit, target)
    package_files.extend(technical)
    for item in technical:
        checksum_lines.append(f"{item['sha256']}  {item['filename']}")
    for item in package_files[:-len(technical)]:
        checksum_lines.append(f"{item['sha256']}  {item['filename']}")
    manifest = {
        "board": TARGET_BOARDS[target],
        "build_environment": environment, "components": components,
        "direct_dependencies": DIRECT_DEPENDENCIES[target],
        "firmware_version": FIRMWARE_VERSION,
        "firmware_filename": firmware["filename"],
        "firmware_sha256": firmware["sha256"],
        "firmware_size_bytes": firmware["size_bytes"],
        "flash_parameters": FLASH_PARAMETERS[target],
        "elf_filename": elf_name, "elf_sha256": technical[0]["sha256"],
        "elf_processing": "strip-debug",
        "link_map_filename": map_name, "link_map_sha256": technical[1]["sha256"],
        "link_map_gzip_filename": gzip_name,
        "link_map_gzip_sha256": technical[2]["sha256"],
        "dependency_inventory_filename": inventory_name,
        "dependency_inventory_sha256": technical[3]["sha256"],
        "git_worktree": "dirty" if dirty else "clean",
        "legal_review_status": LEGAL_REVIEW_STATUS,
        "manifest_schema": MANIFEST_SCHEMA,
        "packaging_revision": PACKAGING_REVISION,
        "package_files": package_files,
        "platformio": metadata, "profile": profile, "project": PROJECT,
        "public_release_eligible": target == "ESP8266" and profile == "PUBLIC" and not dirty,
        "public_distribution_authorized": False,
        "source_commit": exact_commit,
        "firmware_source_commit": commit,
        "target": target,
        "technical_package_eligible": not dirty,
        "version": RELEASE_CANDIDATE,
    }
    if target == "ESP32":
        manifest["partition_table"] = verify_partition_table(build_repo, environment)
    manifest_name = f"{stem}-manifest.json"
    manifest_path = output / manifest_name
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                             encoding="utf-8")
    checksum_lines.append(f"{sha256_file(manifest_path)}  {manifest_name}")
    (output / "SHA256SUMS").write_text("\n".join(checksum_lines) + "\n",
                                       encoding="ascii")
    return manifest_name


def stage(repo: Path, environment: str, output: Path, public_release: bool = False) -> Path:
    if environment not in ENVIRONMENTS:
        raise StagingError(f"unsupported environment: {environment}")
    profile, target = ENVIRONMENTS[environment]
    commit, dirty = git_identity(repo)
    if public_release and profile != "PUBLIC":
        raise StagingError("DEVELOPMENT artifacts cannot enter public-release staging")
    if public_release and environment != "esp8266_public":
        raise StagingError("this release candidate is ESP8266-only")
    if public_release and (dirty or commit == "unknown"):
        raise StagingError("public-release staging requires a clean known commit")

    ensure_destination_available(output)
    temporary = Path(tempfile.mkdtemp(prefix=f".{output.name}-", dir=output.parent))
    try:
        if public_release:
            full_commit = run(repo, "git", "rev-parse", "HEAD")
            with controlled_checkout(repo, full_commit, environment) as build_repo:
                metadata = platformio_metadata(build_repo, environment)
                manifest_name = write_package(
                    build_repo, environment, temporary, commit, False, metadata,
                    source_commit=full_commit)
        else:
            metadata = platformio_metadata(repo, environment)
            manifest_name = write_package(
                repo, environment, temporary, commit, dirty, metadata)
        if output.exists():
            output.rmdir()
        os.replace(temporary, output)
        return output / manifest_name
    except Exception:
        shutil.rmtree(temporary, ignore_errors=True)
        raise


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--environment", required=True)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--public-release", action="store_true")
    arguments = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    try:
        manifest = stage(repo, arguments.environment,
                         arguments.output_dir.resolve(), arguments.public_release)
    except StagingError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    print(manifest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
