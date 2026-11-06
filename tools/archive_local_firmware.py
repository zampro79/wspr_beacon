#!/usr/bin/env python3
"""Build and retain verified local firmware packages, never public releases."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys

import stage_firmware
import verify_firmware_package


ENVIRONMENTS = tuple(stage_firmware.ENVIRONMENTS)


def archive_environment(repo: Path, environment: str, output_root: Path) -> Path:
    if environment not in ENVIRONMENTS:
        raise stage_firmware.StagingError(f"unsupported environment: {environment}")
    subprocess.run(["pio", "run", "-e", environment], cwd=repo, check=True)
    commit, dirty = stage_firmware.git_identity(repo)
    if commit == "unknown":
        raise stage_firmware.StagingError("local archiving requires a known Git commit")
    profile, target = stage_firmware.ENVIRONMENTS[environment]
    stage_firmware.load_provenance(repo, environment, commit, profile, target)
    firmware = repo / ".pio" / "build" / environment / "firmware.bin"
    digest = stage_firmware.sha256_file(firmware)
    stem = stage_firmware.artifact_stem(profile, target, commit, dirty)
    destination = output_root / environment / f"{stem}-{digest[:16]}"
    if destination.exists() or destination.is_symlink():
        manifest = verify_firmware_package.verify_package(destination)
        if manifest["firmware_sha256"] != digest or \
                manifest["build_environment"] != environment or \
                manifest["firmware_source_commit"] != commit or \
                manifest["public_distribution_authorized"] is not False:
            raise stage_firmware.StagingError("existing local package does not match build")
        return destination
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage_firmware.stage(repo, environment, destination)
    manifest = verify_firmware_package.verify_package(destination)
    if manifest["firmware_sha256"] != digest or \
            manifest["public_distribution_authorized"] is not False:
        raise stage_firmware.StagingError("local package verification mismatch")
    return destination


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment", action="append", choices=ENVIRONMENTS,
                        help="Archive one environment; repeat to select several (default: all four)")
    parser.add_argument("--output-root", type=Path,
                        help="Local archive root (default: repository/local_firmware)")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    requested_root = args.output_root or repo / "local_firmware"
    if requested_root.is_symlink():
        print("error: local archive root must be a directory", file=sys.stderr)
        return 2
    output_root = requested_root.resolve()
    if output_root.exists() and not output_root.is_dir():
        print("error: local archive root must be a directory", file=sys.stderr)
        return 2
    for environment in args.environment or ENVIRONMENTS:
        try:
            destination = archive_environment(repo, environment, output_root)
        except (stage_firmware.StagingError, verify_firmware_package.VerificationError,
                OSError, subprocess.CalledProcessError) as error:
            print(f"error: {environment}: {error}", file=sys.stderr)
            return 2
        manifest = json.loads(next(destination.glob("*-manifest.json")).read_text())
        print(f"{environment}: {destination / manifest['firmware_filename']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
