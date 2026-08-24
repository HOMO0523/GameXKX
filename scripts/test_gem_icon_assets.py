#!/usr/bin/env python3
"""Deterministic contract tests for the progressive gem-icon asset processor."""

from __future__ import annotations

from contextlib import redirect_stderr
import hashlib
import io
import json
from pathlib import Path
import random
import re
import tempfile
import unittest

from PIL import Image, ImageDraw

from scripts import process_gem_icons as processor


REQUIRED_RECORD_KEYS = {
    "item_id",
    "source_png",
    "texture_path",
    "size",
    "mode",
    "alpha_bbox",
    "transparent_border_ratio",
    "sha256",
}


def _fixture_layout(root: Path) -> tuple[Path, Path, Path, Path]:
    gem_root = root / "SourceArt" / "UI" / "Items" / "Gems"
    return (
        gem_root / "generated",
        gem_root / "final",
        gem_root / "gem_icon_manifest.json",
        gem_root / "review" / "gem-quality-progression-contact-sheet.png",
    )


def _write_valid_rgba(path: Path, index: int = 0) -> None:
    image = Image.new("RGBA", (96, 80), (0, 0, 0, 0))
    color = (20 + index * 7, 40 + index * 5, 60 + index * 3, 255)
    ImageDraw.Draw(image).rectangle((18, 14, 77, 65), fill=color)
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, format="PNG")


def _write_complete_generated_set(input_dir: Path, *, unique: bool = True) -> None:
    for index, (gem_type, quality) in enumerate(processor.ASSET_MATRIX):
        fixture_index = index if unique else 0
        _write_valid_rgba(
            input_dir / f"{processor.stem(gem_type, quality)}.png", fixture_index
        )


