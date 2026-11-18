import contextlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import windows_flash as flash


class FlashTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="lanluo flash 中文 ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.bundle = self.root / "firmware"
        shutil.copytree(flash.ROOT / "firmware", self.bundle)

    def test_bundle_and_offsets(self):
        plan = flash.validate_firmware(self.bundle)
        self.assertEqual(plan[::2], ["0x0", "0x8000", "0x10000"])
        self.assertTrue(all(Path(path).is_file() for path in plan[1::2]))

    def test_tampered_image_is_rejected(self):
        with (self.bundle / "lanluo_pet.bin").open("ab") as file:
            file.write(b"corrupted")
        with self.assertRaisesRegex(ValueError, "校验失败"):
            flash.validate_firmware(self.bundle)

    def test_wrong_chip_and_layout_are_rejected(self):
        file = self.bundle / "manifest.json"
        original = json.loads(file.read_text())
        for field, value in [("chip", "esp32"), ("files", original["files"][:2])]:
            with self.subTest(field=field):
                file.write_text(json.dumps({**original, field: value}))
                with self.assertRaises(ValueError):
                    flash.validate_firmware(self.bundle)

    def test_missing_image_is_rejected(self):
        (self.bundle / "bootloader.bin").unlink()
        with self.assertRaisesRegex(ValueError, "缺失"):
            flash.validate_firmware(self.bundle)

    def test_single_native_usb_and_multiple_devices(self):
        port = lambda name, vid=0x303A, pid=0x1001: SimpleNamespace(device=name, vid=vid, pid=pid, description=name)
        self.assertEqual(flash.choose_port([port("COM2", None, None), port("COM7")]), "COM7")
        with contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(flash.choose_port([port("COM7"), port("COM8")], read=lambda _: "2"), "COM8")
            self.assertEqual(flash.choose_port([port("COM2", None, None)], read=lambda _: "1"), "COM2")
            with self.assertRaises(KeyboardInterrupt):
                flash.choose_port([port("COM7"), port("COM8")], read=lambda _: "q")
        with self.assertRaises(ValueError):
            flash.choose_port([])

    def test_port_and_command_are_not_shell_input(self):
        self.assertEqual(flash.port_name("com12"), "COM12")
        for name in ["COM0", "COM3 & del x", "COM5\n", "--port", "/dev/tty"]:
            with self.subTest(name=name), self.assertRaises(ValueError):
                flash.port_name(name)
        plan = flash.validate_firmware(self.bundle)
        command = flash.flash_command("COM5", 115200, plan)
        self.assertEqual(command[command.index("--chip") + 1], "esp32s3")
        self.assertEqual(command[-6:], plan)
        self.assertNotIn("erase-flash", command)

    def test_dry_run_does_not_launch_any_process(self):
        with patch.object(flash, "ROOT", self.root), patch.object(flash.subprocess, "run") as run:
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(flash.main(["--port", "COM5", "--dry-run"]), 0)
            run.assert_not_called()

    def test_failed_build_cannot_flash_old_bundle(self):
        fake_os = SimpleNamespace(name="nt", environ={})
        failure = subprocess.CalledProcessError(2, ["idf.py", "build"])
        with patch.object(flash, "os", fake_os), patch.object(flash, "build_command", return_value=["idf.py", "build"]):
            with patch.object(flash.importlib.util, "find_spec", return_value=True), patch("importlib.metadata.version", return_value="5.6.0"):
                with patch.object(flash.subprocess, "run", side_effect=failure) as run, contextlib.redirect_stdout(io.StringIO()):
                    with self.assertRaises(subprocess.CalledProcessError):
                        flash.main(["--build", "--port", "COM5"])
                    self.assertEqual(run.call_count, 1)

    def test_build_uses_independent_windows_cache(self):
        sdk = self.root / "idf"
        (sdk / "tools").mkdir(parents=True)
        (sdk / "tools/idf.py").touch()
        (self.root / "main").mkdir()
        (self.root / "main/main.c").touch()
        with patch.dict(flash.os.environ, {"IDF_PATH": str(sdk)}):
            command = flash.build_command(self.root)
        self.assertIn(str(self.root / "build-windows"), command)
        self.assertIn(f"SDKCONFIG={self.root / 'sdkconfig.windows'}", command)


if __name__ == "__main__":
    unittest.main()
