"""Regression checks for the generated, number-safe Party Qi icon source."""

from __future__ import annotations

from pathlib import Path
import unittest

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT_ROOT / "SourceArt" / "UI" / "Battle" / "PartyQi" / "battle_party_qi_soul_orb_v1.png"


class BattlePartyQiIconTests(unittest.TestCase):
    def test_icon_keeps_a_transparent_background_and_paper_number_field(self) -> None:
        self.assertTrue(SOURCE.is_file(), f"missing generated Party Qi icon: {SOURCE}")
        image = Image.open(SOURCE).convert("RGBA")
        self.assertEqual((1254, 1254), image.size)
        self.assertEqual(0, image.getpixel((0, 0))[3], "chroma-key background must be transparent")

        center = image.getpixel((image.width // 2, image.height // 2))
        self.assertGreaterEqual(center[3], 245, "dynamic number field must be opaque")
        self.assertGreater(center[0], 190, "dynamic number field must remain warm paper, not dark ink")
        self.assertGreater(center[1], 180, "dynamic number field must remain warm paper, not dark ink")
        self.assertGreater(center[2], 155, "dynamic number field must remain warm paper, not dark ink")

        visible_pixels = [pixel for pixel in image.get_flattened_data() if pixel[3] > 32]
        magenta_visible = [
            pixel for pixel in visible_pixels
            if pixel[0] > 210 and pixel[2] > 160 and pixel[1] < 80
        ]
        self.assertFalse(magenta_visible, "no chroma-key magenta may survive inside the icon")


if __name__ == "__main__":
    unittest.main()
