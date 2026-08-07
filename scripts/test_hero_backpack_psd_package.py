"""Regression tests for the GameXXK Hero/Backpack PSD candidate package."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PACKAGE_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "PSD" / "gamexxk-v3" / "hero-backpack"
SOURCE_LOCK = PACKAGE_ROOT / "source-lock.json"
SCREEN_SPEC = PACKAGE_ROOT / "screen-spec.json"
MANIFEST = PACKAGE_ROOT / "manifest.json"
SEMANTIC_MAP = PACKAGE_ROOT / "semantic-map.json"
PREVIEW = PACKAGE_ROOT / "Previews" / "GameXXK_HeroBackpack_V1.png"
BUILDER = PROJECT_ROOT / "scripts" / "build_hero_backpack_psd_package.py"


def image_size(path: Path) -> tuple[int, int]:
    with Image.open(path) as image:
        return image.size


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class HeroBackpackPsdPackageTests(unittest.TestCase):
    def test_source_lock_uses_runtime_idle_frame(self) -> None:
        source_lock = json.loads(SOURCE_LOCK.read_text(encoding="utf-8"))
        source_path = str(source_lock["heroIdleSource"])
        absolute_source = PROJECT_ROOT / source_path

        self.assertEqual(
            "SourceAssets/AnimationProcessing/Production/character_00_hero_idle/frames/frame_0000.png",
            source_path,
        )
        self.assertEqual((512, 512), image_size(absolute_source))
        self.assertEqual(sha256(absolute_source), source_lock["heroIdleSha256"])
        self.assertEqual("T_character_00_hero_idle_atlas", source_lock["runtimeAtlas"])
        self.assertNotIn("PartyDeck/card-portraits", source_path)
        self.assertIn(
            "SourceAssets/PartyDeck/card-portraits/generated",
            source_lock["retiredSourceRoots"],
        )

    def test_screen_contract_is_full_hd_with_named_groups(self) -> None:
        screen_spec = json.loads(SCREEN_SPEC.read_text(encoding="utf-8"))

        self.assertEqual({"width": 1920, "height": 1080, "resolution": 72}, screen_spec["canvas"])
        self.assertEqual(
            [
                "00_Reference",
                "10_WorldContext",
                "20_Shell",
                "30_Hero",
                "40_Equipment",
                "50_Inventory",
                "60_Detail",
                "70_RuntimeText",
            ],
            screen_spec["requiredGroups"],
        )
        self.assertTrue(screen_spec["outputPsd"].startswith("outputs/UI_PSD/Candidates/"))

    def test_generated_manifest_contains_complete_editable_screen(self) -> None:
        screen_spec = json.loads(SCREEN_SPEC.read_text(encoding="utf-8"))
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        image_layers = manifest["imageLayers"]
        text_layers = manifest["textLayers"]
        groups = {layer["group"] for layer in image_layers + text_layers}

        self.assertEqual(1920, manifest["document"]["width"])
        self.assertEqual(1080, manifest["document"]["height"])
        self.assertEqual(set(screen_spec["requiredGroups"]), groups)
        self.assertEqual(6, sum(layer["name"].startswith("equipment_slot_") for layer in image_layers))
        self.assertEqual(20, sum(layer["name"].startswith("inventory_slot_") for layer in image_layers))
        self.assertGreaterEqual(len(text_layers), 22)
        self.assertTrue(all("文字" not in layer["name"] for layer in image_layers))
        self.assertTrue(all((PACKAGE_ROOT / layer["path"]).is_file() for layer in image_layers))

    def test_semantic_map_has_runtime_slices_and_button_families(self) -> None:
        semantic_map = json.loads(SEMANTIC_MAP.read_text(encoding="utf-8"))
        semantics = {
            asset["buttonSemantic"]
            for asset in semantic_map["assets"]
            if asset.get("buttonSemantic")
        }
        runtime_assets = semantic_map["runtimeAssets"]

        self.assertEqual({"neutral", "primary", "destructive"}, semantics)
        self.assertFalse(semantic_map["textBaked"])
        self.assertGreaterEqual(len(runtime_assets), 8)
        self.assertTrue(all((PACKAGE_ROOT / record["file"]).is_file() for record in runtime_assets))

    def test_preview_is_full_hd(self) -> None:
        self.assertEqual((1920, 1080), image_size(PREVIEW))

    def test_detail_and_inventory_actions_do_not_overlap(self) -> None:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        layers = {layer["name"]: layer for layer in manifest["imageLayers"]}
        detail = layers["item_detail_panel"]
        use = layers["button_use"]
        sort = layers["button_sort"]
        destructive = layers["button_disassemble"]

        self.assertGreaterEqual(use["x"], detail["x"])
        self.assertGreaterEqual(use["y"], detail["y"])
        self.assertLessEqual(use["x"] + use["width"], detail["x"] + detail["width"])
        self.assertLessEqual(use["y"] + use["height"], detail["y"] + detail["height"])
        self.assertGreaterEqual(sort["y"], detail["y"] + detail["height"] + 8)
        self.assertGreaterEqual(destructive["y"], detail["y"] + detail["height"] + 8)
        self.assertLessEqual(max(sort["y"] + sort["height"], destructive["y"] + destructive["height"]), 928)

    def test_builder_cli_generates_an_isolated_candidate(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_root = Path(temporary_directory) / "hero-backpack"
            result = subprocess.run(
                [sys.executable, str(BUILDER), "--output-root", str(output_root)],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertEqual(0, result.returncode, result.stderr)
            report = json.loads(result.stdout)
            self.assertTrue(report["ok"])
            self.assertEqual([1920, 1080], report["canvas"])
            self.assertTrue((output_root / "manifest.json").is_file())
            self.assertTrue((output_root / "semantic-map.json").is_file())
            self.assertEqual(
                (1920, 1080),
                image_size(output_root / "Previews" / "GameXXK_HeroBackpack_V1.png"),
            )


if __name__ == "__main__":
    unittest.main()
