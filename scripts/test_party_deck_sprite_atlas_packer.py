#!/usr/bin/env python3
"""Regression tests for deterministic 4x2 green-screen PartyDeck sprite packing."""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image, ImageDraw


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PACKER = PROJECT_ROOT / "scripts" / "prepare_party_deck_sprite_atlas.py"
PACKED_ROOT = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "character-references" / "packed"
DIRECTIONS = ["S", "SW", "W", "NW", "N", "NE", "E", "SE"]
COLORS = [
    (240, 58, 58),
    (240, 132, 49),
    (235, 206, 46),
    (91, 190, 78),
    (48, 169, 184),
    (57, 98, 209),
    (133, 78, 196),
    (208, 68, 159),
]


def _make_green_screen_fixture(path: Path) -> None:
    cell_width, cell_height = 140, 240
    image = Image.new("RGB", (cell_width * 4, cell_height * 2), (0, 255, 0))
    draw = ImageDraw.Draw(image)
    for index, color in enumerate(COLORS):
        column = index % 4
        row = index // 4
        left = column * cell_width + 35
        top = row * cell_height + 40
        draw.rectangle((left, top, left + 60, top + 150), fill=(25, 25, 25))
        draw.rectangle((left + 2, top + 2, left + 58, top + 148), fill=color)
        # An enclosed bright-green screen pocket must key out too; it is not
        # connected to a crop edge after the dark outline is drawn.  Leave the
        # green-bodied NW sample untouched to prove an isolated green garment
        # does not become background merely because it is green.
        if index != 3:
            draw.rectangle((left + 30, top + 82, left + 42, top + 96), fill=(0, 255, 0))
        # Deliberately asymmetric left-side marker: a later mirror would put it on the right.
        draw.rectangle((left + 4, top + 18, left + 21, top + 44), fill=(255, 255, 255))
        draw.rectangle((left + 44, top + 105, left + 56, top + 130), fill=(25, 25, 25))
    image.save(path)


