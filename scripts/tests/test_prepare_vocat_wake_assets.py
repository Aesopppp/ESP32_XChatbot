import importlib.util
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))
SPEC = importlib.util.spec_from_file_location(
    "prepare_vocat_wake_assets", SCRIPTS / "prepare_vocat_wake_assets.py"
)
BUILD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BUILD)


class VoCatWakeAssetsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.sdkconfig = self.root / "sdkconfig"
        self.config = self.root / "config.json"
        self.config.write_text(json.dumps({
            "wakenet_model": "vendor.bin", "text_font": "font", "emoji_collection": "emoji"
        }), encoding="utf-8")
        self.staging = self.root / "staging"

    def prepare(self):
        BUILD.prepare(self.sdkconfig, self.root / "models", self.config, self.staging)
        return json.loads((self.staging / "360_360/config.json").read_text())

    def test_packages_selected_model_instead_of_vendor_model(self):
        self.sdkconfig.write_text(
            'CONFIG_IDF_TARGET="esp32s3"\nCONFIG_USE_AFE_WAKE_WORD=y\n'
            'CONFIG_SR_WN_WN9_NIHAOXIAOYI_TTS2=y\n'
            '# CONFIG_SR_WN_WN9_NIHAOXIAOZHI_TTS is not set\n'
        )
        model = self.root / "models/wakenet_model/wn9_nihaoxiaoyi_tts2"
        model.mkdir(parents=True)
        (model / "wn9_data").write_bytes(b"model data")
        config = self.prepare()
        self.assertEqual(config["wakenet_model"], "srmodels.bin")
        self.assertEqual(config["text_font"], "font")
        self.assertEqual(config["emoji_collection"], "emoji")
        packed = (self.staging / "wakenet/srmodels.bin").read_bytes()
        self.assertEqual(struct.unpack_from("<I", packed)[0], 1)
        self.assertEqual(packed[4:36].rstrip(b"\0"), b"wn9_nihaoxiaoyi_tts2")

    def test_missing_selected_model_fails_without_vendor_fallback(self):
        self.sdkconfig.write_text(
            'CONFIG_USE_AFE_WAKE_WORD=y\nCONFIG_SR_WN_WN9_NIHAOXIAOYI_TTS2=y\n'
        )
        with self.assertRaises(ValueError):
            self.prepare()

    def test_disabled_wake_does_not_package_vendor_model(self):
        self.sdkconfig.write_text('CONFIG_WAKE_WORD_DISABLED=y\n')
        self.assertEqual(self.prepare()["wakenet_model"], "none")
        self.assertFalse((self.staging / "wakenet/srmodels.bin").exists())


if __name__ == "__main__":
    unittest.main()
