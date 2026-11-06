import hashlib
import gzip
import io
import importlib.util
import json
from pathlib import Path
import tarfile
import tempfile
import unittest


MODULE_PATH = Path(__file__).resolve().parents[1] / "verify_firmware_package.py"
SPEC = importlib.util.spec_from_file_location("verify_firmware_package", MODULE_PATH)
verify = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(verify)


class PackageVerificationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.package = Path(self.temporary.name) / "package"
        self.package.mkdir()

    def tearDown(self):
        self.temporary.cleanup()

    @staticmethod
    def digest(data):
        return hashlib.sha256(data).hexdigest()

    def make_source_archive(self, commit, legacy=False):
        name = f"wspr-beacon-{verify.RELEASE_CANDIDATE}-source.tar.gz"
        path = self.package / name
        root = f"wspr-beacon-{verify.RELEASE_CANDIDATE}-source"
        entries = {
            "SOURCE_COMMIT": (commit + "\n").encode(),
            "LICENSE": b"GPL fixture\n",
            "COPYRIGHT.md": b"copyright fixture\n",
            "platformio.ini": b"[env]\n",
            "tools/stage_firmware.py": b"stage\n",
            "tools/verify_firmware_package.py": b"verify\n",
            "tools/generate_web_assets.py": b"assets\n",
        }
        if legacy:
            entries["wspr_5.ino"] = b"legacy\n"
        with path.open("wb") as raw:
            with gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0) as zipped:
                with tarfile.open(fileobj=zipped, mode="w") as archive:
                    for filename, content in sorted(entries.items()):
                        info = tarfile.TarInfo(f"{root}/{filename}")
                        info.size = len(content)
                        info.mtime = 0
                        archive.addfile(info, io.BytesIO(content))
        return path

    def make_package(self, target="ESP8266"):
        commit = "3a1bf1d" + "0" * 33
        firmware_commit = commit[:7]
        stem = (f"wspr-beacon-{verify.RELEASE_CANDIDATE}-public-"
                f"{target.lower()}-{firmware_commit}")
        kinds = verify.OFFSETS[target]
        components = []
        for kind, offset in kinds.items():
            filename = f"{stem}-{kind}"
            data = (kind + target).encode()
            (self.package / filename).write_bytes(data)
            components.append({"filename": filename, "offset": offset,
                               "sha256": self.digest(data),
                               "size_bytes": len(data)})
        firmware = next(c for c in components
                        if c["filename"].endswith("-firmware.bin"))
        package_files = []
        for filename, role in verify.REQUIRED_PACKAGE_FILES.items():
            data = f"fixture: {filename}\n".encode()
            (self.package / filename).write_bytes(data)
            package_files.append({"filename": filename, "role": role,
                                  "sha256": self.digest(data),
                                  "size_bytes": len(data)})
        source = self.make_source_archive(commit)
        package_files.append({
            "filename": source.name, "role": "corresponding-source",
            "sha256": verify.sha256_file(source),
            "size_bytes": source.stat().st_size,
        })
        manifest = {
            "build_environment": target.lower() + "_public",
            "components": components,
            "direct_dependencies": ["example@1"],
            "firmware_filename": firmware["filename"],
            "firmware_sha256": firmware["sha256"],
            "firmware_size_bytes": firmware["size_bytes"],
            "firmware_source_commit": firmware_commit,
            "firmware_version": "dev",
            "git_worktree": "clean",
            "legal_review_status": verify.LEGAL_REVIEW_STATUS,
            "manifest_schema": 2,
            "package_files": package_files,
            "platformio": {"core_version": "6.1.19",
                           "resolved_packages": [{"name": "p", "version": "1"}]},
            "profile": "PUBLIC", "project": "WSPR Beacon 2.0",
            "public_distribution_authorized": False,
            "public_release_eligible": True, "source_commit": commit,
            "target": target, "technical_package_eligible": True,
            "version": verify.RELEASE_CANDIDATE,
        }
        if target == "ESP32":
            partition = next(c for c in components
                             if c["filename"].endswith("-partitions.bin"))
            manifest["partition_table"] = {
                "layout": [{"name": name, "offset": hex(offset), "size": hex(size)}
                           for name, offset, size in verify.ESP32_LAYOUT],
                "nvs_range": {"offset": "0x9000", "size": "0x5000"},
                "sha256": partition["sha256"],
            }
        manifest_name = f"{stem}-manifest.json"
        manifest_path = self.package / manifest_name
        manifest_path.write_text(json.dumps(manifest, sort_keys=True) + "\n")
        lines = [f"{c['sha256']}  {c['filename']}" for c in components]
        lines.extend(f"{item['sha256']}  {item['filename']}"
                     for item in package_files)
        lines.append(f"{verify.sha256_file(manifest_path)}  {manifest_name}")
        (self.package / "SHA256SUMS").write_text("\n".join(lines) + "\n")
        return manifest_path

    def rewrite(self, manifest_path, mutate):
        manifest = json.loads(manifest_path.read_text())
        mutate(manifest)
        manifest_path.write_text(json.dumps(manifest, sort_keys=True) + "\n")
        sums = (self.package / "SHA256SUMS").read_text().splitlines()
        sums[-1] = f"{verify.sha256_file(manifest_path)}  {manifest_path.name}"
        (self.package / "SHA256SUMS").write_text("\n".join(sums) + "\n")

    def assert_rejected(self):
        with self.assertRaises(verify.VerificationError):
            verify.verify_package(self.package)

    def test_valid_esp8266_package_and_esp32_rc_rejection(self):
        manifest = self.make_package()
        self.assertEqual(verify.verify_package(self.package)["target"], "ESP8266")
        for path in list(self.package.iterdir()):
            path.unlink()
        self.make_package("ESP32")
        self.assert_rejected()

    def test_malformed_json_and_unsupported_schema(self):
        manifest = self.make_package()
        manifest.write_text("{")
        self.assert_rejected()
        for path in list(self.package.iterdir()):
            path.unlink()
        manifest = self.make_package()
        self.rewrite(manifest, lambda data: data.update(manifest_schema=3))
        self.assert_rejected()

    def test_unknown_packaging_revision_is_rejected(self):
        manifest = self.make_package()
        self.rewrite(manifest, lambda data: data.update(packaging_revision=4))
        self.assert_rejected()

        manifest = self.make_package()
        self.rewrite(manifest, lambda data: data.update(packaging_revision=1.0))
        self.assert_rejected()

    def test_development_wrong_target_and_environment_are_rejected(self):
        for field, value in (("profile", "DEVELOPMENT"), ("target", "OTHER"),
                             ("build_environment", "esp32_public")):
            for path in list(self.package.iterdir()):
                path.unlink()
            manifest = self.make_package()
            self.rewrite(manifest, lambda data, f=field, v=value: data.update({f: v}))
            self.assert_rejected()

    def test_dirty_or_ineligible_package_is_rejected(self):
        for field, value in (("git_worktree", "dirty"),
                             ("public_release_eligible", False),
                             ("technical_package_eligible", False),
                             ("public_distribution_authorized", True),
                             ("legal_review_status", "APPROVED")):
            for path in list(self.package.iterdir()):
                path.unlink()
            manifest = self.make_package()
            self.rewrite(manifest, lambda data, f=field, v=value: data.update({f: v}))
            self.assert_rejected()

    def test_missing_component_wrong_size_and_wrong_hash(self):
        manifest = self.make_package()
        component = json.loads(manifest.read_text())["components"][0]
        (self.package / component["filename"]).unlink()
        self.assert_rejected()
        for field, value in (("size_bytes", 999), ("sha256", "0" * 64)):
            for path in list(self.package.iterdir()):
                path.unlink()
            manifest = self.make_package()
            self.rewrite(manifest, lambda data, f=field, v=value:
                         data["components"][0].update({f: v}))
            self.assert_rejected()

    def test_invalid_offset_and_partition_layout_are_rejected(self):
        manifest = self.make_package()
        self.rewrite(manifest, lambda data:
                     data["components"][0].update(offset="0x10000"))
        self.assert_rejected()
        for path in list(self.package.iterdir()):
            path.unlink()
        manifest = self.make_package()
        self.rewrite(manifest, lambda data:
                     data.update(firmware_source_commit="1111111"))
        self.assert_rejected()

    def test_missing_and_altered_legal_material_are_rejected(self):
        manifest = self.make_package()
        (self.package / "LICENSE").unlink()
        self.assert_rejected()
        for path in list(self.package.iterdir()):
            path.unlink()
        self.make_package()
        (self.package / "COPYRIGHT.md").write_text("altered")
        self.assert_rejected()

    def test_source_commit_mismatch_and_legacy_reintroduction_are_rejected(self):
        manifest_path = self.make_package()
        self.rewrite(
            manifest_path,
            lambda data: data.update(source_commit="3a1bf1d" + "1" * 33),
        )
        self.assert_rejected()
        for path in list(self.package.iterdir()):
            path.unlink()
        manifest_path = self.make_package()
        manifest = json.loads(manifest_path.read_text())
        source_item = next(item for item in manifest["package_files"]
                           if item["role"] == "corresponding-source")
        source = self.make_source_archive(manifest["source_commit"], legacy=True)
        source_item["sha256"] = verify.sha256_file(source)
        source_item["size_bytes"] = source.stat().st_size
        manifest_path.write_text(json.dumps(manifest, sort_keys=True) + "\n")
        lines = []
        for path in self.package.iterdir():
            if path.name != "SHA256SUMS":
                lines.append(f"{verify.sha256_file(path)}  {path.name}")
        (self.package / "SHA256SUMS").write_text("\n".join(sorted(lines)) + "\n")
        self.assert_rejected()

    def test_checksum_mismatch_and_missing_manifest_entry_are_rejected(self):
        manifest = self.make_package()
        sums = (self.package / "SHA256SUMS").read_text().splitlines()
        sums[0] = "0" * 64 + sums[0][64:]
        (self.package / "SHA256SUMS").write_text("\n".join(sums) + "\n")
        self.assert_rejected()
        (self.package / "SHA256SUMS").write_text("\n".join(sums[:-1]) + "\n")
        self.assert_rejected()

    def test_unexpected_file_and_second_manifest_are_rejected(self):
        manifest = self.make_package()
        (self.package / "extra.txt").write_text("unexpected")
        self.assert_rejected()
        (self.package / "extra.txt").unlink()
        (self.package / "other-manifest.json").write_text(manifest.read_text())
        self.assert_rejected()

    def test_path_traversal_absolute_and_symlink_are_rejected(self):
        manifest = self.make_package()
        for unsafe in ("../firmware.bin", "/tmp/firmware.bin", "a/b.bin"):
            data = json.loads(manifest.read_text())
            data["components"][0]["filename"] = unsafe
            manifest.write_text(json.dumps(data))
            self.assert_rejected()
        for path in list(self.package.iterdir()):
            path.unlink()
        manifest = self.make_package()
        component = json.loads(manifest.read_text())["components"][0]["filename"]
        path = self.package / component
        path.unlink()
        path.symlink_to(manifest)
        self.assert_rejected()


if __name__ == "__main__":
    unittest.main()
