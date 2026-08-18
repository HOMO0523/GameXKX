#!/usr/bin/env python3
"""Deterministic checks for the isolated hero left-walk review asset."""

from __future__ import annotations

import hashlib
import json
import unittest
from pathlib import Path

import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ASSET_ROOT = ROOT / "SourceAssets/AnimationProcessing/walkloop_pilot_v1/character_00_hero_walk_left"
MANIFEST_PATH = ASSET_ROOT / "manifest.json"


class WalkLoopAtlasPipelineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
        cls.frames = sorted((ASSET_ROOT / "frames").glob("frame_*.png"))

    def test_manifest_locks_left_direction_and_exact_loop_seam(self) -> None:
        self.assertEqual(self.manifest["status"], "review-only")
        self.assertEqual(self.manifest["direction"], "left")
        self.assertEqual(self.manifest["fps"], 12)
        self.assertEqual(self.manifest["frameCount"], 60)
        self.assertTrue(self.manifest["firstLastIdentical"])
        self.assertEqual(self.manifest["firstFrameSha256"], self.manifest["lastFrameSha256"])
        self.assertEqual(len(self.frames), 60)

    def test_frames_are_rgba_512_cells_with_clean_chroma(self) -> None:
        self.assertTrue(self.frames)
        images = [Image.open(path).convert("RGBA") for path in self.frames]
        self.assertTrue(all(image.size == (512, 512) for image in images))
        self.assertEqual(images[0].tobytes(), images[-1].tobytes())
        first = np.asarray(images[0])
        self.assertGreater(int((first[..., 3] == 0).sum()), 0)
        self.assertTrue(np.all(first[first[..., 3] == 0, :3] == 0))
        visible = first[first[..., 3] > 0, :3]
        magenta_spill = (visible[:, 0] >= 200) & (visible[:, 2] >= 200) & (visible[:, 1] <= 24)
        self.assertEqual(int(magenta_spill.sum()), 0)

    def test_requested_2k_and_1k_atlases_have_project_grid_and_hashes(self) -> None:
        expected = {"4K": (4096, 512), "2K": (2048, 256), "1K": (1024, 128)}
        for label, (axis, cell) in expected.items():
            record = self.manifest["variants"][label]
            path = ROOT / record["atlas"]
            self.assertTrue(path.is_file(), path)
            with Image.open(path) as image:
                self.assertEqual(image.mode, "RGBA")
                self.assertEqual(image.size, (axis, axis))
            self.assertEqual(record["cellSize"], cell)
            self.assertEqual(record["sha256"], hashlib.sha256(path.read_bytes()).hexdigest())


if __name__ == "__main__":
    unittest.main()
