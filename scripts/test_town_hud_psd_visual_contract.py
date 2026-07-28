"""Source-level contract for the Town HUD Codex PSD visual shell.

This is intentionally pure Python: it verifies the only-missing hero-detail import source
and the C++ widget's locked resource choices without needing a running UE editor.
"""

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import types
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
IMPORTER_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_import_town_ui_assets.py"
TOWN_HUD_SOURCE = PROJECT_ROOT / "Source" / "GameXXK" / "Private" / "UI" / "GameXXKTownHudWidget.cpp"
TOWN_HUD_HEADER = PROJECT_ROOT / "Source" / "GameXXK" / "Public" / "UI" / "GameXXKTownHudWidget.h"


def load_importer_module():
    # The actual importer runs inside UE.  Its data/source contract must remain testable in CI.
    sys.modules.setdefault("unreal", types.ModuleType("unreal"))
    spec = importlib.util.spec_from_file_location("gamexxk_import_town_ui_assets", IMPORTER_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load town UI importer: {IMPORTER_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class TownHudPsdVisualContractTests(unittest.TestCase):
    def test_hero_detail_036_is_a_hash_locked_missing_only_import(self) -> None:
        importer = load_importer_module()

        metadata = importer.verify_missing_hero_detail_source()

        self.assertEqual(metadata["name"], "036.png")
        self.assertEqual(metadata["width"], 454)
        self.assertEqual(metadata["height"], 908)
        self.assertEqual(
            metadata["sha256"],
            "a93b2f20e6702e13a997831b2d40679e34d9f81d7861afa0af4aa01455002789",
        )
        self.assertEqual(metadata["asset_path"], "/Game/GameXXK/UI/Town/Textures/Character/T_TownCharacter_HeroDetail036")
        self.assertIn("T_TownCharacter_HeroDetail036", importer.MISSING_ONLY_IMPORT_ASSETS)
        self.assertIn(
            ("Character", "hero_detail_036.png", "T_TownCharacter_HeroDetail036"),
            importer.IMPORTS,
        )
        self.assertEqual(
            importer.resolve_import_source("Character", "hero_detail_036.png", "T_TownCharacter_HeroDetail036").name,
            "036.png",
        )
        importer_source = IMPORTER_PATH.read_text(encoding="utf-8")
        self.assertIn("unreal.EditorAssetLibrary.does_asset_exist(asset_path)", importer_source)
        self.assertIn("task.replace_existing = asset_name not in MISSING_ONLY_IMPORT_ASSETS", importer_source)
        self.assertIn("task.replace_existing_settings = asset_name not in MISSING_ONLY_IMPORT_ASSETS", importer_source)

    def test_town_hud_uses_only_the_approved_057_card_face_and_psd_containers(self) -> None:
        source = TOWN_HUD_SOURCE.read_text(encoding="utf-8")
        header = TOWN_HUD_HEADER.read_text(encoding="utf-8")

        self.assertIn("int32 CodexColumnCount = 6;", header)
        self.assertIn("FVector2D CodexCardSize = FVector2D(113.0f, 129.0f);", header)
        self.assertGreaterEqual(
            source.count("CardButton->SetStyle(MakeTextureButtonStyle(PartyDeckCardFrameTexturePath, CodexCardSize));"),
            2,
        )
        self.assertIn("T_TownBackpack_WindowFrame", source)
        self.assertIn("T_TownBackpack_Header", source)
        self.assertIn("T_TownBackpack_ActionBlank", source)
        self.assertIn("TaskNpcCodexDetailPortraitSlot->SetBrush(MakeBoxTextureBrush(BackpackSlotTexturePath", source)
        self.assertIn("T_TownCharacter_HeroDetail036", source)
        self.assertNotIn("TaskNpcCodexSlotTexturePath", source)
        self.assertNotIn("CardBorder->SetBrushColor", source)
        self.assertNotIn("060.png", source)
        self.assertNotIn("061.png", source)
        self.assertNotIn("062.png", source)


if __name__ == "__main__":
    unittest.main()
