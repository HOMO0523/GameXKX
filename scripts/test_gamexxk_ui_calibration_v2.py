"""Regression tests for the GameXXK Hero/Backpack V2 calibration package."""

from __future__ import annotations

import hashlib
import json
import unittest
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PACKAGE_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "PSD" / "gamexxk-v4" / "calibration-v2"
SPEC = PACKAGE_ROOT / "calibration-spec.json"
SOURCE_LOCK = PACKAGE_ROOT / "source-lock.json"


def load_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def image_size(path: Path) -> tuple[int, int]:
    with Image.open(path) as image:
        return image.size


class GameXXKUiCalibrationV2Tests(unittest.TestCase):
    def test_contract_locks_reference_and_final_idle(self) -> None:
        spec = load_json(SPEC)
        source_lock = load_json(SOURCE_LOCK)

        self.assertEqual({"width": 1920, "height": 1080}, spec["canvas"])
        self.assertEqual("GameXXK_HeroBackpack_V2", spec["candidateName"])
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


if __name__ == "__main__":
    unittest.main()
