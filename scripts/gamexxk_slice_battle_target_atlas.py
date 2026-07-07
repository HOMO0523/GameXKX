#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = PROJECT_ROOT / "Content" / "GameXXK" / "SourceArt" / "UI" / "Battle"
DEFAULT_ATLAS = SOURCE_DIR / "battle_target_ink_dots_atlas.png"
DEFAULT_OUT_DIR = SOURCE_DIR / "Generated"
ROWS = 3
COLUMNS = 4
DAB_COUNT = ROWS * COLUMNS


def alpha_bounds(image: Image.Image, threshold: int) -> tuple[int, int, int, int]:
    alpha = image.getchannel("A")
    width, height = image.size
    xs: list[int] = []
    ys: list[int] = []
    for y in range(height):
        for x in range(width):
            if alpha.getpixel((x, y)) > threshold:
                xs.append(x)
                ys.append(y)
    if not xs:
        raise ValueError("cell has no visible alpha")
    return min(xs), min(ys), max(xs) + 1, max(ys) + 1


def expand_box(
    box: tuple[int, int, int, int],
    size: tuple[int, int],
    padding: int,
) -> tuple[int, int, int, int]:
    left, top, right, bottom = box
    width, height = size
    return (
        max(0, left - padding),
        max(0, top - padding),
        min(width, right + padding),
        min(height, bottom + padding),
    )


def slice_atlas(atlas_path: Path, out_dir: Path, padding: int, alpha_threshold: int) -> list[Path]:
    image = Image.open(atlas_path).convert("RGBA")
    cell_width = image.width // COLUMNS
    cell_height = image.height // ROWS
    out_dir.mkdir(parents=True, exist_ok=True)
    outputs: list[Path] = []

    for index in range(DAB_COUNT):
        row = index // COLUMNS
        column = index % COLUMNS
        cell_box = (
            column * cell_width,
            row * cell_height,
            image.width if column == COLUMNS - 1 else (column + 1) * cell_width,
            image.height if row == ROWS - 1 else (row + 1) * cell_height,
        )
        cell = image.crop(cell_box)
        bounds = expand_box(alpha_bounds(cell, alpha_threshold), cell.size, padding)
        dab = cell.crop(bounds)
        output = out_dir / f"battle_target_ink_dab_{index:02d}.png"
        dab.save(output)
        outputs.append(output)
    return outputs


def main() -> None:
    parser = argparse.ArgumentParser(description="Slice GameXXK battle targeting ink dab atlas")
    parser.add_argument("--atlas", type=Path, default=DEFAULT_ATLAS)
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT_DIR)
    parser.add_argument("--padding", type=int, default=12)
    parser.add_argument("--alpha-threshold", type=int, default=8)
    args = parser.parse_args()

    outputs = slice_atlas(args.atlas, args.out_dir, args.padding, args.alpha_threshold)
    print({"ok": True, "count": len(outputs), "out_dir": str(args.out_dir)})


if __name__ == "__main__":
    main()
