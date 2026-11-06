import importlib.util
import json
from pathlib import Path
import sys
import unittest


MODULE_PATH = Path(__file__).resolve().parents[1] / "esp32_archive_licenses.py"
SPEC = importlib.util.spec_from_file_location("esp32_archive_licenses", MODULE_PATH)
licenses = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(licenses)


class Esp32ArchiveLicenseTests(unittest.TestCase):
    def test_exact_phase_one_unresolved_set_is_covered(self):
        package = Path("/tmp/wspr-clean-esp32-public-abebe31-package/compliance/esp32-sbom.json")
        if not package.is_file():
            self.skipTest("clean Phase 1 package not present")
        sbom = json.loads(package.read_text())
        unresolved = set(sbom["unresolved_incorporated_archives"])
        self.assertEqual(unresolved, set(licenses.ARCHIVE_LICENSES))
        self.assertEqual(len(unresolved), 49)

    def test_every_record_has_required_evidence_fields(self):
        required = {"license", "origin", "upstream_version", "license_evidence",
                    "copyright_notice", "redistribution_conditions",
                    "source_obligation", "status"}
        for name, item in licenses.ARCHIVE_LICENSES.items():
            with self.subTest(name=name):
                self.assertEqual(set(item), required)
                self.assertTrue(all(item.values()))

    def test_all_formerly_unresolved_archives_are_now_classified(self):
        unresolved = [name for name, item in licenses.ARCHIVE_LICENSES.items()
                      if item["status"].startswith("UNRESOLVED")]
        self.assertEqual(unresolved, [])
        diagnostics = licenses.ARCHIVE_LICENSES["libesp_diagnostics.a"]
        self.assertEqual(diagnostics["license"], "Apache-2.0")
        self.assertIn("203a87f671710295579404f7d4c62a8a5e25ae7c",
                      diagnostics["upstream_version"])
        self.assertIn("f7e621272ab8b74c27a2dbd2cd818d6439cc01b0b130654e0fc9c6d3f796e940",
                      diagnostics["license_evidence"])

    def test_priority_wrappers_are_not_mislabeled_as_binary_blobs(self):
        for name in ("libbt.a", "libesp_phy.a", "libesp_wifi.a"):
            self.assertEqual(licenses.ARCHIVE_LICENSES[name]["origin"],
                             "ESP-IDF source component")
        for name in ("libcoexist.a", "libnet80211.a", "libpp.a",
                     "libsmartconfig.a"):
            self.assertIn("binary archive", licenses.ARCHIVE_LICENSES[name]["origin"])


if __name__ == "__main__":
    unittest.main()
