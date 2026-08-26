import colorsys
import tempfile
import unittest
from pathlib import Path

from PIL import Image

from scripts.gamexxk_ui_master_assets import build_component_assets


class GameXXKUiMasterAssetTests(unittest.TestCase):
    def test_builds_required_text_free_component_states(self):
        with tempfile.TemporaryDirectory() as td:
            manifest = build_component_assets(Path(td))
            self.assertEqual({"button_purchase"}, set(manifest))
            for record in manifest.values():
                with Image.open(Path(td) / record["file"]) as source:
                    image = source.convert("RGBA")
                self.assertEqual(record["size"], list(image.size))
                self.assertEqual((0, 255), image.getchannel("A").getextrema())
                self.assertFalse(record["textBaked"])

    def test_buttons_do_not_use_large_red_blue_or_green_fills(self):
        with tempfile.TemporaryDirectory() as td:
            manifest = build_component_assets(Path(td))
            keys = ("button_purchase",)
            for key in keys:
                with Image.open(Path(td) / manifest[key]["file"]) as source:
                    image = source.convert("RGBA")
                vivid = 0
                opaque = 0
                for r, g, b, a in image.getdata():
                    if a < 128:
                        continue
                    opaque += 1
                    _, saturation, _ = colorsys.rgb_to_hsv(
                        r / 255, g / 255, b / 255
                    )
                    vivid += saturation > 0.42
                self.assertLess(vivid / max(opaque, 1), 0.06, key)


if __name__ == "__main__":
    unittest.main()
