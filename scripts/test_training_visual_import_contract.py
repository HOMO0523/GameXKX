#!/usr/bin/env python3
"""Contract checks for the desktop-training visual MVP runtime import."""

from __future__ import annotations

import hashlib
import json
import unittest
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
PROCESSING_ROOT = ROOT / "SourceAssets/AnimationProcessing/walkloop_pilot_v1/character_00_hero_walk_left"
IMPORT_MANIFEST = PROCESSING_ROOT / "runtime-import-manifest.json"
SOURCE_MANIFEST = PROCESSING_ROOT / "manifest.json"
CONTENT_ROOT = ROOT / "Content/GameXXK/UI/Training/Generated/walkloop_pilot_v1"


class TrainingVisualImportContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.import_manifest = json.loads(IMPORT_MANIFEST.read_text(encoding="utf-8"))
        cls.source_manifest = json.loads(SOURCE_MANIFEST.read_text(encoding="utf-8"))

    def test_runtime_import_manifest_is_explicit_and_hash_locked(self) -> None:
        self.assertEqual(self.import_manifest["schemaVersion"], 1)
        self.assertEqual(self.import_manifest["status"], "runtime-mvp")
        imports = self.import_manifest["imports"]
        self.assertEqual(len(imports), 3)

        for record in imports:
            source = ROOT / record["source"]
            self.assertTrue(source.is_file(), source)
            self.assertEqual(
                hashlib.sha256(source.read_bytes()).hexdigest(),
                record["sourceSha256"].lower(),
            )
            object_path = record["asset"].lstrip("/").split("/")
            self.assertGreaterEqual(len(object_path), 3)
            package_parts = object_path[1:]
            package_parts[-1] = package_parts[-1].split(".", 1)[0] + ".uasset"
            asset_path = ROOT / "Content" / Path(*package_parts)
            self.assertTrue(asset_path.is_file(), asset_path)

    def test_atlas_and_background_dimensions_are_preserved(self) -> None:
        expected = {
            PROCESSING_ROOT / "atlas_2K/character_00_hero_walk_left_atlas.png": (2048, 2048, "RGBA"),
            PROCESSING_ROOT / "atlas_1K/character_00_hero_walk_left_atlas.png": (1024, 1024, "RGBA"),
            ROOT / "SourceArt/UI/PSD/desktop-training-v1/generated/TrainingIdleStrip_Background_GPT_v003_Seamless_RGBA.png": (1983, 793, "RGBA"),
        }
        for path, (width, height, mode) in expected.items():
            with Image.open(path) as image:
                self.assertEqual(image.size, (width, height), path)
                self.assertEqual(image.mode, mode, path)

    def test_source_review_asset_stays_separate_from_runtime_mvp_import(self) -> None:
        self.assertEqual(self.source_manifest["status"], "review-only")
        self.assertEqual(self.source_manifest["direction"], "left")
        self.assertEqual(self.source_manifest["frameCount"], 60)
        self.assertTrue((CONTENT_ROOT / "T_TrainingIdleStrip_Background.uasset").is_file())
        self.assertTrue(
            (CONTENT_ROOT / "character_00_hero_walk_left/atlas_2K/T_TrainingHeroWalkLeft_2K.uasset").is_file()
        )


if __name__ == "__main__":
    unittest.main()
