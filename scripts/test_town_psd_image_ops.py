"""Tests for the OpenCV-free mask cleanup used by the PSD cutter."""

from __future__ import annotations

import importlib
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PIPELINE_ROOT = PROJECT_ROOT / "scripts" / "ui_psd_pipeline"
if str(PIPELINE_ROOT) not in sys.path:
    sys.path.insert(0, str(PIPELINE_ROOT))

try:
    image_ops = importlib.import_module("town_psd_image_ops")
    clean_mask = getattr(image_ops, "clean_mask", None)
    export_runtime_backgrounds = getattr(image_ops, "export_runtime_backgrounds", None)
except ModuleNotFoundError:
    clean_mask = None
    export_runtime_backgrounds = None


class TownPsdImageOpsTest(unittest.TestCase):
    def test_cleanup_discards_noise_and_fills_enclosed_sheet_hole(self) -> None:
        self.assertIsNotNone(clean_mask, "pipeline must export clean_mask(mask, min_area)")
        mask = np.zeros((9, 9), dtype=np.uint8)
        mask[2:7, 2:7] = 1
        mask[4, 4] = 0
        mask[0, 8] = 1

        result = clean_mask(mask, min_area=18)

        self.assertEqual(0, int(result[0, 8]), "a one-pixel exterior speck must be removed")
        self.assertEqual(1, int(result[4, 4]), "an enclosed white detail must remain opaque")
        self.assertEqual(1, int(result[2, 2]), "the retained painted component must survive")

    def test_export_runtime_backgrounds_splits_the_sheet_without_resizing(self) -> None:
        self.assertIsNotNone(
            export_runtime_backgrounds,
            "pipeline must export export_runtime_backgrounds(image, destination, rectangles)",
        )
        sheet = Image.new("RGBA", (12, 8), (0, 0, 0, 0))
        sheet.paste((180, 90, 40, 255), (0, 0, 6, 4))
        sheet.paste((40, 120, 170, 255), (6, 0, 12, 4))
        with tempfile.TemporaryDirectory() as temporary_directory:
            outputs = export_runtime_backgrounds(
                sheet,
                Path(temporary_directory),
                {"hud": (0, 0, 6, 4), "character": (6, 0, 12, 4)},
            )
            self.assertEqual({"hud", "character"}, set(outputs))
            with Image.open(outputs["hud"]) as hud:
                self.assertEqual((6, 4), hud.size)
                self.assertEqual((180, 90, 40, 255), hud.getpixel((0, 0)))
            with Image.open(outputs["character"]) as character:
                self.assertEqual((40, 120, 170, 255), character.getpixel((0, 0)))


if __name__ == "__main__":
    unittest.main()
