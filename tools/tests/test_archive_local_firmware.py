import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location(
    "archive_local_firmware", TOOLS / "archive_local_firmware.py")
archive = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(archive)


class LocalArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.repo = Path(self.temporary.name) / "repo"
        self.output = self.repo / "local_firmware"
        self.repo.mkdir()

    def tearDown(self):
        self.temporary.cleanup()

    def test_four_targets_use_distinct_retained_packages(self):
        digest = "a" * 64
        def staged(repo, environment, destination):
            destination.mkdir(parents=True)
            manifest = {"build_environment": environment,
                        "firmware_source_commit": "6010f5d",
                        "firmware_sha256": digest,
                        "public_distribution_authorized": False}
            (destination / "package-manifest.json").write_text(json.dumps(manifest))
            return destination / "package-manifest.json"
        with mock.patch.object(archive.subprocess, "run") as build, \
             mock.patch.object(archive.stage_firmware, "git_identity",
                               return_value=("6010f5d", False)), \
             mock.patch.object(archive.stage_firmware, "load_provenance"), \
             mock.patch.object(archive.stage_firmware, "sha256_file", return_value=digest), \
             mock.patch.object(archive.stage_firmware, "stage", side_effect=staged) as stage, \
             mock.patch.object(archive.verify_firmware_package, "verify_package",
                               side_effect=lambda path: json.loads(
                                   (path / "package-manifest.json").read_text())):
            paths = [archive.archive_environment(self.repo, env, self.output)
                     for env in archive.ENVIRONMENTS]
            self.assertEqual(len(set(paths)), 4)
            self.assertEqual(stage.call_count, 4)
            self.assertEqual(build.call_count, 4)
            for env, path in zip(archive.ENVIRONMENTS, paths):
                self.assertEqual(path.parent.name, env)
                self.assertTrue(path.name.endswith(digest[:16]))
            self.assertEqual(archive.archive_environment(
                self.repo, "esp8266_public", self.output), paths[1])
            self.assertEqual(stage.call_count, 4)

    def test_failed_build_preserves_existing_archive(self):
        preserved = self.output / "esp8266_public" / "previous-valid-package"
        preserved.mkdir(parents=True)
        marker = preserved / "firmware.bin"
        marker.write_bytes(b"previous valid firmware")
        with mock.patch.object(archive.subprocess, "run",
                               side_effect=archive.subprocess.CalledProcessError(1, "pio")), \
             mock.patch.object(archive.stage_firmware, "stage") as stage:
            with self.assertRaises(archive.subprocess.CalledProcessError):
                archive.archive_environment(self.repo, "esp8266_public", self.output)
            stage.assert_not_called()
        self.assertEqual(marker.read_bytes(), b"previous valid firmware")


if __name__ == "__main__":
    unittest.main()
