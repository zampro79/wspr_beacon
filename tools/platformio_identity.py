"""Inject source identity and attest the resulting firmware binary."""

import hashlib
import json
from pathlib import Path
import re
import subprocess

Import("env")  # type: ignore[name-defined]  # Provided by PlatformIO/SCons.


PROJECT = "WSPR Beacon 2.0"
VERSION = "dev"
ENVIRONMENTS = {
    "esp8266": ("DEVELOPMENT", "ESP8266"),
    "esp8266_public": ("PUBLIC", "ESP8266"),
    "esp32": ("DEVELOPMENT", "ESP32"),
    "esp32_public": ("PUBLIC", "ESP32"),
}


def source_commit():
    try:
        value = subprocess.check_output(
            ["git", "rev-parse", "--short=7", "HEAD"],
            cwd=env.subst("$PROJECT_DIR"),
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"
    return value if re.fullmatch(r"[0-9a-f]{7,40}", value) else "unknown"


commit = source_commit()
env.Append(CPPDEFINES=[("WSPR_SOURCE_COMMIT", '\\"%s\\"' % commit)])

# Keep host paths out of compiler-generated diagnostics and ELF/BIN strings.
if env.subst("$PIOENV") in ENVIRONMENTS:
    project_dir = Path(env.subst("$PROJECT_DIR")).resolve()
    package_dir = Path(env.PioPlatform().get_package_dir(
        "framework-arduinoespressif32" if env.subst("$PIOENV").startswith("esp32")
        else "framework-arduinoespressif8266")).resolve().parent
    env.Append(CCFLAGS=[f"-ffile-prefix-map={project_dir}=project",
                        f"-ffile-prefix-map={package_dir}=platformio/packages"])


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(65536), b""):
            digest.update(block)
    return digest.hexdigest()


def write_provenance(source, target, env):
    environment = env.subst("$PIOENV")
    if environment not in ENVIRONMENTS:
        return
    profile, platform_target = ENVIRONMENTS[environment]
    firmware = Path(env.subst("$BUILD_DIR")) / (
        env.subst("$PROGNAME") + ".bin"
    )
    if not firmware.is_file():
        raise RuntimeError("firmware binary missing for provenance generation")
    elf = firmware.with_suffix(".elf")
    link_map = firmware.with_suffix(".map")
    if not elf.is_file() or not link_map.is_file():
        raise RuntimeError("firmware ELF or linker map missing for provenance")
    evidence = {
        "build_environment": environment,
        "firmware_sha256": sha256_file(firmware),
        "elf_sha256": sha256_file(elf),
        "link_map_sha256": sha256_file(link_map),
        "profile": profile,
        "project": PROJECT,
        "source_commit": commit,
        "target": platform_target,
        "version": VERSION,
    }
    (firmware.parent / "firmware.provenance.json").write_text(
        json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", write_provenance)
