"""Regression tests for the project-local town PSD import manifest."""

from __future__ import annotations

import importlib
import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT / "scripts") not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

try:
    import_manifest = importlib.import_module("town_psd_import_manifest")
    build_import_plan = getattr(import_manifest, "build_import_plan", None)
except ModuleNotFoundError:
    build_import_plan = None


class TownPsdImportManifestTest(unittest.TestCase):
    def test_import_plan_covers_separated_backgrounds_and_never_reuses_action_blank(self) -> None:
        self.assertIsNotNone(build_import_plan, "import manifest must export build_import_plan(package_root)")
        plan = build_import_plan(PROJECT_ROOT / "SourceArt" / "UI" / "PSD" / "town-v2")
        background_pages = {item.page for item in plan if item.group == "Backgrounds"}
        self.assertEqual({"hud", "character", "companion", "task", "map", "backpack"}, background_pages)
        self.assertTrue(all(item.source.is_file() for item in plan))
        self.assertTrue(all(item.asset_name.startswith("T_TownPsd_") for item in plan))
        self.assertTrue(all("ActionBlank" not in item.asset_name for item in plan))


if __name__ == "__main__":
    unittest.main()
