#!/usr/bin/env python3
"""Center source art on a 1600-square extended magenta canvas."""

from __future__ import annotations

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
JOBS = (
    (
        ROOT / "SourceAssets/CharacterVisuals/final_selected_v1",
        ROOT / "SourceAssets/AnimationProduction/safe_frame_1600/characters",
    ),
    (
        ROOT / "SourceAssets/RouteEnemies/final_selected_v1",
        ROOT / "SourceAssets/AnimationProduction/safe_frame_1600/enemies",
    ),
)


def expand_source(source_path: Path, output_path: Path) -> None:
    with Image.open(source_path) as source_image:
        source = source_image.convert("RGB")
        if source.width != source.height:
            raise ValueError(f"Animation source must be square: {source_path}")
        target_size = 1600
        if source.width > target_size or source.height > target_size:
            raise ValueError(f"Animation source exceeds 1600px: {source_path}")
        margin_x = (target_size - source.width) // 2
        margin_y = (target_size - source.height) // 2
        right_margin = target_size - source.width - margin_x
        bottom_margin = target_size - source.height - margin_y
        expanded = Image.new(
            "RGB",
            (target_size, target_size),
            source.getpixel((0, 0)),
        )
        expanded.paste(
            source.crop((0, 0, 1, source.height)).resize((margin_x, source.height)),
            (0, margin_y),
        )
        expanded.paste(
            source.crop((source.width - 1, 0, source.width, source.height)).resize(
                (right_margin, source.height)
            ),
            (margin_x + source.width, margin_y),
        )
        expanded.paste(
            source.crop((0, 0, source.width, 1)).resize((source.width, margin_y)),
            (margin_x, 0),
        )
        expanded.paste(
            source.crop((0, source.height - 1, source.width, source.height)).resize(
                (source.width, bottom_margin)
            ),
            (margin_x, margin_y + source.height),
        )
        expanded.paste(
            Image.new("RGB", (margin_x, margin_y), source.getpixel((0, 0))),
            (0, 0),
        )
        expanded.paste(
            Image.new(
                "RGB",
                (right_margin, margin_y),
                source.getpixel((source.width - 1, 0)),
            ),
            (margin_x + source.width, 0),
        )
        expanded.paste(
            Image.new(
                "RGB",
                (margin_x, bottom_margin),
                source.getpixel((0, source.height - 1)),
            ),
            (0, margin_y + source.height),
        )
        expanded.paste(
            Image.new(
                "RGB",
                (right_margin, bottom_margin),
                source.getpixel((source.width - 1, source.height - 1)),
            ),
            (margin_x + source.width, margin_y + source.height),
        )
        expanded.paste(source, (margin_x, margin_y))
        output_path.parent.mkdir(parents=True, exist_ok=True)
        expanded.save(output_path)


def main() -> None:
    count = 0
    for source_dir, output_dir in JOBS:
        for source_path in sorted(source_dir.glob("*.png")):
            output_path = output_dir / source_path.name
            expand_source(source_path, output_path)
            count += 1
            print(f"{source_path.relative_to(ROOT)} -> {output_path.relative_to(ROOT)}")
    if count != 34:
        raise RuntimeError(f"Expected 34 animation sources, generated {count}")


if __name__ == "__main__":
    main()
