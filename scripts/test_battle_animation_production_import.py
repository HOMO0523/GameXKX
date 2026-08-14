#!/usr/bin/env python3
"""Contracts for importing the approved production animation atlases into UE."""

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
IMPORTER_PATH = PROJECT_ROOT / "Content/Python/gamexxk_import_battle_animation_production.py"
PRODUCTION_ROOT = PROJECT_ROOT / "SourceAssets/AnimationProcessing/Production"


def load_importer():
    spec = importlib.util.spec_from_file_location("gamexxk_import_battle_animation_production", IMPORTER_PATH)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


class BattleAnimationProductionImportTests(unittest.TestCase):
    def test_uses_bc7_no_mipmap_policy_for_every_production_atlas(self) -> None:
        importer = load_importer()

        self.assertEqual(importer.TEXTURE_COMPRESSION_SETTING, "TC_BC7")
        self.assertEqual(importer.TEXTURE_MIP_SETTING, "TMGS_NO_MIPMAPS")
        self.assertEqual(importer.TEXTURE_FILTER_SETTING, "TF_BILINEAR")
        self.assertTrue(importer.TEXTURE_SRGB)

    def test_pilot_set_is_limited_to_hero_and_rooster_combat_clips(self) -> None:
        importer = load_importer()

        self.assertEqual(
            importer.PILOT_ASSET_IDS,
            {
                "character_00_hero_idle",
                "character_00_hero_attack",
                "character_00_hero_hit",
                "enemy_01_rooster_idle",
                "enemy_01_rooster_attack",
                "enemy_01_rooster_hit",
            },
        )

    def test_discovers_every_successful_atlas_and_only_idle_flipbooks(self) -> None:
        importer = load_importer()
        entries = importer.discover_animation_assets(PRODUCTION_ROOT)

        self.assertEqual(len(entries), 138)
        self.assertEqual(sum(entry.create_idle_flipbook for entry in entries), 34)
        self.assertNotIn("enemy_07_graywolf_attack", {entry.asset_id for entry in entries})
        self.assertIn("impact_ink_generic", {entry.asset_id for entry in entries})

    def test_uses_stable_texture_and_idle_flipbook_names(self) -> None:
        importer = load_importer()
        entries = importer.discover_animation_assets(PRODUCTION_ROOT)
        hero_idle = next(entry for entry in entries if entry.asset_id == "character_00_hero_idle")
        hero_attack = next(entry for entry in entries if entry.asset_id == "character_00_hero_attack")

        self.assertEqual(hero_idle.texture_name, "T_character_00_hero_idle_atlas")
        self.assertEqual(hero_idle.flipbook_name, "FB_character_00_hero_idle")
        self.assertTrue(hero_idle.create_idle_flipbook)
        self.assertFalse(hero_attack.create_idle_flipbook)
        self.assertEqual(hero_attack.flipbook_name, "")

    def test_rejects_manifest_drift_before_touching_ue_assets(self) -> None:
        importer = load_importer()
        bad_manifest = {
            "frameCount": 59,
            "canvasSize": 512,
            "fps": 12,
            "atlas": "C:/missing.png",
        }
        with self.assertRaisesRegex(RuntimeError, "frameCount"):
            importer.validate_manifest("bad", bad_manifest, require_atlas_file=False)

    def test_two_k_mode_switches_grid_to_half_resolution(self) -> None:
        importer = load_importer()

        self.assertEqual(importer.TWO_K_ATLAS_SIZE, 2048)
        self.assertEqual(importer.TWO_K_CELL_SIZE, 256)
        self.assertEqual(importer.TWO_K_PRODUCTION_RELATIVE_ROOT.name, "Production2K")

        # Default mode still validates the approved 4K grid.
        four_k_manifest = {
            "frameCount": 60,
            "canvasSize": 512,
            "fps": 12,
            "atlas": "C:/staging/unit_atlas.png",
            "atlasGrid": {"columns": 8, "rows": 8, "cellWidth": 512, "cellHeight": 512},
        }
        with self.assertRaisesRegex(RuntimeError, "atlas name"):
            importer.validate_manifest("other", four_k_manifest, require_atlas_file=False)

        importer._apply_two_k_mode()
        self.assertEqual(importer.ATLAS_SIZE, 2048)
        self.assertEqual(importer.CELL_SIZE, 256)
        two_k_manifest = {
            "frameCount": 60,
            "canvasSize": 256,
            "fps": 12,
            "atlas": "C:/staging/unit_atlas.png",
            "atlasGrid": {"columns": 8, "rows": 8, "cellWidth": 256, "cellHeight": 256},
        }
        with self.assertRaisesRegex(RuntimeError, "atlas name"):
            importer.validate_manifest("other", two_k_manifest, require_atlas_file=False)
        # A 4K-shaped manifest is now rejected under 2K mode.
        with self.assertRaisesRegex(RuntimeError, "canvasSize"):
            importer.validate_manifest("unit", four_k_manifest, require_atlas_file=False)

    def test_one_k_mode_switches_grid_to_quarter_resolution(self) -> None:
        importer = load_importer()

        self.assertEqual(importer.ONE_K_ATLAS_SIZE, 1024)
        self.assertEqual(importer.ONE_K_CELL_SIZE, 128)
        self.assertEqual(importer.ONE_K_PRODUCTION_RELATIVE_ROOT.name, "Production1K")

        importer._apply_one_k_mode()
        self.assertEqual(importer.ATLAS_SIZE, 1024)
        self.assertEqual(importer.CELL_SIZE, 128)
        one_k_manifest = {
            "frameCount": 60,
            "canvasSize": 128,
            "fps": 12,
            "atlas": "C:/staging/unit_atlas.png",
            "atlasGrid": {"columns": 8, "rows": 8, "cellWidth": 128, "cellHeight": 128},
        }
        with self.assertRaisesRegex(RuntimeError, "atlas name"):
            importer.validate_manifest("other", one_k_manifest, require_atlas_file=False)

    def test_variant_suffix_renames_sibling_assets_without_touching_originals(self) -> None:
        importer = load_importer()
        entries = importer.discover_animation_assets(PRODUCTION_ROOT)
        hero_idle = next(entry for entry in entries if entry.asset_id == "character_00_hero_idle")
        hero_attack = next(entry for entry in entries if entry.asset_id == "character_00_hero_attack")
        originals = {
            "idle_id": hero_idle.asset_id,
            "idle_texture": hero_idle.texture_name,
            "idle_flipbook": hero_idle.flipbook_name,
            "attack_id": hero_attack.asset_id,
            "attack_texture": hero_attack.texture_name,
            "attack_flipbook": hero_attack.flipbook_name,
        }

        variants = importer._with_variant_suffix([hero_idle, hero_attack], "_2k")
        idle_variant, attack_variant = variants

        self.assertEqual(idle_variant.asset_id, "character_00_hero_2k_idle")
        self.assertEqual(idle_variant.texture_name, "T_character_00_hero_2k_idle_atlas")
        self.assertEqual(idle_variant.flipbook_name, "FB_character_00_hero_2k_idle")
        self.assertEqual(attack_variant.asset_id, "character_00_hero_2k_attack")
        self.assertEqual(attack_variant.texture_name, "T_character_00_hero_2k_attack_atlas")
        self.assertEqual(attack_variant.flipbook_name, "")
        # The 4K originals keep their stable names.
        self.assertEqual(hero_idle.asset_id, originals["idle_id"])
        self.assertEqual(hero_idle.texture_name, originals["idle_texture"])
        self.assertEqual(hero_idle.flipbook_name, originals["idle_flipbook"])
        self.assertEqual(hero_attack.asset_id, originals["attack_id"])
        self.assertEqual(hero_attack.texture_name, originals["attack_texture"])
        self.assertEqual(hero_attack.flipbook_name, originals["attack_flipbook"])

    def test_base_selection_survives_variant_suffix_rename(self) -> None:
        importer = load_importer()
        entries = importer.discover_animation_assets(PRODUCTION_ROOT)
        selected = importer._select_entries(entries, {"character_00_hero_idle"}, 0)
        variants = importer._with_variant_suffix(selected, "_2k")
        self.assertEqual(len(variants), 1)
        self.assertEqual(variants[0].asset_id, "character_00_hero_2k_idle")
        self.assertEqual(variants[0].texture_name, "T_character_00_hero_2k_idle_atlas")


if __name__ == "__main__":
    unittest.main()
