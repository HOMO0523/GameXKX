#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

from PIL import Image

PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = PROJECT_ROOT / "Content" / "GameXXK" / "SourceArt" / "UI" / "Battle"
GENERATED_DIR = SOURCE_DIR / "Generated"
ARROW_HEAD = SOURCE_DIR / "battle_target_arrow_head.png"
INK_ATLAS = SOURCE_DIR / "battle_target_ink_dots_atlas.png"
DAB_COUNT = 12


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def validate_alpha_png(path: Path) -> None:
    require(path.exists(), f"missing file: {path}")
    image = Image.open(path).convert("RGBA")
    width, height = image.size
    require(width > 0 and height > 0, f"invalid size: {path}")
    corners = [
        image.getpixel((0, 0))[3],
        image.getpixel((width - 1, 0))[3],
        image.getpixel((0, height - 1))[3],
        image.getpixel((width - 1, height - 1))[3],
    ]
    require(all(alpha == 0 for alpha in corners), f"corners are not transparent: {path}")
    alpha_channel = image.getchannel("A")
    if hasattr(alpha_channel, "get_flattened_data"):
        alpha_values = list(alpha_channel.get_flattened_data())
    else:
        alpha_values = list(alpha_channel.getdata())
    require(any(alpha > 0 for alpha in alpha_values), f"no visible pixels: {path}")


def main() -> None:
    validate_alpha_png(ARROW_HEAD)
    validate_alpha_png(INK_ATLAS)
    require(GENERATED_DIR.exists(), f"missing generated directory: {GENERATED_DIR}")
    dabs = sorted(GENERATED_DIR.glob("battle_target_ink_dab_*.png"))
    require(len(dabs) == DAB_COUNT, f"expected {DAB_COUNT} dabs, found {len(dabs)}")
    expected_names = [f"battle_target_ink_dab_{index:02d}.png" for index in range(DAB_COUNT)]
    require([path.name for path in dabs] == expected_names, "dab filenames are not deterministic")
    for dab in dabs:
        validate_alpha_png(dab)
    print({"ok": True, "dab_count": len(dabs), "generated_dir": str(GENERATED_DIR)})


if __name__ == "__main__":
    main()
