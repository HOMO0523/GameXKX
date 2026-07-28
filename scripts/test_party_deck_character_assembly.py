#!/usr/bin/env python3
"""Static contract for the isolated PartyDeck Paper2D character assembler."""

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
ASSEMBLER = PROJECT_ROOT / "Content" / "Python" / "gamexxk_assemble_party_deck_characters.py"

EXPECTED_TARGET_IDS = (
    "Npc.TusiChief",
    "Npc.SongJinBao",
    "Npc.YueBai",
    "Npc.ZhouGuangZu",
    "Npc.JinGui",
    "Npc.QiongMeiEr",
    "PartnerRole.Blade",
    "PartnerRole.Guard",
    "PartnerRole.Healer",
    "PartnerRole.Hunter",
    "PartnerRole.Sorcerer",
    "PartnerRole.FormationMaster",
)
EXPECTED_DIRECTIONS = (
    "South",
    "SouthWest",
    "West",
    "NorthWest",
    "North",
    "NorthEast",
    "East",
    "SouthEast",
)


def _load_assembler():
    if not ASSEMBLER.is_file():
        raise AssertionError(f"PartyDeck character assembler is missing: {ASSEMBLER}")
    spec = importlib.util.spec_from_file_location("gamexxk_assemble_party_deck_characters", ASSEMBLER)
    if spec is None or spec.loader is None:
        raise AssertionError(f"unable to load PartyDeck character assembler: {ASSEMBLER}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class PartyDeckCharacterAssemblyTests(unittest.TestCase):
    def test_plan_has_twelve_isolated_eight_direction_paper2d_characters(self) -> None:
        assembler = _load_assembler()

        plan = assembler.build_assembly_plan()

        self.assertTrue(plan["ok"])
        self.assertEqual(tuple(plan["directions"]), EXPECTED_DIRECTIONS)
        self.assertEqual(tuple(entry["target_id"] for entry in plan["targets"]), EXPECTED_TARGET_IDS)
        self.assertEqual(len(plan["targets"]), 12)
        self.assertEqual(plan["sprite_count"], 12 * 8 * 7)
        self.assertEqual(plan["flipbook_count"], 12 * 8 * 2)
        self.assertEqual(plan["default_flipbook_count"], 12)

        for target in plan["targets"]:
            target_root = target["asset_root"]
            self.assertTrue(
                target_root.startswith("/Game/GameXXK/Characters/PartyDeckNPC/")
                or target_root.startswith("/Game/GameXXK/Characters/PartyDeckPartners/"),
                target_root,
            )
            self.assertEqual(target["default_idle_flipbook"], f"{target_root}/Flipbooks/{target['flipbook_prefix']}_Idle_South")
            self.assertEqual(tuple(target["directions"]), EXPECTED_DIRECTIONS)
            self.assertEqual(target["walk_frame_count"], 6)
            self.assertEqual(target["idle_frame_count"], 1)
            self.assertEqual(target["sprite_count"], 56)
            self.assertEqual(target["flipbook_count"], 16)

    def test_assembler_only_reuses_the_reviewed_generated_textures_and_never_deletes_assets(self) -> None:
        assembler = _load_assembler()
        source = ASSEMBLER.read_text(encoding="utf-8")
        plan = assembler.build_assembly_plan()

        self.assertEqual(plan["texture_root"], "/Game/GameXXK/Sprites/Generated/PartyDeck")
        texture_paths = {
            target["idle_texture"]
            for target in plan["targets"]
        } | {
            target["walk_texture"]
            for target in plan["targets"]
        }
        self.assertIn("/Game/GameXXK/Sprites/Generated/PartyDeck/T_PartyDeck_Npc_TusiChief_Idle8Dir", texture_paths)
        self.assertIn("/Game/GameXXK/Sprites/Generated/PartyDeck/T_PartyDeck_PartnerRole_FormationMaster_Walk8Dir", texture_paths)
        self.assertIn("unreal.PaperSpriteFactory", source)
        self.assertIn("unreal.PaperFlipbookFactory", source)
        self.assertIn("unreal.PaperFlipbookKeyFrame", source)
        self.assertNotIn("delete_asset(", source)
        self.assertNotIn("delete_directory(", source)
        self.assertNotIn("AssetImportTask", source)


if __name__ == "__main__":
    unittest.main()
