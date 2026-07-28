#!/usr/bin/env python3
"""Static contract for the generated PartyDeck paper/ink scrollbar pipeline."""

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PIPELINE_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_import_psd_paper_ink_scrollbar.py"
PREPARE_PATH = PROJECT_ROOT / "scripts" / "prepare_partydeck_scrollbar_generated_assets.py"
MANIFEST_PATH = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "ui-scrollbar" / "scrollbar_generated_manifest_v1.json"


def _load_pipeline():
    spec = importlib.util.spec_from_file_location("gamexxk_import_psd_paper_ink_scrollbar", PIPELINE_PATH)
    if spec is None or spec.loader is None:
        raise AssertionError(f"scrollbar pipeline is missing: {PIPELINE_PATH}")
    module = importlib.util.module_from_spec(spec)
    import sys
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class PsdPaperInkScrollbarPipelineTests(unittest.TestCase):
    def test_plan_locks_two_generated_nontext_assets_to_partydeck_ui_root(self) -> None:
        pipeline = _load_pipeline()
        plan = pipeline.validate_scrollbar_plan()
        self.assertTrue(plan["ok"])
        self.assertEqual(plan["destination_root"], "/Game/GameXXK/UI/PartyDeck/Scrollbars")
        self.assertEqual(plan["asset_count"], 2)
        self.assertEqual(
            [asset["assetName"] for asset in plan["assets"]],
            ["T_PartyDeck_ScrollPaperTrack_GeneratedV1", "T_PartyDeck_ScrollInkThumb_GeneratedV1"],
        )
        self.assertEqual([asset["pixels"] for asset in plan["assets"]], [[148, 1102], [163, 441]])
        self.assertTrue(all(Path(asset["source"]).is_file() for asset in plan["assets"]))

    def test_pipeline_explicitly_forbids_misclassified_psd_cuts_and_destruction(self) -> None:
        source = PIPELINE_PATH.read_text(encoding="utf-8")
        manifest = MANIFEST_PATH.read_text(encoding="utf-8")
        self.assertTrue(PREPARE_PATH.is_file())
        self.assertNotIn("PSD019", source)
        self.assertNotIn("PSD051", source)
        self.assertNotIn("019.png", manifest)
        self.assertNotIn("051.png", manifest)
        self.assertIn("task.replace_existing = False", source)
        self.assertNotIn("delete_asset(", source)
        self.assertNotIn("delete_directory(", source)
        self.assertIn("TextureGroup.TEXTUREGROUP_UI", source)


if __name__ == "__main__":
    unittest.main()
