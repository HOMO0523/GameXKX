import tempfile
import unittest
from pathlib import Path

from PIL import Image

from scripts.gamexxk_ui_master_pages import build_page_previews, contain_canvas


class GameXXKUiMasterPageTests(unittest.TestCase):
    def test_contain_canvas_keeps_one_uniform_scale(self):
        placement = contain_canvas((512, 512), (490, 490), (118, 48, 378, 471))
        self.assertEqual(placement.scale_x, placement.scale_y)
        self.assertAlmostEqual(490 / 512, placement.scale_x)
        self.assertAlmostEqual(260 * placement.scale_x, placement.content_width)
        self.assertAlmostEqual(423 * placement.scale_y, placement.content_height)

    def test_builds_exactly_eighteen_full_hd_previews(self):
        with tempfile.TemporaryDirectory() as td:
            output = Path(td)
            records = build_page_previews(output)
            self.assertEqual(18, len(records))
            self.assertEqual("00_公共组件", records[0]["group"])
            self.assertEqual("17_战斗HUD_卡牌选中目标", records[-1]["group"])
            for index, record in enumerate(records):
                with Image.open(output / record["file"]) as image:
                    self.assertEqual((1920, 1080), image.size)
                self.assertTrue(record["imageLayers"])
                if index == 0:
                    self.assertEqual([], record["textLayers"])
                else:
                    self.assertTrue(record["textLayers"])


if __name__ == "__main__":
    unittest.main()