def _padded_checkerboard(
    tile_size: int,
    phase_x: int = 0,
    phase_y: int = 0,
    *,
    canvas_size: int = 64,
    border: int = 8,
) -> Image.Image:
    image = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    colors = ((198, 198, 198, 255), (232, 232, 232, 255))
    for y in range(border, canvas_size - border):
        for x in range(border, canvas_size - border):
            image.putpixel(
                (x, y),
                colors[
                    (
                        ((x - border + phase_x) // tile_size)
                        + ((y - border + phase_y) // tile_size)
                    )
                    % 2
                ],
            )
    return image


class GemMatrixContractTests(unittest.TestCase):
    def test_exact_three_by_ten_matrix_and_order(self) -> None:
        self.assertEqual(processor.GEM_TYPES, ("Attack", "Defense", "MaxHealth"))
        self.assertEqual(
            processor.QUALITIES,
            (
                "Common",
                "Rare",
                "Epic",
                "Legendary",
                "Immortal",
                "Treasure",
                "Transcendent",
                "Celestial",
                "Ascendant",
                "Cosmic",
            ),
        )
        self.assertEqual(len(processor.ASSET_MATRIX), 30)
        self.assertEqual(
            processor.ASSET_MATRIX,
            tuple(
                (gem_type, quality)
                for gem_type in processor.GEM_TYPES
                for quality in processor.QUALITIES
            ),
        )

    def test_stable_stem_item_id_and_texture_path(self) -> None:
        for gem_type, quality in processor.ASSET_MATRIX:
            with self.subTest(gem_type=gem_type, quality=quality):
                expected_stem = f"T_Item_Gem_{gem_type}_{quality}"
                self.assertEqual(processor.stem(gem_type, quality), expected_stem)
                self.assertEqual(
                    processor.item_id(gem_type, quality),
                    f"Item.Gem.{gem_type}.{quality}",
                )
                self.assertEqual(
                    processor.texture_path(gem_type, quality),
                    f"/Game/GameXXK/UI/Items/Gems/{expected_stem}.{expected_stem}",
                )

    def test_unknown_axis_values_are_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "unknown gem type"):
            processor.stem("Healing", "Common")
        with self.assertRaisesRegex(ValueError, "unknown gem quality"):
            processor.item_id("Attack", "Mythic")


class GemNormalizationTests(unittest.TestCase):
    def test_normalization_is_rgba_512_centered_at_seventy_five_percent(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source.png"
            destination = root / "final.png"
            _write_valid_rgba(source)

            metrics = processor.normalize_icon(source, destination)

            self.assertEqual(metrics["size"], [512, 512])
            self.assertEqual(metrics["mode"], "RGBA")
            self.assertEqual(metrics["transparent_border_ratio"], 1.0)
            with Image.open(destination) as output:
                self.assertEqual(output.size, (512, 512))
                self.assertEqual(output.mode, "RGBA")
                bbox = processor.alpha_bbox(output)
                self.assertIsNotNone(bbox)
                assert bbox is not None
                left, top, right, bottom = bbox
                self.assertEqual(
                    max(right - left, bottom - top), processor.TARGET_SUBJECT_EXTENT
                )
                self.assertLessEqual(abs((left + right) / 2 - 256), 0.5)
                self.assertLessEqual(abs((top + bottom) / 2 - 256), 0.5)
                self.assertTrue(
                    all(
                        output.getpixel(point)[3] == 0
                        for point in ((0, 0), (511, 0), (0, 511), (511, 511))
                    )
                )

    def test_source_without_alpha_channel_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "rgb.png"
            Image.new("RGB", (32, 32), (100, 90, 80)).save(source, format="PNG")
            with self.assertRaisesRegex(RuntimeError, "no alpha channel"):
                processor.normalize_icon(source, root / "out.png")

    def test_normalization_preserves_partial_alpha_instead_of_keying_it(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "partial-alpha.png"
            destination = root / "out.png"
            image = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
            ImageDraw.Draw(image).rectangle((8, 8, 23, 23), fill=(80, 90, 100, 96))
            image.save(source, format="PNG")

            processor.normalize_icon(source, destination)

            with Image.open(destination) as output:
                self.assertEqual(output.getpixel((256, 256))[3], 96)

    def test_fully_opaque_source_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "opaque.png"
            Image.new("RGBA", (32, 32), (100, 90, 80, 255)).save(
                source, format="PNG"
            )
            with self.assertRaisesRegex(RuntimeError, "fully opaque canvas"):
                processor.normalize_icon(source, root / "out.png")

    def test_fully_transparent_source_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "transparent.png"
            Image.new("RGBA", (32, 32), (0, 0, 0, 0)).save(source, format="PNG")
            with self.assertRaisesRegex(RuntimeError, "fully transparent"):
                processor.normalize_icon(source, root / "out.png")

    def test_opaque_source_corner_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "corner.png"
            _write_valid_rgba(source)
            with Image.open(source) as opened:
                image = opened.convert("RGBA")
            image.putpixel((0, 0), (100, 90, 80, 255))
            image.save(source, format="PNG")
            with self.assertRaisesRegex(RuntimeError, "opaque canvas corner"):
                processor.normalize_icon(source, root / "out.png")

    def test_baked_checkerboard_is_explicitly_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "checkerboard.png"
            image = Image.new("RGBA", (64, 64), (0, 0, 0, 255))
            colors = ((204, 204, 204, 255), (238, 238, 238, 255))
            for y in range(64):
                for x in range(64):
                    image.putpixel((x, y), colors[((x // 8) + (y // 8)) % 2])
            for point in ((0, 0), (63, 0), (0, 63), (63, 63)):
                image.putpixel(point, (0, 0, 0, 0))
            image.save(source, format="PNG")

            self.assertTrue(processor.has_baked_checkerboard(image))
            with self.assertRaisesRegex(RuntimeError, "baked checkerboard"):
                processor.normalize_icon(source, root / "out.png")

    def test_transparent_padding_around_internal_baked_checkerboard_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            covered_phase_count = 0
            for tile_size in (4, 8, 12):
                for phase_y in range(tile_size):
                    for phase_x in range(tile_size):
                        with self.subTest(
                            tile_size=tile_size,
                            phase_x=phase_x,
                            phase_y=phase_y,
                        ):
                            image = _padded_checkerboard(
                                tile_size, phase_x, phase_y
                            )
                            self.assertTrue(
                                processor.has_baked_checkerboard(image),
                                msg=(
                                    f"missed tile={tile_size} phase=({phase_x},{phase_y})"
                                ),
                            )
                            covered_phase_count += 1

            self.assertEqual(covered_phase_count, 4**2 + 8**2 + 12**2)

            asymmetric_phases = {4: (1, 3), 8: (1, 5), 12: (2, 7)}
            for tile_size, (phase_x, phase_y) in asymmetric_phases.items():
                with self.subTest(
                    normalize_tile_size=tile_size,
                    phase_x=phase_x,
                    phase_y=phase_y,
                ):
                    source = root / f"padded-checkerboard-{tile_size}.png"
                    image = _padded_checkerboard(tile_size, phase_x, phase_y)
                    image.save(source, format="PNG")
                    with self.assertRaisesRegex(RuntimeError, "baked checkerboard"):
                        processor.normalize_icon(
                            source, root / f"out-{tile_size}.png"
                        )

    def test_nonperiodic_ink_facets_are_not_rejected_as_checkerboard(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "faceted-ink-gem.png"
            destination = root / "out.png"
            image = Image.new("RGBA", (80, 80), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.polygon(
                ((39, 7), (69, 28), (61, 68), (18, 61), (8, 31)),
                fill=(91, 101, 108, 255),
                outline=(35, 39, 42, 255),
                width=3,
            )
            draw.polygon(
                ((39, 10), (39, 39), (12, 31)), fill=(126, 134, 137, 255)
            )
            draw.polygon(
                ((42, 11), (65, 29), (42, 39)), fill=(75, 88, 98, 255)
            )
            draw.polygon(
                ((12, 34), (39, 42), (21, 58)), fill=(70, 78, 82, 255)
            )
            draw.polygon(
                ((42, 42), (64, 32), (58, 63)), fill=(111, 116, 112, 255)
            )
            draw.line(((39, 8), (39, 65)), fill=(42, 47, 49, 220), width=2)
            image.save(source, format="PNG")

            self.assertFalse(processor.has_baked_checkerboard(image))
            metrics = processor.normalize_icon(source, destination)

            self.assertEqual(metrics["mode"], "RGBA")
            self.assertEqual(metrics["transparent_border_ratio"], 1.0)
            self.assertTrue(destination.is_file())

    def test_checkerboard_behind_central_irregular_gem_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "checkerboard-with-subject.png"
            image = _padded_checkerboard(
                8, 3, 5, canvas_size=96, border=8
            )
            draw = ImageDraw.Draw(image)
            draw.polygon(
                ((47, 25), (70, 39), (64, 70), (42, 80), (25, 58), (31, 34)),
                fill=(154, 42, 34, 255),
                outline=(49, 31, 29, 255),
                width=3,
            )
            draw.polygon(
                ((47, 29), (47, 52), (32, 38)), fill=(205, 84, 56, 255)
            )
            draw.polygon(
                ((50, 29), (67, 40), (50, 52)), fill=(108, 35, 38, 255)
            )
            image.save(source, format="PNG")

            self.assertTrue(processor.has_baked_checkerboard(image))
            with self.assertRaisesRegex(RuntimeError, "baked checkerboard"):
                processor.normalize_icon(source, root / "out.png")

    def test_low_saturation_diagonal_stripes_are_not_checkerboards(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            colors = ((194, 198, 201, 255), (226, 230, 233, 255))
            for stripe_width in (4, 8, 16):
                with self.subTest(stripe_width=stripe_width):
                    source = root / f"diagonal-stripes-{stripe_width}.png"
                    destination = root / f"out-{stripe_width}.png"
                    image = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
                    for y in range(8, 56):
                        for x in range(8, 56):
                            image.putpixel(
                                (x, y),
                                colors[
                                    (((x - 8) + (y - 8)) // stripe_width) % 2
                                ],
                            )
                    image.save(source, format="PNG")

                    self.assertFalse(processor.has_baked_checkerboard(image))
                    metrics = processor.normalize_icon(source, destination)
                    self.assertEqual(metrics["mode"], "RGBA")
                    self.assertTrue(destination.is_file())

    def test_seeded_nonperiodic_textures_have_zero_checkerboard_false_positives(self) -> None:
        generator = random.Random(20260824)
        palettes = (
            ((188, 192, 195, 255), (224, 228, 231, 255)),
            (
                (172, 178, 182, 255),
                (197, 202, 205, 255),
                (221, 225, 228, 255),
                (145, 151, 155, 255),
            ),
        )
        false_positive_indices: list[int] = []
        for index in range(1000):
            image = Image.new("RGBA", (56, 56), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            palette = palettes[index % len(palettes)]
            for block_y in range(12):
                for block_x in range(12):
                    color = palette[generator.randrange(len(palette))]
                    left = 4 + block_x * 4
                    top = 4 + block_y * 4
                    draw.rectangle((left, top, left + 3, top + 3), fill=color)
            if processor.has_baked_checkerboard(image):
                false_positive_indices.append(index)

        self.assertEqual(false_positive_indices, [])


class GemPipelineContractTests(unittest.TestCase):
    def test_complete_temporary_set_builds_manifest_and_three_by_ten_sheet(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            input_dir, output_dir, manifest_path, contact_sheet_path = _fixture_layout(root)
            _write_complete_generated_set(input_dir)

            result = processor.process_icon_set(
                input_dir,
                output_dir,
                manifest_path,
                contact_sheet_path,
                project_root=root,
            )

            self.assertEqual(result["ok"], True)
            self.assertEqual(result["icon_count"], 30)
            final_paths = sorted(output_dir.glob("*.png"))
            self.assertEqual(len(final_paths), 30)

            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual(manifest["icon_count"], 30)
            records = manifest["records"]
            self.assertEqual(len(records), 30)
            hashes: list[str] = []
            for record, (gem_type, quality) in zip(records, processor.ASSET_MATRIX):
                with self.subTest(gem_type=gem_type, quality=quality):
                    self.assertEqual(set(record), REQUIRED_RECORD_KEYS)
                    self.assertEqual(record["item_id"], processor.item_id(gem_type, quality))
                    filename = f"{processor.stem(gem_type, quality)}.png"
                    self.assertEqual(
                        record["source_png"],
                        f"SourceArt/UI/Items/Gems/final/{filename}",
                    )
                    self.assertEqual(
                        record["texture_path"], processor.texture_path(gem_type, quality)
                    )
                    self.assertEqual(record["size"], [512, 512])
                    self.assertEqual(record["mode"], "RGBA")
                    left, top, right, bottom = record["alpha_bbox"]
                    self.assertGreater(right, left)
                    self.assertGreater(bottom, top)
                    self.assertEqual(
                        max(right - left, bottom - top),
                        processor.TARGET_SUBJECT_EXTENT,
                    )
                    self.assertEqual(record["transparent_border_ratio"], 1.0)
                    self.assertRegex(record["sha256"], r"^[0-9a-f]{64}$")
                    final_path = output_dir / filename
                    self.assertEqual(
                        record["sha256"],
                        hashlib.sha256(final_path.read_bytes()).hexdigest(),
                    )
                    with Image.open(final_path) as final_image:
                        self.assertEqual(final_image.size, (512, 512))
                        self.assertEqual(final_image.mode, "RGBA")
                    hashes.append(record["sha256"])
            self.assertEqual(len(set(hashes)), 30)

            with Image.open(contact_sheet_path) as contact_sheet:
                self.assertEqual(contact_sheet.mode, "RGB")
                self.assertEqual(
                    contact_sheet.size,
                    (
                        10 * processor.CONTACT_CELL_WIDTH,
                        3 * processor.CONTACT_CELL_HEIGHT,
                    ),
                )
                self.assertEqual(contact_sheet.getpixel((0, 0)), processor.PAPER_BACKGROUND)
                preview_center = (
                    processor.CONTACT_PREVIEW_OFFSET[0]
                    + processor.CONTACT_PREVIEW_SIZE // 2,
                    processor.CONTACT_PREVIEW_OFFSET[1]
                    + processor.CONTACT_PREVIEW_SIZE // 2,
                )
                inset_center = (
                    processor.CONTACT_INSET_OFFSET[0]
                    + processor.CONTACT_INSET_SIZE // 2,
                    processor.CONTACT_INSET_OFFSET[1]
                    + processor.CONTACT_INSET_SIZE // 2,
                )
                self.assertEqual(contact_sheet.getpixel(preview_center), (20, 40, 60))
                self.assertEqual(contact_sheet.getpixel(inset_center), (20, 40, 60))

    def test_duplicate_final_hashes_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            input_dir, output_dir, manifest_path, contact_sheet_path = _fixture_layout(root)
            _write_complete_generated_set(input_dir, unique=False)

            with self.assertRaisesRegex(RuntimeError, "hashes must be unique"):
                processor.process_icon_set(
                    input_dir,
                    output_dir,
                    manifest_path,
                    contact_sheet_path,
                    project_root=root,
                )
            self.assertFalse(manifest_path.exists())
            self.assertFalse(contact_sheet_path.exists())

    def test_cli_missing_inputs_fails_clearly_instead_of_skipping(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            input_dir, output_dir, manifest_path, contact_sheet_path = _fixture_layout(root)
            stderr = io.StringIO()
            with redirect_stderr(stderr):
                exit_code = processor.main(
                    [
                        "--input-dir",
                        str(input_dir),
                        "--output-dir",
                        str(output_dir),
                        "--manifest",
                        str(manifest_path),
                        "--contact-sheet",
                        str(contact_sheet_path),
                    ]
                )

            self.assertEqual(exit_code, 1)
            failure = json.loads(stderr.getvalue())
            self.assertEqual(failure["ok"], False)
            self.assertIn("missing generated gem inputs (30 of 30)", failure["error"])
            self.assertIn("T_Item_Gem_Attack_Common.png", failure["error"])
            self.assertFalse(output_dir.exists())
            self.assertFalse(manifest_path.exists())
            self.assertFalse(contact_sheet_path.exists())

    def test_sha256_contract_is_lowercase_hex(self) -> None:
        self.assertTrue(re.fullmatch(r"[0-9a-f]{64}", "0" * 64))
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "bytes.bin"
            path.write_bytes(b"GameXXK gem icon")
            digest = processor.sha256_file(path)
            self.assertRegex(digest, r"^[0-9a-f]{64}$")
            self.assertEqual(digest, hashlib.sha256(path.read_bytes()).hexdigest())


if __name__ == "__main__":
    unittest.main()
