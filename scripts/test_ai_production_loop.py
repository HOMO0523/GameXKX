import unittest
from pathlib import Path

from ai_production_loop import (
    SCRIPT_TEST_MANIFEST,
    discover_script_tests,
    load_script_test_manifest,
)


class ScriptTestManifestTests(unittest.TestCase):
    def test_manifest_has_three_known_tags(self):
        manifest = load_script_test_manifest()
        self.assertEqual(1, manifest["schema"])
        self.assertEqual(
            {"headless", "asset-contract", "mcp-live"},
            set(manifest["tags"]),
        )

    def test_all_mode_excludes_asset_and_live_tags(self):
        manifest = load_script_test_manifest()
        headless = set(discover_script_tests("headless"))
        self.assertTrue(headless)
        self.assertTrue(set(manifest["tags"]["asset-contract"]).isdisjoint(headless))
        self.assertTrue(set(manifest["tags"]["mcp-live"]).isdisjoint(headless))

    def test_tagged_files_exist_under_scripts(self):
        manifest = load_script_test_manifest()
        for tag, names in manifest["tags"].items():
            for name in names:
                self.assertTrue(
                    (Path(__file__).parent / name).is_file(),
                    f"{tag}: {name}",
                )


if __name__ == "__main__":
    unittest.main()
