"""Regression tests for the GameXXK Hero/Backpack V2 calibration package."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image

from scripts.build_gamexxk_ui_calibration_v2 import (
    preview_text_specs,
    split_transparent_icon_sheet,
)


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PACKAGE_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "PSD" / "gamexxk-v4" / "calibration-v2"
SPEC = PACKAGE_ROOT / "calibration-spec.json"
SOURCE_LOCK = PACKAGE_ROOT / "source-lock.json"
BUILDER = PROJECT_ROOT / "scripts" / "build_gamexxk_ui_calibration_v2.py"


def load_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def image_size(path: Path) -> tuple[int, int]:
    with Image.open(path) as image:
        return image.size


def run_builder(output_root: Path) -> dict[str, object]:
    result = subprocess.run(
        [sys.executable, str(BUILDER), "--output-root", str(output_root)],
        cwd=PROJECT_ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise AssertionError(result.stderr or result.stdout)
    return json.loads(result.stdout)


class GameXXKUiCalibrationV2Tests(unittest.TestCase):
    def test_split_transparent_icon_sheet_exports_square_high_fill_icons(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source = root / "sheet.png"
            output = root / "icons"
            sheet = Image.new("RGBA", (900, 300), (0, 0, 0, 0))
            for index, color in enumerate(((180, 40, 30, 255), (30, 150, 90, 255), (40, 70, 170, 255))):
                left = index * 300 + 48
                sheet.paste(Image.new("RGBA", (204, 246), color), (left, 27))
            sheet.save(source)

            paths = split_transparent_icon_sheet(
                source,
                ["stone", "sand", "seal"],
                output,
                canvas_size=(512, 512),
                subject_fill=0.90,
            )

            self.assertEqual(["stone.png", "sand.png", "seal.png"], [path.name for path in paths])
            for path in paths:
                with Image.open(path) as opened:
                    icon = opened.convert("RGBA")
                self.assertEqual((512, 512), icon.size)
                self.assertEqual(0, icon.getpixel((0, 0))[3])
                bounds = icon.getchannel("A").getbbox()
                self.assertIsNotNone(bounds)
                assert bounds is not None
                self.assertGreaterEqual(max(bounds[2] - bounds[0], bounds[3] - bounds[1]), 455)
                self.assertLessEqual(max(bounds[2] - bounds[0], bounds[3] - bounds[1]), 462)

    def test_split_transparent_icon_sheet_discards_neighbor_cell_fragments(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            source = root / "sheet.png"
            sheet = Image.new("RGBA", (600, 300), (0, 0, 0, 0))
            sheet.paste(Image.new("RGBA", (180, 220), (120, 80, 40, 255)), (50, 40))
            sheet.paste(Image.new("RGBA", (8, 90), (20, 150, 80, 255)), (292, 110))
            sheet.paste(Image.new("RGBA", (180, 220), (20, 150, 80, 255)), (360, 40))
            sheet.save(source)

            first, _ = split_transparent_icon_sheet(
                source,
                ["first", "second"],
                root / "icons",
            )
            with Image.open(first) as opened:
                alpha = opened.convert("RGBA").getchannel("A")
            visible = {
                (x, y)
                for y in range(alpha.height)
                for x in range(alpha.width)
                if alpha.getpixel((x, y)) > 36
            }
            components = 0
            while visible:
                components += 1
                frontier = [visible.pop()]
                while frontier:
                    x, y = frontier.pop()
                    for neighbor in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                        if neighbor in visible:
                            visible.remove(neighbor)
                            frontier.append(neighbor)
            self.assertEqual(1, components)

    def test_contract_locks_reference_and_final_idle(self) -> None:
        spec = load_json(SPEC)
        source_lock = load_json(SOURCE_LOCK)

        self.assertEqual({"width": 1920, "height": 1080}, spec["canvas"])
        self.assertEqual("GameXXK_HeroBackpack_V2", spec["candidateName"])
        self.assertEqual(
            "Generated/hero_backpack_textless_base_clean.png",
            spec["generatedBase"],
        )
        self.assertEqual(
            "Generated/hero_backpack_ui_shell_no_icons.png",
            spec["uiShellNoIcons"],
        )
        self.assertEqual(
            "Generated/town_background_clean_no_ui.png",
            spec["townBackgroundCleanNoUi"],
        )
        self.assertEqual((1672, 941), image_size(PACKAGE_ROOT / spec["uiShellNoIcons"]))
        self.assertEqual((1672, 941), image_size(PACKAGE_ROOT / spec["townBackgroundCleanNoUi"]))
        self.assertEqual(
            {
                "x": 403,
                "y": 216,
                "width": 612,
                "height": 612,
                "fitMode": "contain_canvas",
            },
            spec["heroPlacement"],
        )
        self.assertEqual(
            "SourceArt/UI/PSD/gamexxk-v4/ui-master/Reference/approved_town_hero_backpack.png",
            source_lock["approvedReference"]["path"],
        )
        self.assertEqual(
            "SourceAssets/AnimationProcessing/Production/character_00_hero_idle/frames/frame_0000.png",
            source_lock["heroIdle"]["path"],
        )

        for record in source_lock.values():
            source = PROJECT_ROOT / record["path"]
            self.assertEqual(record["sha256"], sha256(source))
            self.assertEqual(tuple(record["dimensions"]), image_size(source))

    def test_builder_preserves_hero_canvas_aspect(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_root = Path(temporary_directory) / "calibration-v2"
            report = run_builder(output_root)

            self.assertEqual([1920, 1080], report["canvas"])
            self.assertEqual(1.0, report["heroScaleRatioXToY"])
            self.assertEqual([512, 512], report["heroSourceCanvas"])
            self.assertEqual([1920, 1080], list(image_size(Path(report["preview"]))))
            self.assertEqual([1920, 1080], list(image_size(Path(report["comparison"]))))

    def test_builder_never_reads_rejected_procedural_assets(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_root = Path(temporary_directory) / "calibration-v2"
            report = run_builder(output_root)

            consumed = "\n".join(report["consumedSources"])
            self.assertNotIn("ui-master/Assets", consumed)
            self.assertNotIn("ui-master/LayoutAssets", consumed)
            self.assertIn("approved_town_hero_backpack.png", consumed)
            self.assertIn("hero_backpack_textless_base_clean.png", consumed)
            self.assertIn("character_00_hero_idle", consumed)

    def test_resource_and_stat_values_clear_their_icons(self) -> None:
        specs = {record["name"]: record for record in preview_text_specs()}

        self.assertGreaterEqual(specs["resource_coin"]["x"], 1265)
        self.assertGreaterEqual(specs["resource_jade"]["x"], 1510)
        self.assertGreaterEqual(specs["resource_gold"]["x"], 1785)
        self.assertGreaterEqual(specs["stat_attack"]["x"], 545)
        self.assertGreaterEqual(specs["stat_health"]["x"], 740)
        self.assertGreaterEqual(specs["stat_defense"]["x"], 920)

    def test_contract_separates_large_panel_from_every_small_control(self) -> None:
        spec = load_json(SPEC)

        self.assertEqual(
            "Generated/hero_backpack_large_panel_clean.png",
            spec["largePanelClean"],
        )
        self.assertEqual("Components", spec["componentOutputDir"])
        self.assertEqual(
            "Review/GameXXK_HeroBackpack_V2_components.png",
            spec["componentSheet"],
        )

        crops = spec["componentCrops"]
        self.assertEqual(28, len(crops))
        self.assertEqual(
            {"tab": 5, "equipment_slot": 6, "inventory_slot": 16, "detail_slot": 1},
            {
                role: sum(1 for crop in crops if crop["role"] == role)
                for role in {crop["role"] for crop in crops}
            },
        )
        names = [crop["name"] for crop in crops]
        self.assertEqual(len(names), len(set(names)))
        for crop in crops:
            left, top, right, bottom = crop["box"]
            self.assertLess(left, right)
            self.assertLess(top, bottom)
            self.assertGreaterEqual(left, 0)
            self.assertGreaterEqual(top, 0)
            self.assertLessEqual(right, 1672)
            self.assertLessEqual(bottom, 941)

    def test_builder_exports_every_small_control_as_a_separate_asset(self) -> None:
        spec = load_json(SPEC)
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_root = Path(temporary_directory) / "calibration-v2"
            report = run_builder(output_root)

            component_paths = [Path(path) for path in report["componentAssets"]]
            self.assertEqual(28, len(component_paths))
            self.assertTrue(all(path.is_file() for path in component_paths))
            self.assertEqual(
                {crop["name"] for crop in spec["componentCrops"]},
                {path.stem for path in component_paths},
            )
            for path in component_paths:
                with Image.open(path) as component:
                    self.assertEqual("RGBA", component.mode)
                    self.assertEqual(0, component.getpixel((0, 0))[3])
                    self.assertGreater(
                        component.getpixel((component.width // 2, component.height // 2))[3],
                        240,
                    )
                    ink = component.convert("L").point(lambda value: 255 if value < 145 else 0)
                    ink_bounds = ink.getbbox()
                    self.assertIsNotNone(ink_bounds)
                    left, top, right, bottom = ink_bounds
                    self.assertLessEqual(
                        max(left, top, component.width - right, component.height - bottom),
                        12,
                    )
            self.assertEqual(
                [1672, 941],
                list(image_size(Path(report["largePanelClean"]))),
            )
            self.assertEqual(
                [1920, 1080],
                list(image_size(Path(report["componentSheet"]))),
            )

    def test_builder_records_recomposable_component_placements(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_root = Path(temporary_directory) / "calibration-v2"
            report = run_builder(output_root)
            layout = load_json(Path(report["componentLayout"]))

            self.assertEqual([1672, 941], layout["sourceCanvas"])
            self.assertEqual(28, len(layout["components"]))
            self.assertEqual(
                28,
                len({record["name"] for record in layout["components"]}),
            )
            for record in layout["components"]:
                self.assertEqual(4, len(record["sourcePlacement"]))
                self.assertEqual(
                    [
                        record["sourcePlacement"][2] - record["sourcePlacement"][0],
                        record["sourcePlacement"][3] - record["sourcePlacement"][1],
                    ],
                    record["size"],
                )

    def test_builder_exports_separate_master_content_layers(self) -> None:
        base_expected = {
            "nav_scroll",
            "nav_backpack",
            "nav_codex",
            "nav_companion",
            "nav_route",
            "resource_coin",
            "resource_jade",
            "resource_gold",
            "stat_attack",
            "stat_health",
            "stat_defense",
            "category_selected_ink",
            "hero_portrait",
        }
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_root = Path(temporary_directory) / "calibration-v2"
            report = run_builder(output_root)
            manifest = load_json(Path(report["contentManifest"]))
            spec = load_json(SPEC)
            approved_expected = {
                *(f"{set_name}_{slot}" for slot in spec["equipmentSlotOrder"] for set_name in spec["equipmentSetOrder"]),
                *(f"starter_{slot}" for slot in spec["equipmentSlotOrder"]),
                *spec["coreItemOrder"],
            }

            self.assertEqual(
                base_expected | approved_expected,
                {record["name"] for record in manifest["content"]},
            )
            self.assertEqual(58, len(report["contentAssets"]))
            self.assertEqual(45, report["approvedContentCount"])
            for asset in map(Path, report["contentAssets"]):
                self.assertTrue(asset.is_file())
                with Image.open(asset) as opened:
                    image = opened.convert("RGBA")
                self.assertEqual(0, image.getpixel((0, 0))[3])
                self.assertIsNotNone(image.getchannel("A").getbbox())

    def test_builder_uses_approved_generated_navigation_icons(self) -> None:
        spec = load_json(SPEC)
        source_lock = load_json(SOURCE_LOCK)

        self.assertEqual(
            "Generated/nav_icons_simplified_approved_alpha.png",
            spec["generatedNavIcons"],
        )
        self.assertEqual(
            "SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Generated/"
            "nav_icons_simplified_approved_alpha.png",
            source_lock["generatedNavIcons"]["path"],
        )

        with tempfile.TemporaryDirectory() as temporary_directory:
            output_root = Path(temporary_directory) / "calibration-v2"
            report = run_builder(output_root)
            manifest = load_json(Path(report["contentManifest"]))
            navigation = {
                record["name"]: record
                for record in manifest["content"]
                if record.get("role") == "navigation"
            }

            self.assertEqual(
                {
                    "nav_scroll",
                    "nav_backpack",
                    "nav_codex",
                    "nav_companion",
                    "nav_route",
                },
                set(navigation),
            )
            for record in navigation.values():
                self.assertEqual("generatedNavIcons", record["source"])
                asset = output_root / record["file"]
                with Image.open(asset) as opened:
                    image = opened.convert("RGBA")
                self.assertEqual((256, 256), image.size)
                self.assertEqual(0, image.getpixel((0, 0))[3])
                bounds = image.getchannel("A").getbbox()
                self.assertIsNotNone(bounds)
                left, top, right, bottom = bounds
                self.assertGreater((right - left) * (bottom - top), 256 * 256 * 0.28)
                magenta_spill = sum(
                    1
                    for red, green, blue, alpha in image.getdata()
                    if alpha > 16 and red > 180 and blue > 180 and green < 80
                )
                self.assertEqual(0, magenta_spill)

                alpha = image.getchannel("A")
                visible = {
                    (x, y)
                    for y in range(image.height)
                    for x in range(image.width)
                    if alpha.getpixel((x, y)) > 32
                }
                components = 0
                while visible:
                    components += 1
                    frontier = [visible.pop()]
                    while frontier:
                        x, y = frontier.pop()
                        for neighbor in (
                            (x - 1, y),
                            (x + 1, y),
                            (x, y - 1),
                            (x, y + 1),
                        ):
                            if neighbor in visible:
                                visible.remove(neighbor)
                                frontier.append(neighbor)
                self.assertEqual(1, components, record["name"])

    def test_builder_exports_approved_equipment_and_core_items(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_root = Path(temporary_directory) / "calibration-v2"
            report = run_builder(output_root)
            manifest = load_json(Path(report["contentManifest"]))
            approved_records = [
                record
                for record in manifest["content"]
                if "category" in record
            ]

            self.assertEqual(
                {"set_equipment": 36, "starter_equipment": 6, "core_item": 3},
                manifest["approvedCategoryCounts"],
            )
            self.assertEqual(45, len(approved_records))
            self.assertEqual(45, len({record["name"] for record in approved_records}))
            self.assertTrue(Path(report["approvedContentReview"]).is_file())
            rejected_old_names = {
                "equipment_left_01",
                "equipment_left_02",
                "equipment_left_03",
                "equipment_right_01",
                "equipment_right_02",
                "equipment_right_03",
                "inventory_sword",
                "inventory_bag",
                "inventory_crystal",
                "inventory_jade",
                "detail_bag",
            }
            self.assertTrue(rejected_old_names.isdisjoint({record["name"] for record in manifest["content"]}))
            for record in approved_records:
                asset = output_root / record["file"]
                with Image.open(asset) as opened:
                    image = opened.convert("RGBA")
                self.assertEqual((512, 512), image.size, record["name"])
                alpha = image.getchannel("A")
                self.assertTrue(
                    all(
                        alpha.getpixel((x, y)) == 0
                        for x, y in (
                            *((x, 0) for x in range(image.width)),
                            *((x, image.height - 1) for x in range(image.width)),
                            *((0, y) for y in range(image.height)),
                            *((image.width - 1, y) for y in range(image.height)),
                        )
                    ),
                    record["name"],
                )
                self.assertIsNotNone(alpha.getbbox(), record["name"])
                magenta_spill = sum(
                    1
                    for red, green, blue, pixel_alpha in image.getdata()
                    if pixel_alpha > 16 and red > 180 and blue > 180 and green < 100
                )
                self.assertEqual(0, magenta_spill, record["name"])


if __name__ == "__main__":
    unittest.main()
