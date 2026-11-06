#!/usr/bin/env python3
"""Create a local ESP32 LGPL relink kit from a verified PUBLIC build."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import shlex
import tempfile

import verify_firmware_package


class RelinkKitError(RuntimeError):
    pass


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(65536), b""):
            value.update(block)
    return value.hexdigest()


def safe_output(path: Path) -> None:
    if path.is_symlink() or (path.exists() and
                             (not path.is_dir() or any(path.iterdir()))):
        raise RelinkKitError("output must be a new or empty directory")


def extract_commands(log: Path) -> tuple[str, str, str]:
    compile_core = link = image = None
    for line in log.read_text(encoding="utf-8").splitlines():
        if line.startswith("xtensa-esp32-elf-g++ -o ") and "firmware.elf" in line:
            link = line
        if line.startswith("xtensa-esp32-elf-gcc -o ") and \
                "FrameworkArduino/esp32-hal-misc.c.o" in line:
            compile_core = line
        if "esptool.py" in line and " elf2image " in line:
            image = line
    if not compile_core or not link or not image:
        raise RelinkKitError("verbose log lacks core compile, link or elf2image command")
    return compile_core, link, image


def normalized(command: str, build_dir: Path, framework: Path,
               toolchain: Path) -> str:
    command = command.replace(str(build_dir), "${KIT_ROOT}/build")
    command = command.replace(str(build_dir.parent.parent / "build" / "esp32_public"),
                              "${KIT_ROOT}/build")
    command = command.replace(".pio/build/esp32_public", "${KIT_ROOT}/build")
    command = command.replace(str(framework), "${FRAMEWORK_DIR}")
    command = command.replace(str(toolchain), "${TOOLCHAIN_DIR}")
    command = command.replace(str(framework.parent), "${PACKAGES_ROOT}")
    command = command.replace(str(build_dir.parents[2]), "${PROJECT_SOURCE}")
    command = re.sub(r'(?<![\w/])xtensa-esp32-elf-g\+\+',
                     '${TOOLCHAIN_DIR}/bin/xtensa-esp32-elf-g++', command)
    command = command.replace('"/usr/bin/python3"', '${PYTHON}')
    if "/home/" in command:
        raise RelinkKitError("unresolved private host path in command")
    return command


def copy_inputs(build: Path, destination: Path) -> list[dict]:
    inputs = sorted((build / "src").glob("*.o"))
    inputs += sorted(build.glob("lib*/*.a"))
    framework_archive = build / "libFrameworkArduino.a"
    if framework_archive.is_file():
        inputs.append(framework_archive)
    if not inputs or not framework_archive.is_file():
        raise RelinkKitError("build objects or Arduino framework archive missing")
    records = []
    for source in inputs:
        relative = source.relative_to(build)
        target = destination / "build" / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        records.append({"path": f"build/{relative.as_posix()}",
                        "sha256": digest(target), "size_bytes": target.stat().st_size})
    return records


def copy_lgpl_source(source_root: Path, destination: Path) -> list[dict]:
    root = destination / "lgpl-source"
    for relative in (Path("cores/esp32"), Path("variants/esp32"),
                     Path("libraries/Wire"), Path("libraries/SPI"),
                     Path("libraries/WiFi"), Path("libraries/Preferences")):
        source = source_root / relative
        if not source.is_dir():
            raise RelinkKitError(f"missing LGPL source directory: {relative}")
        shutil.copytree(source, root / relative)
    license_file = source_root / "LICENSE.md"
    if not license_file.is_file():
        raise RelinkKitError("Arduino framework LGPL license missing")
    shutil.copyfile(license_file, root / "LICENSE.md")
    return [{"path": path.relative_to(destination).as_posix(),
             "sha256": digest(path), "size_bytes": path.stat().st_size}
            for path in sorted(root.rglob("*")) if path.is_file()]


def copy_dependency_sources(source_root: Path, destination: Path) -> list[dict]:
    expected = {"Adafruit BusIO", "Adafruit GFX Library", "Adafruit SSD1306",
                "Etherkit JTEncode", "Etherkit Si5351"}
    present = {path.name for path in source_root.iterdir() if path.is_dir()}
    if present != expected:
        raise RelinkKitError("direct-library source set mismatch")
    target_root = destination / "corresponding-source" / "libraries"
    for name in sorted(expected):
        shutil.copytree(source_root / name, target_root / name,
                        ignore=shutil.ignore_patterns(".git", ".piopm",
                                                      "*.code-workspace"))
    return [{"path": path.relative_to(destination).as_posix(),
             "sha256": digest(path), "size_bytes": path.stat().st_size}
            for path in sorted(target_root.rglob("*")) if path.is_file()]


def validate_provenance(build: Path, staged: Path, source_archive: Path,
                        source_commit: str) -> dict:
    try:
        manifest = verify_firmware_package.verify_package(staged)
    except verify_firmware_package.VerificationError as error:
        raise RelinkKitError(f"invalid staged package: {error}") from error
    if manifest.get("target") != "ESP32" or manifest.get("profile") != "PUBLIC" or \
            manifest.get("build_environment") != "esp32_public" or \
            manifest.get("technical_package_eligible") is not True:
        raise RelinkKitError("staged package is not clean ESP32 PUBLIC evidence")
    firmware = next((item for item in manifest.get("components", [])
                     if item.get("offset") == "0x10000"), None)
    source = next((item for item in manifest.get("package_files", [])
                   if item.get("role") == "corresponding-source"), None)
    if firmware is None or source is None or \
            sha256_file_compatible(build / "firmware.bin") != firmware.get("sha256") or \
            sha256_file_compatible(source_archive) != source.get("sha256"):
        raise RelinkKitError("build or source does not correspond to staged package")
    if not re.fullmatch(r"[0-9a-f]{40}", source_commit) or \
            not source_commit.startswith(manifest["source_commit"]):
        raise RelinkKitError("full source commit does not match staged provenance")
    return {"source_commit": source_commit,
            "staged_firmware_sha256": firmware["sha256"]}


def sha256_file_compatible(path: Path) -> str:
    if not path.is_file():
        return ""
    return digest(path)


def create(build: Path, verbose_log: Path, output: Path, source_archive: Path,
           framework: Path, toolchain: Path, esptool: Path,
           lgpl_source_root: Path, staged_package: Path,
           source_commit: str, library_source_root: Path) -> Path:
    safe_output(output)
    if not all(path.is_file() for path in (verbose_log, source_archive, esptool)):
        raise RelinkKitError("required evidence or source archive missing")
    if not framework.is_dir() or not toolchain.is_dir():
        raise RelinkKitError("framework or toolchain directory missing")
    provenance = validate_provenance(build, staged_package, source_archive,
                                     source_commit)
    compile_core, link, image = extract_commands(verbose_log)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=f".{output.name}-", dir=output.parent))
    try:
        records = copy_inputs(build, temporary)
        lgpl_source = copy_lgpl_source(lgpl_source_root, temporary)
        dependency_source = copy_dependency_sources(library_source_root, temporary)
        shutil.copyfile(source_archive, temporary / source_archive.name)
        compile_core = normalized(compile_core, build, framework, toolchain)
        compile_core = compile_core.replace(
            "${FRAMEWORK_DIR}/cores/esp32", "${KIT_ROOT}/lgpl-source/cores/esp32")
        compile_core = compile_core.replace(
            "${FRAMEWORK_DIR}/variants/esp32", "${KIT_ROOT}/lgpl-source/variants/esp32")
        compile_core = re.sub(r'(?<![\w/])xtensa-esp32-elf-gcc',
                              '${TOOLCHAIN_DIR}/bin/xtensa-esp32-elf-gcc',
                              compile_core)
        link = normalized(link, build, framework, toolchain)
        image = normalized(image.replace(str(esptool), "${ESPTOOL}"),
                           build, framework, toolchain)
        commands = {
            "compile_modified_lgpl_object": compile_core,
            "replace_lgpl_archive_member":
                "${TOOLCHAIN_DIR}/bin/xtensa-esp32-elf-gcc-ar r "
                "${KIT_ROOT}/build/libFrameworkArduino.a "
                "${KIT_ROOT}/build/FrameworkArduino/esp32-hal-misc.c.o",
            "index_lgpl_archive":
                "${TOOLCHAIN_DIR}/bin/xtensa-esp32-elf-gcc-ranlib "
                "${KIT_ROOT}/build/libFrameworkArduino.a",
            "link": link, "elf2image": image,
        }
        (temporary / "commands.json").write_text(
            json.dumps(commands, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        dependencies = {
            "framework": {
                "name": "Arduino ESP32 2.0.17",
                "git_commit": "5e19e086c43d0fa5e5a596497ff8f11a0a43f6c2",
                "source_url": "https://github.com/espressif/arduino-esp32/archive/refs/tags/2.0.17.tar.gz",
                "source_sha256": "fb4024408494d6c45701dc8e1c595f84af543e66e06b00a21435a632d577fa61",
                "resolved_package": "framework-arduinoespressif32@4.20017.260907+sha.dcc1105b",
            },
            "toolchain": {
                "package": "espressif/toolchain-xtensa-esp32@8.4.0+2021r2-patch5",
                "platformio_registry_id": 12549,
            },
            "platform": "platformio/espressif32@7.1.3",
            "esptool": "4.11.0",
            "direct_libraries": {
                "Etherkit Si5351": "2.2.0",
                "Etherkit JTEncode": "1.3.1",
                "Adafruit GFX Library": "1.12.6",
                "Adafruit SSD1306": "2.5.17",
                "Adafruit BusIO": "1.17.4",
            },
        }
        manifest = {
            "schema": 1, "environment": "esp32_public",
            "provenance": provenance,
            "application_objects_must_remain_unchanged": True,
            "inputs": records,
            "lgpl_source_files": lgpl_source,
            "direct_library_source_files": dependency_source,
            "corresponding_source": {"filename": source_archive.name,
                                     "sha256": digest(temporary / source_archive.name)},
            "dependencies": dependencies,
            "public_distribution_authorized": False,
            "legal_review_status": "NOT_APPROVED_FOR_PUBLIC_DISTRIBUTION",
            "release_ready": False,
        }
        (temporary / "relink-manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        if any("/home/" in path.read_text(encoding="utf-8")
               for path in (temporary / "commands.json", temporary / "relink-manifest.json")):
            raise RelinkKitError("private path leaked into kit")
        if output.exists():
            output.rmdir()
        temporary.replace(output)
        return output / "relink-manifest.json"
    except Exception:
        shutil.rmtree(temporary, ignore_errors=True)
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--verbose-log", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--source-archive", required=True, type=Path)
    parser.add_argument("--framework-dir", required=True, type=Path)
    parser.add_argument("--toolchain-dir", required=True, type=Path)
    parser.add_argument("--esptool", required=True, type=Path)
    parser.add_argument("--lgpl-source-root", required=True, type=Path)
    parser.add_argument("--staged-package", required=True, type=Path)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--library-source-root", required=True, type=Path)
    args = parser.parse_args()
    try:
        paths = [getattr(args, name).resolve() for name in
                 ("build_dir", "verbose_log", "output_dir", "source_archive",
                  "framework_dir", "toolchain_dir", "esptool",
                  "lgpl_source_root", "staged_package", "library_source_root")]
        result = create(*paths[:-1], args.source_commit, paths[-1])
    except (OSError, RelinkKitError) as error:
        print(f"error: {error}")
        return 2
    print(result)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
