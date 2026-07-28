"""Regression tests for the semantic color assignment of battle resource bars.

Run from the GameXXK project root:
    python scripts/test_battle_resource_psd_cuts.py
"""

from __future__ import annotations

import unittest
from pathlib import Path

from PIL import Image


RESOURCE_ROOT = Path(__file__).resolve().parents[1] / "SourceArt" / "UI" / "Battle" / "ResourceBars"


def mean_saturated_visible_rgb(path: Path) -> tuple[float, float, float]:
    """Return the mean color of the painted fill pixels, excluding parchment/alpha."""
    image = Image.open(path).convert("RGBA")
    pixels = [
        pixel
        for pixel in image.get_flattened_data()
        if pixel[3] > 32 and max(pixel[:3]) - min(pixel[:3]) >= 16
    ]
    if not pixels:
        raise AssertionError(f"No visible colored resource pixels found in {path}")
    return tuple(sum(pixel[channel] for pixel in pixels) / len(pixels) for channel in range(3))


class BattleResourcePsdCutsTest(unittest.TestCase):
    def assert_red_dominant(self, path: Path) -> None:
        red, green, _blue = mean_saturated_visible_rgb(path)
        self.assertGreater(
            red,
            green + 16.0,
            f"{path.name} must be a red-dominant health resource, got RGB ({red:.1f}, {green:.1f})",
        )

    def assert_green_dominant(self, path: Path) -> None:
        red, green, _blue = mean_saturated_visible_rgb(path)
        self.assertGreater(
            green,
            red + 16.0,
            f"{path.name} must be a green-dominant mana resource, got RGB ({red:.1f}, {green:.1f})",
        )

    def test_health_assets_are_red_and_mana_assets_are_green(self) -> None:
        self.assert_red_dominant(RESOURCE_ROOT / "battle_psd_health_fill.png")
        self.assert_red_dominant(RESOURCE_ROOT / "battle_psd_health_full.png")
        self.assert_green_dominant(RESOURCE_ROOT / "battle_psd_mana_fill.png")
        self.assert_green_dominant(RESOURCE_ROOT / "battle_psd_mana_full.png")


if __name__ == "__main__":
    unittest.main()
