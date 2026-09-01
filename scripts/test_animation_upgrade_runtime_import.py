from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
IMPORTER_PATH = (
    PROJECT_ROOT
    / "Content"
    / "Python"
    / "gamexxk_import_animation_upgrade_20260827.py"
)


def load_importer():
    spec = importlib.util.spec_from_file_location(
        "gamexxk_import_animation_upgrade_20260827", IMPORTER_PATH
    )
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load importer spec: {IMPORTER_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class AnimationUpgradeRuntimeImportTest(unittest.TestCase):
    def test_discovers_all_approved_candidates_except_old_deer_bow(self) -> None:
        importer = load_importer()
        entries = importer.discover_entries(PROJECT_ROOT)
        self.assertEqual(27, len(entries))
        self.assertNotIn("enemy_18_deer_bow_candidate", {entry.candidate_id for entry in entries})
        self.assertTrue(all(entry.source_relative != "怪物们/鹿鞠躬.mov" for entry in entries))

    def test_runtime_targets_cover_corrected_town_battle_and_monster_clips(self) -> None:
        importer = load_importer()
        targets = importer.runtime_target_specs()
        expected = {
            "character_00_hero_combat_idle_candidate": "character_00_hero_2k_idle",
            "character_00_hero_attack_punch_candidate": "character_00_hero_2k_attack_punch",
            "character_00_hero_attack_kick_candidate": "character_00_hero_2k_attack_kick",
            "enemy_01_rooster_idle_candidate": "enemy_01_rooster_2k_idle",
            "enemy_01_rooster_attack_candidate": "enemy_01_rooster_2k_attack",
            "enemy_03_weasel_idle_candidate": "enemy_03_weasel_2k_idle",
            "enemy_03_weasel_attack_candidate": "enemy_03_weasel_2k_attack",
            "enemy_05_ironfeather_idle_candidate": "enemy_05_ironfeather_2k_idle",
            "enemy_07_graywolf_idle_candidate": "enemy_07_graywolf_2k_idle",
            "enemy_11_graymane_attack_candidate": "enemy_11_graymane_2k_attack",
            "enemy_16_toad_idle_candidate": "enemy_16_toad_2k_idle",
            "enemy_18_deer_idle_candidate": "enemy_18_deer_2k_idle",
            "enemy_18_deer_attack_candidate": "enemy_18_deer_2k_attack",
            "candidate_yue_fire_idle": "character_09_yue_bai_2k_idle",
        }
        self.assertEqual(expected, {key: value.asset_id_2k for key, value in targets.items()})
        self.assertTrue(targets["character_00_hero_combat_idle_candidate"].update_idle_flipbook)
        self.assertTrue(targets["enemy_18_deer_idle_candidate"].update_idle_flipbook)
        self.assertFalse(targets["enemy_18_deer_attack_candidate"].update_idle_flipbook)

    def test_every_runtime_target_has_matching_one_k_sibling_and_authored_fps(self) -> None:
        importer = load_importer()
        for candidate_id, target in importer.runtime_target_specs().items():
            self.assertIn("_2k_", target.asset_id_2k, candidate_id)
            self.assertEqual(target.asset_id_2k.replace("_2k_", "_1k_"), target.asset_id_1k)
            self.assertGreater(target.fps, 0.0)
            self.assertLessEqual(target.fps, 240.0)


if __name__ == "__main__":
    unittest.main()