def _run_packer(
    source: Path,
    output_dir: Path,
    *,
    replace_existing: bool = False,
) -> tuple[subprocess.CompletedProcess[str], dict[str, object]]:
    command = [
        sys.executable,
        str(PACKER),
        "--input", str(source),
        "--output-dir", str(output_dir),
        "--prefix", "synthetic_yue_bai",
        "--json",
    ]
    if replace_existing:
        command.append("--replace-existing")
    completed = subprocess.run(
        command,
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    payload = json.loads(completed.stdout) if completed.stdout.strip() else {}
    return completed, payload


class PartyDeckSpriteAtlasPackerTests(unittest.TestCase):
    def test_packs_all_eight_source_cells_without_mirroring_and_keeps_idle_distinct(self) -> None:
        self.assertTrue(PACKER.exists(), f"sprite atlas packer is missing: {PACKER}")
        packed_root_existed = PACKED_ROOT.exists()
        PACKED_ROOT.mkdir(parents=True, exist_ok=True)
        try:
            with tempfile.TemporaryDirectory() as input_temp, tempfile.TemporaryDirectory(dir=PACKED_ROOT) as output_temp:
                source = Path(input_temp) / "four_by_two_green.png"
                _make_green_screen_fixture(source)
                completed, payload = _run_packer(source, Path(output_temp))

                self.assertEqual(completed.returncode, 0, completed.stderr)
                self.assertTrue(payload["ok"])
                self.assertEqual(payload["directions"], DIRECTIONS)
                self.assertTrue(payload["validation"]["ok"])

                idle_path = PROJECT_ROOT / str(payload["outputs"]["idle"])
                walk_path = PROJECT_ROOT / str(payload["outputs"]["walk"])
                self.assertTrue(idle_path.is_file())
                self.assertTrue(walk_path.is_file())
                with Image.open(idle_path).convert("RGBA") as idle, Image.open(walk_path).convert("RGBA") as walk:
                    self.assertEqual(idle.size, (171, 1640))
                    self.assertEqual(walk.size, (1026, 1640))
                    for row, color in enumerate(COLORS):
                        idle_cell = idle.crop((0, row * 205, 171, (row + 1) * 205))
                        walk_frame_zero = walk.crop((0, row * 205, 171, (row + 1) * 205))
                        self.assertNotEqual(idle_cell.tobytes(), walk_frame_zero.tobytes())
                        alpha = idle_cell.getchannel("A")
                        self.assertIsNotNone(alpha.getbbox(), DIRECTIONS[row])
                        self.assertGreater(alpha.tobytes().count(0), 0, DIRECTIONS[row])
                        self.assertGreater(sum(1 for pixel in idle_cell.getdata() if pixel[:3] == color and pixel[3] > 0), 100, DIRECTIONS[row])
                        white_x = [x for y in range(205) for x in range(171) if idle_cell.getpixel((x, y))[:3] == (255, 255, 255)]
                        bbox = alpha.getbbox()
                        self.assertTrue(bbox)
                        self.assertLess(sum(white_x) / len(white_x), (bbox[0] + bbox[2]) / 2, DIRECTIONS[row])
                        frames = [walk.crop((column * 171, row * 205, (column + 1) * 171, (row + 1) * 205)).tobytes() for column in range(6)]
                        self.assertEqual(len(set(frames)), 6, DIRECTIONS[row])
        finally:
            if not packed_root_existed and PACKED_ROOT.exists():
                try:
                    PACKED_ROOT.rmdir()
                except OSError:
                    shutil.rmtree(PACKED_ROOT)

    def test_rejects_an_output_directory_outside_the_owned_packed_root(self) -> None:
        self.assertTrue(PACKER.exists(), f"sprite atlas packer is missing: {PACKER}")
        with tempfile.TemporaryDirectory() as input_temp, tempfile.TemporaryDirectory() as outside_temp:
            source = Path(input_temp) / "four_by_two_green.png"
            _make_green_screen_fixture(source)
            completed, payload = _run_packer(source, Path(outside_temp))

        self.assertNotEqual(completed.returncode, 0)
        self.assertFalse(payload["ok"])
        self.assertIn("must remain under", payload["error"])

    def test_explicit_replace_is_required_for_a_changed_owned_output(self) -> None:
        self.assertTrue(PACKER.exists(), f"sprite atlas packer is missing: {PACKER}")
        packed_root_existed = PACKED_ROOT.exists()
        PACKED_ROOT.mkdir(parents=True, exist_ok=True)
        try:
            with tempfile.TemporaryDirectory() as input_temp, tempfile.TemporaryDirectory(dir=PACKED_ROOT) as output_temp:
                source = Path(input_temp) / "four_by_two_green.png"
                _make_green_screen_fixture(source)
                created, created_payload = _run_packer(source, Path(output_temp))
                self.assertEqual(created.returncode, 0, created.stderr)
                self.assertEqual(created_payload["outputs"]["idle_action"], "created")

                with Image.open(source) as changed_source:
                    changed = changed_source.convert("RGB")
                changed.putpixel((50, 120), (210, 10, 10))
                changed.save(source)

                rejected, rejected_payload = _run_packer(source, Path(output_temp))
                self.assertNotEqual(rejected.returncode, 0)
                self.assertFalse(rejected_payload["ok"])
                self.assertIn("refusing to overwrite", rejected_payload["error"])

                replaced, replaced_payload = _run_packer(source, Path(output_temp), replace_existing=True)
                self.assertEqual(replaced.returncode, 0, replaced.stderr)
                self.assertEqual(replaced_payload["outputs"]["idle_action"], "replaced")
                self.assertEqual(replaced_payload["outputs"]["walk_action"], "replaced")
        finally:
            if not packed_root_existed and PACKED_ROOT.exists():
                try:
                    PACKED_ROOT.rmdir()
                except OSError:
                    shutil.rmtree(PACKED_ROOT)


if __name__ == "__main__":
    unittest.main()
