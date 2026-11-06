import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest import mock

MODULE_PATH = Path(__file__).resolve().parents[1] / "stage_firmware.py"
SPEC = importlib.util.spec_from_file_location("stage_firmware", MODULE_PATH)
stage_firmware = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(stage_firmware)
VERIFY_SPEC = importlib.util.spec_from_file_location(
    "verify_firmware_package", MODULE_PATH.with_name("verify_firmware_package.py"))
verify = importlib.util.module_from_spec(VERIFY_SPEC)
VERIFY_SPEC.loader.exec_module(verify)


class ArtifactStagingTests(unittest.TestCase):
    COMMIT = "583cfc5"
    FULL_COMMIT = "583cfc5" + "0" * 33

    def setUp(self):
        self.elf_patcher = mock.patch.object(
            stage_firmware, "package_elf",
            side_effect=lambda source, destination, target, packages_dir=None:
            shutil.copyfile(source, destination))
        self.elf_patcher.start()
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.repo = self.root / "repo"
        self.output = self.root / "staged"
        self.build = self.repo / ".pio/build/esp8266_public"
        self.build.mkdir(parents=True)
        self.write_firmware("PUBLIC", "ESP8266", self.COMMIT)
        for source_name, _, _ in stage_firmware.PACKAGE_MATERIALS:
            path = self.repo / source_name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(f"fixture: {source_name}\n", encoding="utf-8")

    def source_entries(self):
        return [
            ("LICENSE", 0o644, b"GPL fixture\n"),
            ("COPYRIGHT.md", 0o644, b"copyright fixture\n"),
            ("platformio.ini", 0o644, b"[env]\n"),
            ("tools/stage_firmware.py", 0o755, b"stage\n"),
            ("tools/verify_firmware_package.py", 0o755, b"verify\n"),
            ("tools/generate_web_assets.py", 0o755, b"assets\n"),
        ]

    def tearDown(self):
        self.elf_patcher.stop()
        self.temporary.cleanup()

    def metadata(self):
        return {"core_version": "6.1.19", "resolved_packages": [
            {"name": "Platform espressif8266", "version": "4.2.1"},
            {"name": "framework-arduinoespressif8266", "version": "3.30102.0"},
            {"name": "toolchain-xtensa", "version": "2.100300.220621"},
            {"name": "EspSoftwareSerial", "version": "8.2.0"},
            {"name": "ghostl", "version": "1.0.1"}]}

    def write_firmware(self, profile, target, commit,
                       environment="esp8266_public"):
        build = self.repo / ".pio/build" / environment
        build.mkdir(parents=True, exist_ok=True)
        firmware = build / "firmware.bin"
        firmware.write_bytes(
            f"WSPR Beacon 2.0\0dev\0{profile}\0{target}\0{commit}\0".encode())
        (build / "firmware.elf").write_bytes(b"ELF fixture")
        (build / "firmware.map").write_text(
            "Archive member included to satisfy reference by file (symbol)\n\n"
            f"{build}/libEspSoftwareSerial.a(serial.o)\n"
            "                              src/main.o (symbol)\n\n"
            "Discarded input sections\n", encoding="utf-8")
        evidence = {
            "build_environment": environment,
            "firmware_sha256": stage_firmware.sha256_file(firmware),
            "elf_sha256": stage_firmware.sha256_file(build / "firmware.elf"),
            "link_map_sha256": stage_firmware.sha256_file(build / "firmware.map"),
            "profile": profile, "project": "WSPR Beacon 2.0",
            "source_commit": commit, "target": target, "version": "dev",
        }
        (build / stage_firmware.PROVENANCE_FILE).write_text(
            json.dumps(evidence), encoding="utf-8")

    def staged(self, output=None, commit=None):
        output = output or self.output
        commit = commit or self.COMMIT
        with mock.patch.object(stage_firmware, "git_identity",
                               return_value=(commit, False)), \
             mock.patch.object(stage_firmware, "platformio_metadata",
                               return_value=self.metadata()), \
             mock.patch.object(stage_firmware, "git_archive_entries",
                               return_value=self.source_entries()):
            return stage_firmware.stage(self.repo, "esp8266_public", output)

    def test_manifest_checksums_and_output_are_deterministic(self):
        first = self.staged(self.root / "one")
        second = self.staged(self.root / "two")
        self.assertEqual(first.read_bytes(), second.read_bytes())
        self.assertEqual((first.parent / "SHA256SUMS").read_bytes(),
                         (second.parent / "SHA256SUMS").read_bytes())
        manifest = json.loads(first.read_text())
        self.assertEqual(manifest["manifest_schema"], 2)
        self.assertEqual(manifest["version"], "2.0.0-rc.1")
        self.assertEqual(manifest["firmware_version"], "dev")
        self.assertEqual(manifest["profile"], "PUBLIC")
        self.assertEqual(manifest["target"], "ESP8266")
        self.assertEqual(manifest["source_commit"], self.COMMIT)
        self.assertTrue(manifest["technical_package_eligible"])
        self.assertFalse(manifest["public_distribution_authorized"])
        self.assertEqual(manifest["legal_review_status"],
                         "NOT_APPROVED_FOR_PUBLIC_DISTRIBUTION")
        self.assertEqual(len(manifest["package_files"]), 22)
        self.assertEqual(manifest["packaging_revision"], 3)
        self.assertEqual(manifest["board"], "nodemcuv2")
        self.assertEqual(manifest["flash_parameters"], {
            "mode": "dio", "frequency": "40m", "size": "4MB",
            "recommended_baud": 115200})
        packaged = {item["filename"] for item in manifest["package_files"]}
        self.assertTrue({"INSTALLATION.md", "BUILDING.md", "USER_GUIDE.md",
                         "HARDWARE.md"}.issubset(packaged))
        inventory = json.loads((first.parent / manifest["dependency_inventory_filename"]).read_text())
        self.assertEqual(inventory["resolved"]["framework"]["name"],
                         "framework-arduinoespressif8266")
        self.assertEqual(len(inventory["linked_archive_members"]), 1)
        self.assertNotIn(str(self.root), (first.parent / manifest["link_map_filename"]).read_text())
        self.assertNotIn(str(self.root), first.read_text())

    def test_sha256s_cover_component_and_manifest(self):
        manifest_path = self.staged()
        lines = (self.output / "SHA256SUMS").read_text().splitlines()
        self.assertEqual(len(lines), 24)
        for line in lines:
            digest, filename = line.split("  ", 1)
            self.assertEqual(digest,
                             stage_firmware.sha256_file(self.output / filename))
        self.assertIn(manifest_path.name, lines[-1])

    def test_new_package_verifies_and_rejects_changed_map_or_inventory(self):
        manifest_path = self.staged()
        manifest = verify.verify_package(self.output)
        for key in ("link_map_filename", "dependency_inventory_filename"):
            path = self.output / manifest[key]
            original = path.read_bytes()
            path.write_bytes(original + b"altered")
            with self.assertRaises(verify.VerificationError):
                verify.verify_package(self.output)
            path.write_bytes(original)
        self.assertTrue(manifest_path.exists())

    def rewrite_package(self, output, manifest_path, mutate):
        manifest = json.loads(manifest_path.read_text())
        mutate(manifest)
        manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        lines = (output / "SHA256SUMS").read_text().splitlines()
        lines = [f"{stage_firmware.sha256_file(manifest_path)}  {manifest_path.name}"
                 if line.endswith("  " + manifest_path.name) else line for line in lines]
        (output / "SHA256SUMS").write_text("\n".join(lines) + "\n")

    def test_revision_3_rejects_flash_board_and_documentation_errors(self):
        mutations = (
            lambda data: data.pop("flash_parameters"),
            lambda data: data["flash_parameters"].update(mode="qio"),
            lambda data: data["flash_parameters"].update(recommended_baud=0),
            lambda data: data["flash_parameters"].update(recommended_baud=115200.0),
            lambda data: data.update(board="esp32dev"),
        )
        for index, mutate in enumerate(mutations):
            output = self.root / f"invalid-target-{index}"
            manifest_path = self.staged(output)
            self.rewrite_package(output, manifest_path, mutate)
            with self.assertRaises(verify.VerificationError):
                verify.verify_package(output)

        output = self.root / "missing-document"
        manifest_path = self.staged(output)
        (output / "BUILDING.md").unlink()
        with self.assertRaises(verify.VerificationError):
            verify.verify_package(output)

        output = self.root / "broken-link"
        manifest_path = self.staged(output)
        building = output / "BUILDING.md"
        building.write_text("[missing](MISSING.md)\n")
        digest = stage_firmware.sha256_file(building)
        manifest = json.loads(manifest_path.read_text())
        item = next(item for item in manifest["package_files"]
                    if item["filename"] == "BUILDING.md")
        item["sha256"] = digest
        item["size_bytes"] = building.stat().st_size
        manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        lines = (output / "SHA256SUMS").read_text().splitlines()
        lines = [f"{digest}  BUILDING.md" if line.endswith("  BUILDING.md")
                 else f"{stage_firmware.sha256_file(manifest_path)}  {manifest_path.name}"
                 if line.endswith("  " + manifest_path.name) else line for line in lines]
        (output / "SHA256SUMS").write_text("\n".join(lines) + "\n")
        with self.assertRaises(verify.VerificationError):
            verify.verify_package(output)

    def test_revision_2_package_remains_valid_without_revision_3_fields(self):
        manifest_path = self.staged()
        manifest = json.loads(manifest_path.read_text())
        removed = {"BUILDING.md", "USER_GUIDE.md", "HARDWARE.md"}
        manifest["packaging_revision"] = 2
        manifest.pop("board")
        manifest.pop("flash_parameters")
        manifest["package_files"] = [item for item in manifest["package_files"]
                                     if item["filename"] not in removed]
        for filename in removed:
            (self.output / filename).unlink()
        manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        lines = [line for line in (self.output / "SHA256SUMS").read_text().splitlines()
                 if line.split("  ", 1)[1] not in removed | {manifest_path.name}]
        lines.append(f"{stage_firmware.sha256_file(manifest_path)}  {manifest_path.name}")
        (self.output / "SHA256SUMS").write_text("\n".join(lines) + "\n")
        self.assertEqual(verify.verify_package(self.output)["packaging_revision"], 2)

    def test_flash_metadata_covers_all_four_environments(self):
        expected = {
            "ESP8266": ("nodemcuv2", 115200),
            "ESP32": ("esp32dev", 460800),
        }
        self.assertEqual(set(stage_firmware.ENVIRONMENTS), {
            "esp8266", "esp8266_public", "esp32", "esp32_public"})
        for environment, (_, target) in stage_firmware.ENVIRONMENTS.items():
            board, baud = expected[target]
            self.assertEqual(stage_firmware.TARGET_BOARDS[target], board)
            self.assertEqual(
                stage_firmware.FLASH_PARAMETERS[target],
                {"mode": "dio", "frequency": "40m", "size": "4MB",
                 "recommended_baud": baud})

    def test_inventory_rejects_wrong_target_or_sensitive_values_even_with_updated_hashes(self):
        for mutate in (lambda data: data.update(target="ESP32"),
                       lambda data: data.update(local_path="/home/builder/private"),
                       lambda data: data.update(api_key="sample-secret")):
            output = self.root / f"package-{len(list(self.root.glob('package-*')))}"
            manifest_path = self.staged(output)
            manifest = json.loads(manifest_path.read_text())
            inventory_path = output / manifest["dependency_inventory_filename"]
            inventory = json.loads(inventory_path.read_text())
            mutate(inventory)
            inventory_path.write_text(json.dumps(inventory))
            digest = stage_firmware.sha256_file(inventory_path)
            manifest["dependency_inventory_sha256"] = digest
            for item in manifest["package_files"]:
                if item["filename"] == inventory_path.name:
                    item["sha256"] = digest
                    item["size_bytes"] = inventory_path.stat().st_size
            manifest_path.write_text(json.dumps(manifest))
            lines = (output / "SHA256SUMS").read_text().splitlines()
            lines = [f"{digest}  {inventory_path.name}" if line.endswith(inventory_path.name)
                     else f"{stage_firmware.sha256_file(manifest_path)}  {manifest_path.name}"
                     if line.endswith(manifest_path.name) else line for line in lines]
            (output / "SHA256SUMS").write_text("\n".join(lines) + "\n")
            with self.assertRaises(verify.VerificationError):
                verify.verify_package(output)

    def test_development_package_is_technical_only(self):
        self.write_firmware("DEVELOPMENT", "ESP8266", self.COMMIT, "esp8266")
        with mock.patch.object(stage_firmware, "git_identity",
                               return_value=(self.COMMIT, False)), \
             mock.patch.object(stage_firmware, "platformio_metadata",
                               return_value=self.metadata()), \
             mock.patch.object(stage_firmware, "git_archive_entries",
                               return_value=self.source_entries()):
            stage_firmware.stage(self.repo, "esp8266", self.output)
        manifest = verify.verify_package(self.output)
        self.assertEqual(manifest["profile"], "DEVELOPMENT")
        self.assertTrue(manifest["technical_package_eligible"])
        self.assertFalse(manifest["public_release_eligible"])

    def test_missing_elf_or_map_rejected(self):
        for name in ("firmware.elf", "firmware.map"):
            path = self.build / name
            original = path.read_bytes()
            path.unlink()
            with self.assertRaises(stage_firmware.StagingError):
                self.staged()
            path.write_bytes(original)

    def test_esp32_public_technical_package_has_target_specific_inventory(self):
        build = self.repo / ".pio/build/esp32_public"
        self.write_firmware("PUBLIC", "ESP32", self.COMMIT, "esp32_public")
        for name in ("bootloader.bin", "partitions.bin"):
            (build / name).write_bytes(name.encode())
        boot = self.root / "boot_app0.bin"
        boot.write_bytes(b"boot")
        metadata = {"core_version": "6.1.19", "resolved_packages": [
            {"name": "Platform espressif32", "version": "7.1.3"},
            {"name": "framework-arduinoespressif32", "version": "4.20017.0"},
            {"name": "toolchain-xtensa-esp32", "version": "8.4.0"}]}
        partition = {"layout": [{"name": name, "offset": offset, "size": size}
                                for name, offset, size in stage_firmware.PARTITION_LAYOUT],
                     "nvs_range": {"offset": "0x9000", "size": "0x5000"},
                     "sha256": stage_firmware.sha256_file(build / "partitions.bin")}
        with mock.patch.object(stage_firmware, "git_identity",
                               return_value=(self.COMMIT, False)), \
             mock.patch.object(stage_firmware, "platformio_metadata",
                               return_value=metadata), \
             mock.patch.object(stage_firmware, "git_archive_entries",
                               return_value=self.source_entries()), \
             mock.patch.object(stage_firmware, "boot_app0_path", return_value=boot), \
             mock.patch.object(stage_firmware, "verify_partition_table",
                               return_value=partition):
            stage_firmware.stage(self.repo, "esp32_public", self.output)
        manifest = verify.verify_package(self.output)
        self.assertEqual(manifest["target"], "ESP32")
        self.assertEqual(manifest["board"], "esp32dev")
        self.assertEqual(manifest["flash_parameters"]["recommended_baud"], 460800)
        self.assertFalse(manifest["public_distribution_authorized"])
        self.assertFalse(manifest["public_release_eligible"])
        self.assertNotIn("ESPRESSIF-NONOS-SDK.txt",
                         {item["filename"] for item in manifest["package_files"]})

    def test_public_release_rejects_dirty_unknown_and_development(self):
        for identity in ((self.COMMIT, True), ("unknown", False)):
            with mock.patch.object(stage_firmware, "git_identity",
                                   return_value=identity):
                with self.assertRaises(stage_firmware.StagingError):
                    stage_firmware.stage(self.repo, "esp8266_public",
                                          self.output, True)
        with mock.patch.object(stage_firmware, "git_identity",
                               return_value=(self.COMMIT, False)):
            with self.assertRaises(stage_firmware.StagingError):
                stage_firmware.stage(self.repo, "esp8266", self.output, True)
            with self.assertRaises(stage_firmware.StagingError):
                stage_firmware.stage(self.repo, "esp32_public", self.output, True)

    def test_clean_public_release_uses_controlled_checkout(self):
        checkout = mock.MagicMock()
        checkout.__enter__.return_value = self.repo
        checkout.__exit__.return_value = False
        with mock.patch.object(stage_firmware, "git_identity",
                               return_value=(self.COMMIT, False)), \
             mock.patch.object(stage_firmware, "run",
                               return_value=self.FULL_COMMIT), \
             mock.patch.object(stage_firmware, "controlled_checkout",
                               return_value=checkout) as controlled, \
             mock.patch.object(stage_firmware, "platformio_metadata",
                               return_value=self.metadata()), \
             mock.patch.object(stage_firmware, "git_archive_entries",
                               return_value=self.source_entries()):
            manifest = stage_firmware.stage(
                self.repo, "esp8266_public", self.output, True)
        self.assertTrue(manifest.is_file())
        controlled.assert_called_once_with(
            self.repo, self.FULL_COMMIT, "esp8266_public")

    def test_missing_package_material_is_rejected(self):
        (self.repo / "LICENSE").unlink()
        with self.assertRaises(stage_firmware.StagingError):
            self.staged()

    def test_source_archive_rejects_legacy_sketch(self):
        entries = self.source_entries() + [("wspr_5.ino", 0o644, b"legacy")]
        with mock.patch.object(stage_firmware, "git_archive_entries",
                               return_value=entries):
            with self.assertRaises(stage_firmware.StagingError):
                stage_firmware.write_source_archive(
                    self.repo, self.COMMIT, self.root / "source.tar.gz")

    def test_unknown_environment_and_missing_binary_are_rejected(self):
        with self.assertRaises(stage_firmware.StagingError):
            stage_firmware.stage(self.repo, "unknown", self.output)
        (self.build / "firmware.bin").unlink()
        with mock.patch.object(stage_firmware, "git_identity",
                               return_value=(self.COMMIT, False)), \
             mock.patch.object(stage_firmware, "platformio_metadata",
                               return_value=self.metadata()):
            with self.assertRaises(stage_firmware.StagingError):
                stage_firmware.stage(self.repo, "esp8266_public", self.output)

    def test_stale_binary_is_rejected(self):
        (self.build / "firmware.bin").write_bytes(b"changed")
        with self.assertRaises(stage_firmware.StagingError):
            stage_firmware.load_provenance(
                self.repo, "esp8266_public", self.COMMIT, "PUBLIC", "ESP8266")

    def test_stale_elf_and_map_are_rejected(self):
        for name in ("firmware.elf", "firmware.map"):
            path = self.build / name
            original = path.read_bytes()
            path.write_bytes(original + b"stale")
            with self.assertRaises(stage_firmware.StagingError):
                stage_firmware.load_provenance(
                    self.repo, "esp8266_public", self.COMMIT, "PUBLIC", "ESP8266")
            path.write_bytes(original)

    def test_profile_target_and_commit_mismatch_are_rejected(self):
        for field, value in (("profile", "DEVELOPMENT"), ("target", "ESP32"),
                             ("source_commit", "1111111")):
            path = self.build / stage_firmware.PROVENANCE_FILE
            evidence = json.loads(path.read_text())
            evidence[field] = value
            path.write_text(json.dumps(evidence))
            with self.assertRaises(stage_firmware.StagingError):
                stage_firmware.load_provenance(
                    self.repo, "esp8266_public", self.COMMIT,
                    "PUBLIC", "ESP8266")
            self.write_firmware("PUBLIC", "ESP8266", self.COMMIT)

    def test_embedded_identity_mismatch_is_rejected(self):
        firmware = self.build / "firmware.bin"
        firmware.write_bytes(b"not an identity")
        evidence_path = self.build / stage_firmware.PROVENANCE_FILE
        evidence = json.loads(evidence_path.read_text())
        evidence["firmware_sha256"] = stage_firmware.sha256_file(firmware)
        evidence_path.write_text(json.dumps(evidence))
        with self.assertRaises(stage_firmware.StagingError):
            stage_firmware.load_provenance(
                self.repo, "esp8266_public", self.COMMIT, "PUBLIC", "ESP8266")

    def test_missing_toolchain_metadata_is_rejected(self):
        for metadata in ({"core_version": "unknown", "resolved_packages": []},
                         {"core_version": "6.1.19", "resolved_packages": []}):
            with self.assertRaises(stage_firmware.StagingError):
                stage_firmware.require_metadata(metadata)

    def test_nonempty_destination_is_rejected_without_deletion(self):
        self.output.mkdir()
        sentinel = self.output / "keep.txt"
        sentinel.write_text("user data")
        with self.assertRaises(stage_firmware.StagingError):
            self.staged()
        self.assertEqual(sentinel.read_text(), "user data")

    def test_failure_leaves_no_partial_final_package(self):
        with mock.patch.object(stage_firmware, "write_package",
                               side_effect=stage_firmware.StagingError("stop")):
            with self.assertRaises(stage_firmware.StagingError):
                self.staged()
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.root.glob(".staged-*")), [])

    def test_esp32_component_and_partition_failures_are_rejected(self):
        build = self.repo / ".pio/build/esp32_public"
        build.mkdir(parents=True)
        for name in ("bootloader.bin", "partitions.bin", "firmware.bin"):
            (build / name).write_bytes(name.encode())
        package_dir = self.root / "packages"
        boot = package_dir / "framework-arduinoespressif32/tools/partitions"
        boot.mkdir(parents=True)
        (boot / "boot_app0.bin").write_bytes(b"boot")
        sources = stage_firmware.component_sources(
            self.repo, "esp32_public", "ESP32", package_dir)
        self.assertEqual(len(sources), 4)
        (build / "bootloader.bin").unlink()
        with self.assertRaises(stage_firmware.StagingError):
            stage_firmware.component_sources(
                self.repo, "esp32_public", "ESP32", package_dir)
        with mock.patch.object(Path, "read_text", return_value="bad,row\n"):
            with self.assertRaises(stage_firmware.StagingError):
                stage_firmware.verify_partition_table(self.repo, "esp32_public")

    def test_package_parser_has_stable_order(self):
        parsed = stage_firmware.parse_packages(
            "├── Zebra @ 2.0.0 (required: z)\n"
            "Platform alpha @ 1.0.0 (required: a)\n")
        self.assertEqual(parsed, [
            {"name": "Platform alpha", "version": "1.0.0"},
            {"name": "Zebra", "version": "2.0.0"}])
        sources = stage_firmware.parse_package_sources(
            "Platform espressif8266 @ 4.2.1 (required: platformio/espressif8266 @ 4.2.1)\n"
            "├── framework-arduinoespressif8266 @ 3.30102.0 "
            "(required: platformio/framework-arduinoespressif8266 @ ~3.30102.0)\n")
        self.assertEqual(sources["framework-arduinoespressif8266"],
                         "platformio/framework-arduinoespressif8266 @ ~3.30102.0")


if __name__ == "__main__":
    unittest.main()
