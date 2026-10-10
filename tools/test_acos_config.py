"""Exercise private config validation/escaping without using real credentials."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class AcosConfigTests(unittest.TestCase):
    def generate(self, value):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, output = root / "private.json", root / "config.h"
            if value is not None:
                source.write_text(json.dumps(value), encoding="utf-8-sig")
            result = subprocess.run(
                [sys.executable, str(Path(__file__).with_name("generate_acos_config.py")),
                 str(source), str(output)], capture_output=True, text=True
            )
            return result, output.read_text(encoding="utf-8") if output.exists() else ""

    def test_missing_file_uses_empty_token(self):
        result, content = self.generate(None)
        self.assertEqual(result.returncode, 0)
        self.assertIn('#define ACOS_LOCAL_TOKEN ""', content)

    def test_literal_is_escaped_and_secret_not_logged(self):
        token = 'test-"quoted"\\value'
        result, content = self.generate({"url": "wss://example.test/ws", "token": token})
        self.assertEqual(result.returncode, 0)
        self.assertIn(json.dumps(token), content)
        self.assertEqual(result.stdout + result.stderr, "")

    def test_invalid_configuration_is_rejected(self):
        for value in ({"url": "http://example.test"}, {"token": "test\nvalue"},
                      {"token": ["not a string"]}, {"url": "wss://bad\rhost"}):
            with self.subTest(value=value):
                result, content = self.generate(value)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(content, "")


if __name__ == "__main__":
    unittest.main()
