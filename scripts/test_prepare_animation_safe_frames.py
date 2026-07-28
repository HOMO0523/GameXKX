#!/usr/bin/env python3
"""Tests for the fixed 1600-square animation inputs."""

from __future__ import annotations

import unittest
from pathlib import Path

from PIL import Image, ImageChops


ROOT = Path(__file__).resolve().parents[1]
CASES = (
    (
        ROOT / "SourceAssets/CharacterVisuals/final_selected_v1",
        ROOT / "SourceAssets/AnimationProduction/safe_frame_1600/characters",
        13,
    ),
    (
        ROOT / "SourceAssets/RouteEnemies/final_selected_v1",
        ROOT / "SourceAssets/AnimationProduction/safe_frame_1600/enemies",
        21,
    ),
)


class AnimationSafeFrameTests(unittest.TestCase):
    def test_all_sources_have_pixel_preserving_eight_percent_border(self) -> None:
        for source_dir, output_dir, expected_count in CASES:
            sources = sorted(source_dir.glob("*.png"))
            self.assertEqual(expected_count, len(sources))
            outputs = sorted(output_dir.glob("*.png"))
            self.assertEqual(expected_count, len(outputs))
            for source_path in sources:
                output_path = output_dir / source_path.name
                self.assertTrue(output_path.is_file(), output_path)
                with Image.open(source_path) as source_image, Image.open(output_path) as output_image:
                    source = source_image.convert("RGB")
                    output = output_image.convert("RGB")
                    margin_x = (1600 - source.width) // 2
                    margin_y = (1600 - source.height) // 2
                    self.assertEqual((1600, 1600), output.size)
                    restored = output.crop(
                        (
                            margin_x,
                            margin_y,
                            margin_x + source.width,
                            margin_y + source.height,
                        )
                    )
                    self.assertIsNone(
                        ImageChops.difference(source, restored).getbbox(),
                        source_path.name,
                    )
                    left_padding_edge = output.crop(
                        (
                            margin_x - 1,
                            margin_y,
                            margin_x,
                            margin_y + source.height,
                        )
                    )
                    right_padding_edge = output.crop(
                        (
                            margin_x + source.width,
                            margin_y,
                            margin_x + source.width + 1,
                            margin_y + source.height,
                        )
                    )
                    self.assertIsNone(
                        ImageChops.difference(
                            source.crop((0, 0, 1, source.height)),
                            left_padding_edge,
                        ).getbbox(),
                        f"left seam: {source_path.name}",
                    )
                    self.assertIsNone(
                        ImageChops.difference(
                            source.crop(
                                (source.width - 1, 0, source.width, source.height)
                            ),
                            right_padding_edge,
                        ).getbbox(),
                        f"right seam: {source_path.name}",
                    )


if __name__ == "__main__":
    unittest.main()
