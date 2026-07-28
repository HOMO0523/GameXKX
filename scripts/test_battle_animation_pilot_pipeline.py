#!/usr/bin/env python3
"""Contract tests for the isolated battle-animation pilot pipeline."""

from __future__ import annotations

import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
IMPORTER = PROJECT_ROOT / "Content/Python/gamexxk_import_battle_animation_pilot.py"
APPLIER = PROJECT_ROOT / "Content/Python/gamexxk_apply_battle_animation_pilot.py"
PROBE = PROJECT_ROOT / "Content/Python/gamexxk_probe_battle_animation_pilot.py"


class BattleAnimationPilotPipelineTests(unittest.TestCase):
    def test_importer_uses_isolated_hero_idle_contract(self) -> None:
        source = IMPORTER.read_text(encoding="utf-8")
        self.assertIn('/Game/GameXXK/Characters/BattleAnimationPilot/Hero/Idle', source)
        self.assertIn('hero_idle_atlas.png', source)
        self.assertRegex(source, r"FRAME_COUNT\s*=\s*60")
        self.assertRegex(source, r"CELL_SIZE\s*=\s*512")
        self.assertRegex(source, r"FRAMES_PER_SECOND\s*=\s*12(?:\.0)?")
        self.assertRegex(source, r"PIXELS_PER_UNREAL_UNIT\s*=\s*2\.5")
        self.assertIn('SpritePivotMode.BOTTOM_CENTER', source)
        self.assertIn('FB_Pilot_Hero_Idle', source)
        self.assertNotRegex(source, r'/Game/GameXXK/Characters/(?:Hero|Enemies)/')

    def test_pie_applier_is_runtime_only_and_targets_hero(self) -> None:
        source = APPLIER.read_text(encoding="utf-8")
        self.assertIn('FB_Pilot_Hero_Idle', source)
        self.assertIn('unreal.load_asset', source)
        self.assertIn('("Hero", "Player")', source)
        self.assertIn('is_enemy_unit()', source)
        self.assertIn('get_battle_visual_component()', source)
        self.assertIn('set_looping(True)', source)
        self.assertIn('play_from_start()', source)
        self.assertIn('get_editor_property("frames_per_second")', source)
        self.assertNotIn('save_loaded_asset', source)
        self.assertNotIn('save_current_level', source)

    def test_probe_reports_registry_and_loader_state(self) -> None:
        source = PROBE.read_text(encoding="utf-8")
        self.assertIn('does_asset_exist', source)
        self.assertIn('EditorAssetLibrary.load_asset', source)
        self.assertIn('unreal.load_asset', source)
        self.assertIn('list_assets', source)
        self.assertIn('production_hero_sprite', source)
        self.assertIn('get_playback_position', source)
        self.assertIn('is_looping', source)


if __name__ == "__main__":
    unittest.main()
