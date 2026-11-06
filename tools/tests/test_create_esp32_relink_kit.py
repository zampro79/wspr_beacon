import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock


MODULE_PATH = Path(__file__).resolve().parents[1] / "create_esp32_relink_kit.py"
SPEC = importlib.util.spec_from_file_location("create_esp32_relink_kit", MODULE_PATH)
relink = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(relink)


class Esp32RelinkKitTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.build = self.root / "project/.pio/build/esp32_public"
        (self.build / "src").mkdir(parents=True)
        (self.build / "src/main.cpp.o").write_bytes(b"app")
        (self.build / "lib123").mkdir()
        (self.build / "lib123/libExample.a").write_bytes(b"library")
        (self.build / "libFrameworkArduino.a").write_bytes(b"framework")
        (self.build / "firmware.bin").write_bytes(b"firmware")
        self.framework = self.root / "packages/framework-arduinoespressif32"
        self.toolchain = self.root / "packages/toolchain-xtensa-esp32"
        (self.framework / "cores/esp32").mkdir(parents=True)
        (self.framework / "variants/esp32").mkdir(parents=True)
        self.toolchain.mkdir(parents=True)
        self.source = self.root / "official-source"
        (self.source / "cores/esp32").mkdir(parents=True)
        (self.source / "variants/esp32").mkdir(parents=True)
        for name in ("Wire", "SPI", "WiFi", "Preferences"):
            directory = self.source / "libraries" / name
            directory.mkdir(parents=True)
            (directory / "source.cpp").write_text("void x(){}\n")
        (self.source / "cores/esp32/esp32-hal-misc.c").write_text("void x(){}\n")
        (self.source / "variants/esp32/pins_arduino.h").write_text("#pragma once\n")
        (self.source / "LICENSE.md").write_text("LGPL\n")
        self.library_sources = self.root / "library-sources"
        for name in ("Adafruit BusIO", "Adafruit GFX Library", "Adafruit SSD1306",
                     "Etherkit JTEncode", "Etherkit Si5351"):
            directory = self.library_sources / name
            directory.mkdir(parents=True)
            (directory / "source.cpp").write_text("void x(){}\n")
        self.archive = self.root / "source.tar.gz"
        self.archive.write_bytes(b"source")
        self.staged = self.root / "staged"
        self.staged.mkdir()
        self.provenance = {
            "target": "ESP32", "profile": "PUBLIC",
            "build_environment": "esp32_public", "technical_package_eligible": True,
            "source_commit": "a" * 40,
            "components": [{"offset": "0x10000",
                            "sha256": relink.digest(self.build / "firmware.bin")}],
            "package_files": [{"role": "corresponding-source",
                               "sha256": relink.digest(self.archive)}],
        }
        self.esptool = self.root / "packages/tool-esptoolpy/esptool.py"
        self.esptool.parent.mkdir(parents=True)
        self.esptool.write_text("# esptool\n")
        b = self.build
        f = self.framework
        self.log = self.root / "verbose.log"
        self.log.write_text(
            f"xtensa-esp32-elf-gcc -o {b}/FrameworkArduino/esp32-hal-misc.c.o "
            f"-I{f}/cores/esp32 {f}/cores/esp32/esp32-hal-misc.c\n"
            f"xtensa-esp32-elf-g++ -o {b}/firmware.elf {b}/src/main.cpp.o "
            f"-L{f}/tools/sdk/esp32/lib {b}/libFrameworkArduino.a\n"
            f'"/usr/bin/python3" "{self.esptool}" --chip esp32 elf2image '
            f'-o {b}/firmware.bin {b}/firmware.elf\n')

    def tearDown(self):
        self.temp.cleanup()

    def test_kit_is_path_independent_and_hash_complete(self):
        output = self.root / "kit"
        with mock.patch.object(relink.verify_firmware_package, "verify_package",
                               return_value=self.provenance):
            path = relink.create(self.build, self.log, output, self.archive,
                                 self.framework, self.toolchain, self.esptool,
                                 self.source, self.staged, "a" * 40,
                                 self.library_sources)
        manifest = json.loads(path.read_text())
        self.assertEqual(len(manifest["inputs"]), 3)
        self.assertEqual(len(manifest["lgpl_source_files"]), 7)
        self.assertEqual(len(manifest["direct_library_source_files"]), 5)
        self.assertFalse(manifest["release_ready"])
        self.assertEqual(manifest["provenance"]["source_commit"], "a" * 40)
        text = (output / "commands.json").read_text()
        self.assertNotIn(str(self.root), text)
        self.assertIn("${KIT_ROOT}", text)
        self.assertIn("lgpl-source/cores/esp32", text)

    def test_missing_command_and_nonempty_output_are_rejected(self):
        self.log.write_text("no commands\n")
        with self.assertRaises(relink.RelinkKitError):
            with mock.patch.object(relink.verify_firmware_package, "verify_package",
                                   return_value=self.provenance):
                relink.create(self.build, self.log, self.root / "kit", self.archive,
                              self.framework, self.toolchain, self.esptool,
                              self.source, self.staged, "a" * 40,
                              self.library_sources)
        occupied = self.root / "occupied"
        occupied.mkdir()
        (occupied / "file").write_text("x")
        with self.assertRaises(relink.RelinkKitError):
            relink.safe_output(occupied)

    def test_provenance_mismatch_is_rejected(self):
        broken = dict(self.provenance)
        broken["components"] = [{"offset": "0x10000", "sha256": "0" * 64}]
        with mock.patch.object(relink.verify_firmware_package, "verify_package",
                               return_value=broken):
            with self.assertRaises(relink.RelinkKitError):
                relink.create(self.build, self.log, self.root / "kit", self.archive,
                              self.framework, self.toolchain, self.esptool,
                              self.source, self.staged, "a" * 40,
                              self.library_sources)

    def test_output_parent_is_created(self):
        output = self.root / "new-parent/kit"
        with mock.patch.object(relink.verify_firmware_package, "verify_package",
                               return_value=self.provenance):
            result = relink.create(
                self.build, self.log, output, self.archive, self.framework,
                self.toolchain, self.esptool, self.source, self.staged,
                "a" * 40, self.library_sources)
        self.assertTrue(result.is_file())


if __name__ == "__main__":
    unittest.main()
