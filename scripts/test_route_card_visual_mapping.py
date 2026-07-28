#!/usr/bin/env python3
"""Static contract for category-driven route-card portrait presentation."""

from __future__ import annotations

import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BOARD_SOURCE = PROJECT_ROOT / "Source" / "GameXXK" / "Private" / "UI" / "GameXXKBattleBoardWidget.cpp"
BOARD_TEST_SOURCE = PROJECT_ROOT / "Source" / "GameXXK" / "Private" / "Tests" / "GameXXKCardBattleBoardWidgetTest.cpp"


def _portrait_resolver_source() -> str:
    source = BOARD_SOURCE.read_text(encoding="utf-8")
    start = source.index("FString UGameXXKBattleBoardWidget::ResolveCardPortraitResourcePath")
    end = source.index("UTexture2D* UGameXXKBattleBoardWidget::ResolveCardPortraitTexture", start)
    return source[start:end]


class RouteCardVisualMappingTests(unittest.TestCase):
    def test_route_portraits_are_mapped_only_by_acquisition_category(self) -> None:
        resolver = _portrait_resolver_source()

        self.assertIn("Definition.Owner == EGameXXKCardOwner::Route", resolver)
        self.assertIn("Definition.AcquisitionKey.ToString()", resolver)
        self.assertIn('AcquisitionKey == TEXT("Route.General")', resolver)
        self.assertIn('AcquisitionKey == TEXT("Route.Terrain")', resolver)
        self.assertIn('AcquisitionKey == TEXT("Route.Rare")', resolver)
        self.assertIn('AcquisitionKey.StartsWith(TEXT("Route.Boss."))', resolver)
        self.assertEqual(set(re.findall(r'TEXT\("(Route\.[^"]+)"\)', resolver)), {
            "Route.General",
            "Route.Terrain",
            "Route.Rare",
            "Route.Boss.",
        })
        self.assertNotIn("Definition.Id", resolver)
        self.assertNotIn("060", resolver)
        self.assertNotIn("061", resolver)
        self.assertNotIn("062", resolver)

    def test_all_four_category_assets_are_the_card_art_paths(self) -> None:
        source = BOARD_SOURCE.read_text(encoding="utf-8")
        expected = {
            "General": "/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_General.T_CardPortrait_Route_General",
            "Terrain": "/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_Terrain.T_CardPortrait_Route_Terrain",
            "Rare": "/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_Rare.T_CardPortrait_Route_Rare",
            "Boss": "/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_Boss.T_CardPortrait_Route_Boss",
        }
        for category, asset_path in expected.items():
            self.assertIn(f"Route{category}CardPortraitTexturePath", source)
            self.assertIn(asset_path, source)

    def test_cpp_automation_covers_every_route_definition(self) -> None:
        source = BOARD_TEST_SOURCE.read_text(encoding="utf-8")
        self.assertIn("for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())", source)
        self.assertIn("Definition.Owner != EGameXXKCardOwner::Route", source)
        self.assertIn("RoutePortraitPath.IsEmpty()", source)
        self.assertIn("RouteDefinitionCount, 30", source)


if __name__ == "__main__":
    unittest.main()
