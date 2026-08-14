#!/usr/bin/env python3
"""Pure contracts for the disk-only downscale staging preparation."""

from __future__ import annotations

import json
import shutil
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PREPARE_PATH = PROJECT_ROOT / "scripts/gamexxk_prepare_animation_2k.py"

import importlib.util

spec = importlib.util.spec_from_file_location("gamexxk_prepare_animation_2k", PREPARE_PATH)
prepare = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(prepare)


class PrepareAnimationDownscaleTests(unittest.TestCase):
    def setUp(self) -> None:
        for root in (prepare.staging_root_for(2048), prepare.staging_root_for(1024)):
            if root.exists():
                shutil.rmtree(root)

    def tearDown(self) -> None:
        for root in (prepare.staging_root_for(2048), prepare.staging_root_for(1024)):
            if root.exists():
                shutil.rmtree(root)

    def test_prepares_half_resolution_atlas_and_manifest(self) -> None:
        result = prepare.prepare_asset("character_00_hero_idle", atlas_size=2048)

        self.assertEqual(result["staging_size"], (2048, 2048))
        self.assertEqual(result["cell_size"], 256)
        staged_manifest_path = prepare.staging_root_for(2048) / "character_00_hero_idle" / "manifest.json"
        self.assertTrue(staged_manifest_path.is_file())
        payload = json.loads(staged_manifest_path.read_text(encoding="utf-8"))
        self.assertEqual(payload["canvasSize"], 256)
        self.assertEqual(payload["frameCount"], 60)
        self.assertEqual(payload["atlasGrid"], {
            "columns": 8, "rows": 8, "cellWidth": 256, "cellHeight": 256,
        })
        staged_atlas = prepare.staging_root_for(2048) / "character_00_hero_idle" / "atlas" / "character_00_hero_idle_atlas.png"
        self.assertTrue(staged_atlas.is_file())
        self.assertEqual(prepare.png_size(staged_atlas), (2048, 2048))
        # The 4K production master is untouched.
        production_atlas = prepare.PRODUCTION_ROOT / "character_00_hero_idle" / "atlas" / "character_00_hero_idle_atlas.png"
        self.assertEqual(prepare.png_size(production_atlas), (4096, 4096))

    def test_prepares_quarter_resolution_atlas_and_manifest(self) -> None:
        result = prepare.prepare_asset("enemy_01_rooster_idle", atlas_size=1024)

        self.assertEqual(result["staging_size"], (1024, 1024))
        self.assertEqual(result["cell_size"], 128)
        payload = json.loads(
            (prepare.staging_root_for(1024) / "enemy_01_rooster_idle" / "manifest.json").read_text(encoding="utf-8")
        )
        self.assertEqual(payload["canvasSize"], 128)
        self.assertEqual(payload["atlasGrid"], {
            "columns": 8, "rows": 8, "cellWidth": 128, "cellHeight": 128,
        })

    def test_rejects_missing_asset(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "production manifest missing"):
            prepare.prepare_asset("missing_asset_id")

    def test_rejects_non_grid_aligned_atlas_size(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "positive multiple of 8"):
            prepare.prepare_asset("character_00_hero_idle", atlas_size=1001)

    def test_default_pilot_assets_cover_hero_and_rooster(self) -> None:
        self.assertEqual(
            set(prepare.DEFAULT_ASSET_IDS),
            {"character_00_hero_idle", "character_00_hero_attack", "enemy_01_rooster_idle"},
        )

    def test_discovers_all_production_assets(self) -> None:
        ids = prepare.discover_all_asset_ids()
        self.assertEqual(len(ids), 138)
        self.assertIn("character_00_hero_idle", ids)
        self.assertIn("enemy_21_tiger_boss_idle", ids)
        self.assertIn("impact_ink_generic", ids)


if __name__ == "__main__":
    unittest.main()
