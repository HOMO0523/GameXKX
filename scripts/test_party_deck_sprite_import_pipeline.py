#!/usr/bin/env python3
"""Static regression contract for the isolated PartyDeck UE texture importer."""

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PIPELINE = PROJECT_ROOT / "Content" / "Python" / "gamexxk_import_party_deck_sprite_atlases.py"
IMPORT_ROOT = "/Game/GameXXK/Sprites/Generated/PartyDeck"


def _load_pipeline():
    spec = importlib.util.spec_from_file_location("gamexxk_import_party_deck_sprite_atlases", PIPELINE)
    if spec is None or spec.loader is None:
        raise AssertionError(f"unable to load PartyDeck import pipeline: {PIPELINE}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class PartyDeckSpriteImportPipelineTests(unittest.TestCase):
    def test_preflight_has_exactly_twenty_four_isolated_nearest_no_mipmap_texture_imports(self) -> None:
        self.assertTrue(PIPELINE.is_file(), f"PartyDeck import pipeline is missing: {PIPELINE}")
        pipeline = _load_pipeline()

        plan = pipeline.validate_import_plan()

        self.assertTrue(plan["ok"])
        self.assertEqual(plan["destination_root"], IMPORT_ROOT)
        self.assertEqual(plan["texture_count"], 24)
        self.assertEqual(len(plan["textures"]), 24)
        self.assertEqual(plan["texture_settings"], {"filter": "nearest", "mipmaps": "none"})
        for texture in plan["textures"]:
            self.assertTrue(texture["source_path"].is_file())
            self.assertTrue(texture["asset_path"].startswith(f"{IMPORT_ROOT}/T_PartyDeck_"))
            self.assertFalse(texture["asset_path"].startswith("/Game/GameXXK/Characters/"))
            self.assertIn(tuple(texture["expected_pixels"]), {(171, 1640), (1026, 1640)})

    def test_importer_contract_never_replaces_or_deletes_existing_assets(self) -> None:
        self.assertTrue(PIPELINE.is_file(), f"PartyDeck import pipeline is missing: {PIPELINE}")
        pipeline = _load_pipeline()
        source = PIPELINE.read_text(encoding="utf-8")

        self.assertIn("task.replace_existing = False", source)
        self.assertNotIn("delete_asset(", source)
        self.assertNotIn("delete_directory(", source)
        self.assertIn("TextureFilter.TF_NEAREST", source)
        self.assertIn("TextureMipGenSettings.TMGS_NO_MIPMAPS", source)
        self.assertIn("--execute-import", source)


if __name__ == "__main__":
    unittest.main()
