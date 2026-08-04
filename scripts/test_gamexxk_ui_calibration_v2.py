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

from scripts.build_gamexxk_ui_calibration_v2 import preview_text_specs


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


if __name__ == "__main__":
    unittest.main()
