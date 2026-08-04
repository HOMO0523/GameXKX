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
            required = {
                "button_normal",
                "button_hover",
                "button_pressed",
                "button_primary",
                "button_danger",
                "button_disabled",
                "tab_normal",
                "tab_hover",
                "tab_pressed",
                "tab_selected",
                "tab_disabled",
                "nav_normal",
                "nav_hover",
                "nav_selected",
                "nav_reminder",
                "nav_locked",
                "nav_backpack",
                "nav_companion",
                "nav_codex",
                "nav_task",
                "nav_route",
                "item_slot_empty",
                "item_slot_hover",
                "item_slot_selected",
                "item_slot_locked",
                "equipment_slot_empty",
                "equipment_slot_hover",
                "equipment_slot_selected",
                "card_frame_role",
                "card_frame_monster",
                "card_frame_general",
                "card_frame_terrain",
                "card_frame_rare",
                "card_frame_boss",
                "panel_large",
                "panel_medium",
                "panel_small",
                "progress_track",
                "progress_fill",
                "resource_strip",
                "tooltip_panel",
            }
            self.assertTrue(required.issubset(manifest))
            for record in manifest.values():
                with Image.open(Path(td) / record["file"]) as source:
                    image = source.convert("RGBA")
                self.assertEqual(record["size"], list(image.size))
                self.assertEqual((0, 255), image.getchannel("A").getextrema())
                self.assertFalse(record["textBaked"])

    def test_buttons_do_not_use_large_red_blue_or_green_fills(self):
        with tempfile.TemporaryDirectory() as td:
            manifest = build_component_assets(Path(td))
            keys = (
                "button_normal",
                "button_hover",
                "button_pressed",
                "button_primary",
                "button_danger",
            )
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
