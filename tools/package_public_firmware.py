#!/usr/bin/env python3
"""Prepare a local, non-distributable ESP32 PUBLIC compliance package."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

import verify_firmware_package


STATUS = "COMPLIANCE_INCOMPLETE"
FACTORY_NAME = "wspr-beacon-esp32-public-factory.bin"
EXPECTED_OFFSETS = {
    "bootloader.bin": 0x1000,
    "partitions.bin": 0x8000,
    "boot_app0.bin": 0xE000,
    "firmware.bin": 0x10000,
}
FLASH_SIZE = 4 * 1024 * 1024
NVS_START = 0x9000
NVS_END = 0xE000
SENSITIVE = re.compile(
    rb"(?i)(?:password\s*[:=]|api[_-]?key\s*[:=]|secret\s*[:=]|"
    rb"token\s*[:=]|/home/|/Users/|[A-Z]:\\Users\\)"
)

# Licenses are component-specific. Unknown or mixed archives deliberately keep
# the package incomplete instead of inheriting the top-level ESP-IDF license.
ARCHIVE_LICENSES = {
    "libfreertos.a": ("MIT", "ESP-IDF FreeRTOS component"),
    "liblwip.a": ("BSD-3-Clause", "ESP-IDF lwIP component"),
    "libmbedcrypto.a": ("Apache-2.0", "ESP-IDF mbedTLS component"),
    "libmbedtls.a": ("Apache-2.0", "ESP-IDF mbedTLS component"),
    "libmbedtls_2.a": ("Apache-2.0", "ESP-IDF mbedTLS component"),
    "libmbedx509.a": ("Apache-2.0", "ESP-IDF mbedTLS component"),
    "libnewlib.a": ("BSD-family", "newlib; exact notices must be retained"),
    "libnvs_flash.a": ("Apache-2.0", "ESP-IDF NVS component"),
}
UNRESOLVED_ARCHIVES = {
    "libbt.a", "libcoexist.a", "libesp_phy.a", "libesp_wifi.a",
    "libnet80211.a", "libpp.a", "libsmartconfig.a",
}
ARDUINO_FRAMEWORK_ARCHIVES = {
    "libPreferences.a", "libSPI.a", "libWiFi.a", "libWire.a",
}
ADAFRUIT_ARCHIVES = {
    "libAdafruit GFX Library.a": ("BSD-2-Clause", "Adafruit GFX Library"),
    "libAdafruit SSD1306.a": ("BSD-3-Clause", "Adafruit SSD1306"),
    "libAdafruit BusIO.a": ("MIT", "Adafruit BusIO"),
}


class PackagingError(RuntimeError):
    pass


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def safe_output(output: Path) -> None:
    if output.is_symlink() or (output.exists() and
                               (not output.is_dir() or any(output.iterdir()))):
        raise PackagingError("output must be a new or empty directory")


def copy_required(source: Path, destination: Path) -> None:
    if source.is_symlink() or not source.is_file():
        raise PackagingError(f"missing or unsafe package material: {source.name}")
    shutil.copyfile(source, destination)


def full_source_commit(repo: Path, declared: str) -> str:
    if re.fullmatch(r"[0-9a-f]{40}", declared):
        return declared
    if not re.fullmatch(r"[0-9a-f]{7,39}", declared):
        raise PackagingError("invalid source commit")
    try:
        resolved = subprocess.check_output(
            ["git", "rev-parse", "--verify", f"{declared}^{{commit}}"],
            cwd=repo, stderr=subprocess.DEVNULL, text=True).strip()
    except (OSError, subprocess.CalledProcessError) as error:
        raise PackagingError("cannot resolve full source commit") from error
    if not re.fullmatch(r"[0-9a-f]{40}", resolved) or \
            not resolved.startswith(declared):
        raise PackagingError("invalid resolved source commit")
    return resolved


def validate_base_package(package: Path) -> dict:
    try:
        manifest = verify_firmware_package.verify_package(package)
    except verify_firmware_package.VerificationError as error:
        raise PackagingError(f"invalid staged package: {error}") from error
    expected = {
        "target": "ESP32", "profile": "PUBLIC",
        "build_environment": "esp32_public",
        "public_distribution_authorized": False,
        "legal_review_status": "NOT_APPROVED_FOR_PUBLIC_DISTRIBUTION",
    }
    for field, value in expected.items():
        if manifest.get(field) != value:
            raise PackagingError(f"staged package mismatch: {field}")
    if not manifest.get("platformio", {}).get("resolved_packages"):
        raise PackagingError("missing resolved provenance")
    clean = manifest.get("git_worktree") == "clean"
    if manifest.get("git_worktree") not in ("clean", "dirty") or \
            manifest.get("technical_package_eligible") is not clean:
        raise PackagingError("invalid technical-package eligibility")
    offsets = {}
    files = {}
    for item in manifest.get("components", []):
        kind = verify_firmware_package.component_kind(item.get("filename", ""))
        offsets[kind] = int(item.get("offset", "-1"), 0)
        files[kind] = package / item["filename"]
    if offsets != EXPECTED_OFFSETS:
        raise PackagingError("unexpected ESP32 component offsets")
    manifest["_component_files"] = files
    return manifest


def validate_bootloader(path: Path) -> None:
    header = path.read_bytes()[:4]
    # ESP32 image: E9, segment count, flash mode, flash size/frequency.
    # 02 is DIO and 20 encodes 4 MB / 40 MHz for classic ESP32.
    if len(header) != 4 or header[0] != 0xE9 or header[2:] != b"\x02\x20":
        raise PackagingError("bootloader does not encode DIO/40 MHz/4 MB")


def default_esptool() -> Path:
    packages = Path(os.environ.get(
        "PLATFORMIO_PACKAGES_DIR", Path.home() / ".platformio" / "packages"))
    return packages / "tool-esptoolpy" / "esptool.py"


def create_factory(files: dict[str, Path], output: Path, esptool: Path) -> dict:
    validate_bootloader(files["bootloader.bin"])
    if not esptool.is_file():
        raise PackagingError("esptool.py not found")
    command = [
        sys.executable, str(esptool), "--chip", "esp32", "merge_bin",
        "-o", str(output),
    ]
    for kind, offset in EXPECTED_OFFSETS.items():
        command.extend((hex(offset), str(files[kind])))
    try:
        completed = subprocess.run(command, check=True, capture_output=True,
                                   text=True)
    except (OSError, subprocess.CalledProcessError) as error:
        raise PackagingError("esptool merge_bin failed") from error
    data = output.read_bytes()
    if not data or len(data) > FLASH_SIZE:
        raise PackagingError("factory image has invalid size")
    for kind, offset in EXPECTED_OFFSETS.items():
        component = files[kind].read_bytes()
        if data[offset:offset + len(component)] != component:
            raise PackagingError(f"factory image component mismatch: {kind}")
    if any(byte != 0xFF for byte in data[NVS_START:NVS_END]):
        raise PackagingError("factory image NVS gap is not erased padding")
    return {
        "filename": FACTORY_NAME,
        "flash_offset": "0x0",
        "sha256": sha256_file(output),
        "size_bytes": len(data),
        "nvs_effect": "DESTRUCTIVE_FACTORY_PATH",
        "tool_output": completed.stdout.strip().splitlines()[-1:],
    }


def archive_name(path: str) -> str:
    value = path.replace("\\", "/").rsplit("/", 1)[-1]
    return value if value.endswith(".a") else path


def build_sbom(package: Path, manifest: dict) -> dict:
    inventory_path = package / manifest["dependency_inventory_filename"]
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    components = []
    unresolved = set()
    for item in inventory.get("linked_archive_members", []):
        name = archive_name(item.get("archive", ""))
        if not name:
            continue
        if name in ARCHIVE_LICENSES:
            license_id, provenance = ARCHIVE_LICENSES[name]
            status = "VERIFIED_COMPONENT_CLASS"
        elif name in UNRESOLVED_ARCHIVES:
            license_id, provenance = "NOASSERTION", "exact binary-blob terms pending"
            status = "UNRESOLVED"
            unresolved.add(name)
        elif name.startswith("libFrameworkArduino") or name in ARDUINO_FRAMEWORK_ARCHIVES:
            license_id, provenance = "LGPL-2.1-or-later", "Arduino ESP32 core"
            status = "RELINK_REQUIRED"
        elif name.startswith("libEtherkit"):
            license_id, provenance = "GPL-3.0-or-later", "pinned application library"
            status = "VERIFIED_DIRECT_DEPENDENCY"
        elif name in ADAFRUIT_ARCHIVES:
            license_id, provenance = ADAFRUIT_ARCHIVES[name]
            status = "NOTICE_REQUIRED"
        elif ".pio/build/" in item.get("archive", ""):
            license_id, provenance = "NOASSERTION", "unclassified build archive"
            status = "UNRESOLVED"
            unresolved.add(name)
        else:
            license_id, provenance = "NOASSERTION", "component-specific review pending"
            status = "UNRESOLVED"
            unresolved.add(name)
        components.append({
            "name": name, "archive": item["archive"], "license": license_id,
            "provenance": provenance, "inclusion_evidence": item.get("evidence"),
            "status": status,
        })
    return {
        "schema": 1,
        "status": STATUS if unresolved else "REVIEW_REQUIRED",
        "source_commit": manifest["source_commit"],
        "framework": next((item for item in manifest["platformio"]["resolved_packages"]
                           if item.get("name") == "framework-arduinoespressif32"), None),
        "components": sorted(components, key=lambda item: (item["name"], item["archive"])),
        "unresolved_incorporated_archives": sorted(unresolved),
        "completeness_claim": False,
    }


def scan_output(root: Path) -> None:
    for path in root.rglob("*"):
        if path.is_symlink():
            raise PackagingError("symlinks are not allowed in output")
        if not path.is_file() or path.name.endswith((".bin", ".elf", ".gz")):
            continue
        if SENSITIVE.search(path.read_bytes()):
            raise PackagingError(f"sensitive or local data in output: {path.name}")


def checksums(root: Path) -> None:
    entries = []
    for path in sorted(root.rglob("*")):
        if path.is_file() and path.name != "SHA256SUMS":
            entries.append(f"{sha256_file(path)}  {path.relative_to(root).as_posix()}")
    (root / "SHA256SUMS").write_text("\n".join(entries) + "\n", encoding="ascii")


def package(staged: Path, output: Path, repo: Path,
            esptool: Path | None = None) -> Path:
    safe_output(output)
    manifest = validate_base_package(staged)
    manifest["source_commit"] = full_source_commit(repo, manifest["source_commit"])
    parent = output.parent
    parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=f".{output.name}-", dir=parent))
    try:
        firmware_dir = temporary / "firmware"
        evidence_dir = temporary / "evidence"
        compliance_dir = temporary / "compliance"
        docs_dir = temporary / "docs"
        source_dir = temporary / "source"
        for directory in (firmware_dir, evidence_dir, compliance_dir, docs_dir,
                          source_dir):
            directory.mkdir()
        for kind, source in manifest.pop("_component_files").items():
            copy_required(source, firmware_dir / kind)
        factory = create_factory(
            {kind: firmware_dir / kind for kind in EXPECTED_OFFSETS},
            firmware_dir / FACTORY_NAME, esptool or default_esptool())
        for field in ("dependency_inventory_filename", "link_map_filename",
                      "elf_filename"):
            copy_required(staged / manifest[field],
                          evidence_dir / Path(manifest[field]).name)
        source_item = next((item for item in manifest.get("package_files", [])
                            if item.get("role") == "corresponding-source"), None)
        if source_item is None:
            raise PackagingError("missing corresponding source archive")
        source_archive = staged / source_item["filename"]
        copy_required(source_archive, source_dir / source_archive.name)
        compliance_materials = {
            "LICENSE": repo / "LICENSE",
            "COPYRIGHT.md": repo / "COPYRIGHT.md",
            "THIRD_PARTY_NOTICES.md": repo / "THIRD_PARTY_NOTICES.md",
            "GPL-3.0.txt": repo / "licenses/GPL-3.0.txt",
            "LGPL-2.1.txt": repo / "licenses/LGPL-2.1.txt",
            "GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt":
                repo / "licenses/GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt",
            "BSD-2-Clause-Adafruit-GFX.txt":
                repo / "licenses/BSD-2-Clause-Adafruit-GFX.txt",
            "BSD-3-Clause-Adafruit-SSD1306.txt":
                repo / "licenses/BSD-3-Clause-Adafruit-SSD1306.txt",
            "MIT-Adafruit-BusIO.txt": repo / "licenses/MIT-Adafruit-BusIO.txt",
        }
        for name, source in compliance_materials.items():
            copy_required(source, compliance_dir / name)
        for name in ("FLASHING.md", "BUILDING.md", "USER_GUIDE.md", "HARDWARE.md",
                     "LGPL_RELINKING.md", "ESP32_COMPONENT_INVENTORY.md"):
            source = repo / "docs" / name
            copy_required(source, docs_dir / name)
        sbom = build_sbom(staged, manifest)
        (compliance_dir / "esp32-sbom.json").write_text(
            json.dumps(sbom, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        provenance = {
            "schema": 1, "project": manifest["project"],
            "source_commit": manifest["source_commit"],
            "firmware_source_commit": manifest["firmware_source_commit"],
            "input_git_worktree": manifest["git_worktree"],
            "technical_package_eligible":
                manifest["technical_package_eligible"],
            "environment": "esp32_public", "profile": "PUBLIC", "target": "ESP32",
            "board": manifest["board"], "platformio": manifest["platformio"],
            "flash_parameters": manifest["flash_parameters"],
            "partition_table": manifest["partition_table"],
            "components": [
                {
                    "filename": kind,
                    "offset": hex(EXPECTED_OFFSETS[kind]),
                    "sha256": sha256_file(firmware_dir / kind),
                    "size_bytes": (firmware_dir / kind).stat().st_size,
                }
                for kind in EXPECTED_OFFSETS
            ],
            "factory_image": factory,
            "corresponding_source": {
                "filename": source_archive.name,
                "sha256": sha256_file(source_dir / source_archive.name),
                "correspondence_verified":
                    manifest["technical_package_eligible"],
            },
            "compliance_status": STATUS,
            "release_ready": False,
            "public_distribution_authorized": False,
            "legal_review_status": "NOT_APPROVED_FOR_PUBLIC_DISTRIBUTION",
            "lgpl_relink_kit": "FEASIBILITY_EVIDENCE_ONLY_NOT_AUTONOMOUS",
        }
        (temporary / "preparation-manifest.json").write_text(
            json.dumps(provenance, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        scan_output(temporary)
        checksums(temporary)
        if output.exists():
            output.rmdir()
        os.replace(temporary, output)
        return output / "preparation-manifest.json"
    except Exception:
        shutil.rmtree(temporary, ignore_errors=True)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--staged-package", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--esptool", type=Path)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    try:
        result = package(args.staged_package.resolve(), args.output_dir.resolve(),
                         repo, args.esptool.resolve() if args.esptool else None)
    except (PackagingError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    print(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
