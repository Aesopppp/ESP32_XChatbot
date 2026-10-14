"""Validate build selection against the supported product and retained resources."""
import copy
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("ci_build", ROOT / "scripts/build.py")
build = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(build)


class CiSelectionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.variants = build._collect_variants(idf_version=(6, 1, 0))
        cls.manifest = json.loads(build._CI_REPRESENTATIVE_VARIANTS.read_text())

    def load_manifest(self, manifest):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "representatives.json"
            path.write_text(json.dumps(manifest))
            return build._load_representative_variants(self.variants, path)

    def test_ci_builds_only_supported_vocat(self):
        selected = self.load_manifest(self.manifest)
        self.assertEqual(len(selected), 1)
        self.assertEqual(selected[0]["board"], "espressif/esp-vocat")
        self.assertEqual(selected[0]["target"], "esp32s3")
        self.assertEqual(selected, self.variants)

    def test_runtime_and_resource_changes_select_vocat(self):
        for path in (
            "main/application.cc", "main/boards/common/wifi_board.cc",
            "main/boards/espressif/esp-vocat/assets/icon_speaker_waves.hex",
            "main/Kconfig.projbuild", "main/CMakeLists.txt", "main/idf_component.yml",
            "CMakeLists.txt", "sdkconfig.defaults.esp32s3", "dependencies.lock",
            ".github/workflows/build.yml", "scripts/ci/representative-variants.json",
            "scripts/build.py", "partitions/v2/16m.csv",
            "scripts/prepare_vocat_wake_assets.py", "tools/generate_acos_config.py",
        ):
            with self.subTest(path=path):
                self.assertEqual(build._select_variants_for_changes(self.variants, [path]), self.variants)

    def test_mixed_changes_do_not_duplicate_builds(self):
        paths = ["main/application.cc", "main/boards/espressif/esp-vocat/config.h"]
        for changes in (paths, list(reversed(paths))):
            self.assertEqual(build._select_variants_for_changes(self.variants, changes), self.variants)

    def test_docs_only_and_removed_boards_do_not_select_builds(self):
        for paths in ([], ["README.md"], ["main/boards/lckfb/szpi-esp32s3/config.h"]):
            self.assertEqual(build._select_variants_for_changes(self.variants, paths), [])

    def test_invalid_manifest_is_rejected(self):
        invalid = copy.deepcopy(self.manifest)
        invalid["variants"][0]["name"] = "removed-variant"
        with self.assertRaisesRegex(ValueError, "unavailable"):
            self.load_manifest(invalid)
        invalid = copy.deepcopy(self.manifest)
        invalid["variants"].append(invalid["variants"][0])
        with self.assertRaisesRegex(ValueError, "duplicate"):
            self.load_manifest(invalid)
        invalid = copy.deepcopy(self.manifest)
        invalid["variants"][0]["build_options"] = {"network_type": "ethernet"}
        with self.assertRaisesRegex(ValueError, "Unsupported build option"):
            self.load_manifest(invalid)

    def test_selected_languages_have_real_resources(self):
        self.assertEqual(set(build._collect_languages()), {"zh-CN", "en-US"})


if __name__ == "__main__":
    unittest.main()
