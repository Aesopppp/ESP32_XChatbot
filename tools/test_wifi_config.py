"""Validate private Wi-Fi configuration using synthetic credentials."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class WifiConfigTests(unittest.TestCase):
    def generate(self, value):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, output = root / "wifi.json", root / "config.h"
            if value is not None:
                source.write_text(json.dumps(value), encoding="utf-8-sig")
            result = subprocess.run(
                [sys.executable, str(Path(__file__).with_name("generate_wifi_config.py")),
                 str(source), str(output)], capture_output=True, text=True
            )
            return result, output.read_text(encoding="utf-8") if output.exists() else ""

    def test_missing_file_disables_bootstrap(self):
        result, content = self.generate(None)
        self.assertEqual(result.returncode, 0)
        self.assertIn('#define WIFI_LOCAL_SSID ""', content)

    def test_escaping_and_no_secret_output(self):
        password = 'test-"quoted"\\value'
        result, content = self.generate({"ssid": "test-hotspot", "password": password})
        self.assertEqual(result.returncode, 0)
        self.assertIn(json.dumps(password), content)
        self.assertEqual(result.stdout + result.stderr, "")

    def test_byte_limits_and_invalid_values(self):
        for value in ({"ssid": "x" * 33}, {"ssid": "中" * 11}, {"ssid": "test\nname"},
                      {"ssid": "test", "password": "short"}, {"password": "testpass"},
                      {"ssid": "test", "password": "z" * 64}, {"ssid": ["test"]}, []):
            with self.subTest(value=value):
                result, content = self.generate(value)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(content, "")

    def test_supported_password_formats(self):
        for password in ("", "testpass", "x" * 63, "a" * 64):
            with self.subTest(password=password):
                result, _ = self.generate({"ssid": "test", "password": password})
                self.assertEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
