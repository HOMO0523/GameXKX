from __future__ import annotations

import unittest
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "DesktopOverlay"
ASSETS = (
    SOURCE_ROOT / "T_DesktopTownEnterButton.png",
    SOURCE_ROOT / "T_DesktopTownExitButton.png",
)


class DesktopTownToggleAssetTests(unittest.TestCase):
    def test_approved_buttons_are_square_real_alpha_ui_sources(self) -> None:
        for path in ASSETS:
            with self.subTest(path=path.name), Image.open(path) as opened:
                image = opened.convert("RGBA")
                self.assertEqual((512, 512), image.size)
                alpha = image.getchannel("A")
                self.assertEqual((0, 255), alpha.getextrema())
                corners = (
                    alpha.getpixel((0, 0)),
                    alpha.getpixel((511, 0)),
                    alpha.getpixel((0, 511)),
                    alpha.getpixel((511, 511)),
                )
                self.assertTrue(all(value <= 2 for value in corners), corners)
                values = list(alpha.getdata())
                visible_ratio = sum(value > 8 for value in values) / len(values)
                soft_ratio = sum(0 < value < 255 for value in values) / len(values)
                self.assertGreater(visible_ratio, 0.35)
                self.assertLess(visible_ratio, 0.55)
                self.assertGreater(soft_ratio, 0.01)


if __name__ == "__main__":
    unittest.main()
