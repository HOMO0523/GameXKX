#!/usr/bin/env python3
"""Split the approved 3x10 gem review sheet into named transparent sources."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image

from process_gem_icons import GEM_TYPES, QUALITIES, stem


def find_gap_boundaries(alpha: Image.Image, count: int, axis: str) -> list[int]:
    width, height = alpha.size
    extent = width if axis == "x" else height
    radius = max(8, round(extent / count * 0.22))
    boundaries = [0]
    for index in range(1, count):
        expected = round(index * extent / count)
        start = max(boundaries[-1] + 1, expected - radius)
        stop = min(extent - 1, expected + radius)
        candidates: list[tuple[int, int, int]] = []
        for position in range(start, stop + 1):
            if axis == "x":
                visible = sum(1 for y in range(height) if alpha.getpixel((position, y)) >= 16)
            else:
                visible = sum(1 for x in range(width) if alpha.getpixel((x, position)) >= 16)
            candidates.append((visible, abs(position - expected), position))
        if not candidates:
            raise RuntimeError(f"cannot locate {axis}-axis grid gap {index}")
        visible, _, boundary = min(candidates)
        if visible != 0:
            raise RuntimeError(f"no transparent {axis}-axis gap near grid boundary {index}")
        boundaries.append(boundary)
    boundaries.append(extent)
    return boundaries


def main() -> int:
    project_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument("sheet", type=Path)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=project_root / "SourceArt" / "UI" / "Items" / "Gems" / "generated",
    )
    args = parser.parse_args()

    with Image.open(args.sheet) as opened:
        sheet = opened.convert("RGBA")
    if sheet.getchannel("A").getextrema()[0] == 255:
        raise RuntimeError("gem sheet is not genuinely transparent")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    sheet_alpha = sheet.getchannel("A")
    row_boundaries = find_gap_boundaries(sheet_alpha, len(GEM_TYPES), "y")
    written: list[str] = []
    for row, gem_type in enumerate(GEM_TYPES):
        y0 = row_boundaries[row]
        y1 = row_boundaries[row + 1]
        row_alpha = sheet_alpha.crop((0, y0, sheet.width, y1))
        column_boundaries = find_gap_boundaries(row_alpha, len(QUALITIES), "x")
        for column, quality in enumerate(QUALITIES):
            x0 = column_boundaries[column]
            x1 = column_boundaries[column + 1]
            cell = sheet.crop((x0, y0, x1, y1))
            bbox = cell.getchannel("A").point(lambda value: 255 if value >= 16 else 0).getbbox()
            if bbox is None:
                raise RuntimeError(f"empty gem cell: {gem_type}/{quality}")
            left, top, right, bottom = bbox
            if left == 0 or top == 0 or right == cell.width or bottom == cell.height:
                raise RuntimeError(f"gem touches its grid boundary: {gem_type}/{quality}")
            source = cell.crop(bbox)
            destination = args.output_dir / f"{stem(gem_type, quality)}.png"
            source.save(destination, optimize=True)
            written.append(str(destination))

    print({"ok": True, "written_count": len(written), "output_dir": str(args.output_dir)})
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
