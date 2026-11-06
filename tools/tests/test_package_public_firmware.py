import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


MODULE_PATH = Path(__file__).resolve().parents[1] / "package_public_firmware.py"
sys.path.insert(0, str(MODULE_PATH.parent))
SPEC = importlib.util.spec_from_file_location("package_public_firmware", MODULE_PATH)
package_public = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(package_public)


class PublicEsp32PackagingTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.staged = self.root / "staged"
        self.staged.mkdir()
        self.repo = self.root / "repo"
        (self.repo / "docs").mkdir(parents=True)
        for name in ("FLASHING.md", "BUILDING.md", "USER_GUIDE.md", "HARDWARE.md",
                     "LGPL_RELINKING.md", "ESP32_COMPONENT_INVENTORY.md"):
            (self.repo / "docs" / name).write_text(f"# {name}\n", encoding="utf-8")
        compliance = {
            "LICENSE", "COPYRIGHT.md", "THIRD_PARTY_NOTICES.md",
            "licenses/GPL-3.0.txt", "licenses/LGPL-2.1.txt",
            "licenses/GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt",
            "licenses/BSD-2-Clause-Adafruit-GFX.txt",
            "licenses/BSD-3-Clause-Adafruit-SSD1306.txt",
            "licenses/MIT-Adafruit-BusIO.txt",
        }
        for name in compliance:
            path = self.repo / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture\n", encoding="utf-8")
        self.components = {}
        for kind, offset in package_public.EXPECTED_OFFSETS.items():
            data = b"\xe9\x03\x02\x20" + b"B" * 60 if kind == "bootloader.bin" \
                else (kind.encode() * 8)
            filename = f"fixture-{kind}"
            (self.staged / filename).write_bytes(data)
            self.components[kind] = {
                "filename": filename, "offset": hex(offset),
                "sha256": hashlib.sha256(data).hexdigest(), "size_bytes": len(data),
            }
        for name in ("fixture-dependencies.json", "fixture-firmware.map",
                     "fixture-firmware.elf"):
            (self.staged / name).write_bytes(b"evidence\n")
        (self.staged / "fixture-source.tar.gz").write_bytes(b"source\n")
        inventory = {
            "linked_archive_members": [
                {"archive": "platformio/packages/framework/tools/sdk/esp32/lib/liblwip.a",
                 "evidence": "link-map-archive-member"},
                {"archive": "platformio/packages/framework/tools/sdk/esp32/lib/libnet80211.a",
                 "evidence": "link-map-archive-member"},
                {"archive": ".pio/build/esp32_public/libFrameworkArduino.a",
                 "evidence": "link-map-archive-member"},
                {"archive": ".pio/build/esp32_public/libUnknown.a",
                 "evidence": "link-map-archive-member"},
            ]
        }
        (self.staged / "fixture-dependencies.json").write_text(json.dumps(inventory))
        for name in ("LICENSE", "COPYRIGHT.md", "GPL-3.0.txt", "LGPL-2.1.txt",
                     "GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt"):
            (self.staged / name).write_text("fixture\n")
        self.manifest = {
            "project": "WSPR Beacon 2.0", "target": "ESP32", "profile": "PUBLIC",
            "build_environment": "esp32_public", "source_commit": "a" * 40,
            "firmware_source_commit": "a" * 7, "board": "esp32dev",
            "public_distribution_authorized": False,
            "legal_review_status": "NOT_APPROVED_FOR_PUBLIC_DISTRIBUTION",
            "git_worktree": "clean", "technical_package_eligible": True,
            "components": list(self.components.values()),
            "package_files": [{"filename": "fixture-source.tar.gz",
                               "role": "corresponding-source"}],
            "dependency_inventory_filename": "fixture-dependencies.json",
            "link_map_filename": "fixture-firmware.map",
            "elf_filename": "fixture-firmware.elf",
            "flash_parameters": {"mode": "dio", "frequency": "40m",
                                 "size": "4MB", "recommended_baud": 460800},
            "partition_table": {
                "layout": [], "nvs_range": {"offset": "0x9000", "size": "0x5000"}},
            "platformio": {
                "core_version": "6.1.19",
                "resolved_packages": [
                    {"name": "Platform espressif32", "version": "7.1.3"},
                    {"name": "framework-arduinoespressif32",
                     "version": "4.20017.260907+sha.dcc1105b"},
                    {"name": "toolchain-xtensa-esp32", "version": "8.4.0"},
                ],
            },
        }
        self.esptool = self.root / "esptool.py"
        self.esptool.write_text("# fixture\n")

    def tearDown(self):
        self.temporary.cleanup()

    def fake_merge(self, command, **kwargs):
        output = Path(command[command.index("-o") + 1])
        data = bytearray(b"\xff" * 0x10000)
        index = command.index(str(output)) + 1
        while index < len(command):
            offset = int(command[index], 0)
            content = Path(command[index + 1]).read_bytes()
            needed = offset + len(content)
            if needed > len(data):
                data.extend(b"\xff" * (needed - len(data)))
            data[offset:needed] = content
            index += 2
        output.write_bytes(data)
        return subprocess.CompletedProcess(command, 0, "merged\n", "")

    def run_package(self, mutate=None):
        manifest = dict(self.manifest)
        manifest["components"] = [dict(item) for item in self.manifest["components"]]
        if mutate:
            mutate(manifest)
        output = self.root / "output"
        with mock.patch.object(package_public.verify_firmware_package,
                               "verify_package", return_value=manifest), \
             mock.patch.object(package_public.subprocess, "run",
                               side_effect=self.fake_merge):
            path = package_public.package(self.staged, output, self.repo, self.esptool)
        return output, json.loads(path.read_text())

    def test_public_package_is_factory_checked_and_fail_closed(self):
        output, manifest = self.run_package()
        self.assertEqual(manifest["compliance_status"], "COMPLIANCE_INCOMPLETE")
        self.assertFalse(manifest["release_ready"])
        self.assertFalse(manifest["public_distribution_authorized"])
        self.assertTrue(manifest["technical_package_eligible"])
        self.assertTrue(
            manifest["corresponding_source"]["correspondence_verified"])
        self.assertEqual(manifest["factory_image"]["flash_offset"], "0x0")
        self.assertEqual(manifest["factory_image"]["nvs_effect"],
                         "DESTRUCTIVE_FACTORY_PATH")
        self.assertTrue((output / "firmware" /
                         package_public.FACTORY_NAME).is_file())
        self.assertTrue((output / "source" / "fixture-source.tar.gz").is_file())
        sbom = json.loads((output / "compliance" / "esp32-sbom.json").read_text())
        self.assertNotIn("libnet80211.a", sbom["unresolved_incorporated_archives"])
        self.assertIn("libUnknown.a", sbom["unresolved_incorporated_archives"])
        self.assertFalse(sbom["completeness_claim"])
        wifi = next(item for item in sbom["components"]
                    if item["name"] == "libnet80211.a")
        self.assertEqual(wifi["license"], "Apache-2.0")
        self.assertEqual(wifi["upstream_version"], package_public.ESP32_ARCHIVE_LICENSES[
            "libnet80211.a"]["upstream_version"])
        self.assertTrue((output / "SHA256SUMS").is_file())

    def test_development_unknown_target_and_missing_provenance_are_rejected(self):
        for field, value in (("profile", "DEVELOPMENT"), ("target", "OTHER")):
            with self.subTest(field=field):
                with mock.patch.object(package_public.verify_firmware_package,
                                       "verify_package",
                                       return_value=dict(self.manifest, **{field: value})):
                    with self.assertRaises(package_public.PackagingError):
                        package_public.package(self.staged, self.root / field,
                                               self.repo, self.esptool)
        broken = dict(self.manifest)
        broken["platformio"] = {"core_version": "6.1.19", "resolved_packages": []}
        with mock.patch.object(package_public.verify_firmware_package,
                               "verify_package", return_value=broken):
            with self.assertRaises(package_public.PackagingError):
                package_public.package(self.staged, self.root / "provenance",
                                       self.repo, self.esptool)

        ineligible = dict(self.manifest, git_worktree="clean",
                          technical_package_eligible=False)
        with mock.patch.object(package_public.verify_firmware_package,
                               "verify_package", return_value=ineligible):
            with self.assertRaises(package_public.PackagingError):
                package_public.package(self.staged, self.root / "ineligible",
                                       self.repo, self.esptool)

    def test_dirty_input_is_explicitly_not_correspondence_verified(self):
        self.manifest["git_worktree"] = "dirty"
        self.manifest["technical_package_eligible"] = False
        _, manifest = self.run_package()
        self.assertEqual(manifest["input_git_worktree"], "dirty")
        self.assertFalse(manifest["technical_package_eligible"])
        self.assertFalse(
            manifest["corresponding_source"]["correspondence_verified"])

    def test_wrong_offset_missing_image_and_partition_rejection_propagate(self):
        with mock.patch.object(package_public.verify_firmware_package,
                               "verify_package", return_value=self.manifest):
            wrong = dict(self.manifest)
            wrong["components"] = [dict(item) for item in self.manifest["components"]]
            wrong["components"][0]["offset"] = "0x0"
            with mock.patch.object(package_public.verify_firmware_package,
                                   "verify_package", return_value=wrong):
                with self.assertRaises(package_public.PackagingError):
                    package_public.package(self.staged, self.root / "offset",
                                           self.repo, self.esptool)
        missing = self.staged / self.components["firmware.bin"]["filename"]
        missing.unlink()
        with mock.patch.object(package_public.verify_firmware_package,
                               "verify_package",
                               side_effect=package_public.verify_firmware_package.VerificationError(
                                   "partition or component mismatch")):
            with self.assertRaises(package_public.PackagingError):
                package_public.package(self.staged, self.root / "missing",
                                       self.repo, self.esptool)

    def test_checksum_integrity_and_no_private_paths(self):
        output, _ = self.run_package()
        lines = (output / "SHA256SUMS").read_text().splitlines()
        self.assertGreater(len(lines), 8)
        for line in lines:
            digest, name = line.split("  ", 1)
            self.assertEqual(digest, package_public.sha256_file(output / name))
        combined = b"\n".join(path.read_bytes() for path in output.rglob("*")
                              if path.is_file() and path.suffix not in (".bin", ".elf"))
        self.assertNotIn(b"/home/", combined)
        self.assertNotIn(b"password=", combined.lower())

    def test_nonempty_destination_and_bad_boot_header_fail(self):
        occupied = self.root / "occupied"
        occupied.mkdir()
        (occupied / "keep").write_text("x")
        with self.assertRaises(package_public.PackagingError):
            package_public.safe_output(occupied)
        output_link = self.root / "output-link"
        output_link.symlink_to(occupied, target_is_directory=True)
        with self.assertRaises(package_public.PackagingError):
            package_public.safe_output(output_link)
        boot = self.staged / self.components["bootloader.bin"]["filename"]
        boot.write_bytes(b"bad image")
        with mock.patch.object(package_public.verify_firmware_package,
                               "verify_package", return_value=self.manifest), \
             mock.patch.object(package_public.subprocess, "run",
                               side_effect=self.fake_merge):
            with self.assertRaises(package_public.PackagingError):
                package_public.package(self.staged, self.root / "badboot",
                                       self.repo, self.esptool)

    def test_relink_kit_missing_and_tampered_material_are_rejected(self):
        kit = self.root / "kit"
        kit.mkdir()
        item = kit / "build/src/main.o"
        item.parent.mkdir(parents=True)
        item.write_bytes(b"object")
        lgpl = kit / "lgpl-source/core.c"
        lgpl.parent.mkdir()
        lgpl.write_text("source\n")
        direct = kit / "corresponding-source/library.cpp"
        direct.parent.mkdir()
        direct.write_text("source\n")
        source_archive = kit / "source.tar.gz"
        source_archive.write_bytes(b"source")
        commands = {name: "${KIT_ROOT}/tool" for name in (
            "compile_modified_lgpl_object", "replace_lgpl_archive_member",
            "index_lgpl_archive", "link", "elf2image")}
        (kit / "commands.json").write_text(json.dumps(commands))
        manifest = {
            "environment": "esp32_public",
            "application_objects_must_remain_unchanged": True,
            "public_distribution_authorized": False,
            "release_ready": False,
            "provenance": {"source_commit": "a" * 40,
                           "staged_firmware_sha256": "b" * 64},
            "inputs": [{"path": "build/src/main.o",
                        "sha256": package_public.sha256_file(item)}],
            "lgpl_source_files": [{"path": "lgpl-source/core.c",
                                    "sha256": package_public.sha256_file(lgpl)}],
            "direct_library_source_files": [
                {"path": "corresponding-source/library.cpp",
                 "sha256": package_public.sha256_file(direct)}],
            "corresponding_source": {
                "filename": "source.tar.gz",
                "sha256": package_public.sha256_file(source_archive)},
            "dependencies": {
                "framework": {"name": "fixture"},
                "toolchain": {"package": "fixture"},
                "platform": "fixture", "esptool": "fixture",
                "direct_libraries": {"fixture": "1.0"}},
        }
        (kit / "relink-manifest.json").write_text(json.dumps(manifest))
        package_public.validate_relink_kit(kit)
        item.write_bytes(b"tampered")
        with self.assertRaises(package_public.PackagingError):
            package_public.validate_relink_kit(kit)
        item.unlink()
        with self.assertRaises(package_public.PackagingError):
            package_public.validate_relink_kit(kit)

    def test_relink_kit_rejects_missing_sources_and_provenance(self):
        kit = self.root / "incomplete-kit"
        kit.mkdir()
        commands = {name: "${KIT_ROOT}/tool" for name in (
            "compile_modified_lgpl_object", "replace_lgpl_archive_member",
            "index_lgpl_archive", "link", "elf2image")}
        (kit / "commands.json").write_text(json.dumps(commands))
        manifest = {
            "environment": "esp32_public",
            "application_objects_must_remain_unchanged": True,
            "public_distribution_authorized": False, "release_ready": False,
            "provenance": {"source_commit": "a" * 40,
                           "staged_firmware_sha256": "b" * 64},
            "inputs": [], "lgpl_source_files": [],
            "direct_library_source_files": [],
        }
        (kit / "relink-manifest.json").write_text(json.dumps(manifest))
        with self.assertRaises(package_public.PackagingError):
            package_public.validate_relink_kit(kit)

    def test_material_paths_reject_traversal_absolute_and_symlinked_parent(self):
        root = self.root / "materials"
        root.mkdir()
        outside = self.root / "outside"
        outside.mkdir()
        (outside / "item").write_text("outside\n")
        for relative in ("../outside/item", str(outside / "item"),
                         "texts\\item"):
            with self.subTest(relative=relative), \
                    self.assertRaises(package_public.PackagingError):
                package_public.safe_material_path(root, relative)
        (root / "linked").symlink_to(outside, target_is_directory=True)
        with self.assertRaises(package_public.PackagingError):
            package_public.safe_material_path(root, "linked/item")

    def test_license_bundle_requires_exact_component_coverage_and_hashes(self):
        bundle = self.root / "licenses"
        (bundle / "texts").mkdir(parents=True)
        text_path = bundle / "texts/Apache-2.0.txt"
        text_path.write_text("license\n")
        sbom = {"source_commit": "a" * 40,
                "components": [{"name": "libexample.a",
                                "archive": "path/libexample.a",
                                "license": "Apache-2.0",
                                "inclusion_evidence": "map"}]}
        component = {
            "name": "libexample.a", "archive": "path/libexample.a",
            "license": "Apache-2.0",
            "inclusion_evidence": "map", "copyright_notice": "Example",
            "package": "Example package", "version": "1.0",
            "license_source": "upstream LICENSE",
            "redistribution_conditions": "Retain notice",
            "license_texts": [{"path": "texts/Apache-2.0.txt",
                               "sha256": package_public.sha256_file(text_path)}],
        }
        index = {"schema": 1, "complete": True,
                 "legal_clearance_claim": False, "source_commit": "a" * 40,
                 "archive_count": 1, "components": [component]}
        (bundle / "license-bundle.json").write_text(json.dumps(index))
        package_public.validate_license_bundle(bundle, sbom)
        text_path.write_text("tampered\n")
        with self.assertRaises(package_public.PackagingError):
            package_public.validate_license_bundle(bundle, sbom)
        component["license_texts"] = []
        (bundle / "license-bundle.json").write_text(json.dumps(index))
        with self.assertRaises(package_public.PackagingError):
            package_public.validate_license_bundle(bundle, sbom)

    def test_license_bundle_rejects_wrong_commit_and_extra_files(self):
        bundle = self.root / "licenses-extra"
        (bundle / "texts").mkdir(parents=True)
        text_path = bundle / "texts/Apache-2.0.txt"
        text_path.write_text("license\n")
        sbom = {"source_commit": "a" * 40,
                "components": [{"name": "libexample.a",
                                "archive": "path/libexample.a",
                                "license": "Apache-2.0",
                                "inclusion_evidence": "map"}]}
        component = {
            "name": "libexample.a", "archive": "path/libexample.a",
            "license": "Apache-2.0", "inclusion_evidence": "map",
            "package": "Example", "version": "1.0",
            "license_source": "LICENSE", "copyright_notice": "Example",
            "redistribution_conditions": "Retain",
            "license_texts": [{"path": "texts/Apache-2.0.txt",
                               "sha256": package_public.sha256_file(text_path)}],
        }
        index = {"schema": 1, "complete": True,
                 "legal_clearance_claim": False, "source_commit": "b" * 40,
                 "archive_count": 1, "components": [component]}
        index_path = bundle / "license-bundle.json"
        index_path.write_text(json.dumps(index))
        with self.assertRaises(package_public.PackagingError):
            package_public.validate_license_bundle(bundle, sbom)
        index["source_commit"] = "a" * 40
        index_path.write_text(json.dumps(index))
        (bundle / "unexpected.bin").write_text("unexpected\n")
        with self.assertRaises(package_public.PackagingError):
            package_public.validate_license_bundle(bundle, sbom)

    def test_missing_or_symlinked_repository_material_is_rejected(self):
        license_path = self.repo / "LICENSE"
        license_path.unlink()
        with self.assertRaises(package_public.PackagingError):
            self.run_package()

        license_path.symlink_to(self.repo / "COPYRIGHT.md")
        with self.assertRaises(package_public.PackagingError):
            self.run_package()

    def test_short_source_commit_must_resolve_to_full_commit(self):
        with self.assertRaises(package_public.PackagingError):
            package_public.full_source_commit(self.repo, "abcdef0")


if __name__ == "__main__":
    unittest.main()
