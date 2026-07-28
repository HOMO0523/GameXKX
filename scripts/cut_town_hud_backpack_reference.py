#!/usr/bin/env python3
"""Cut dynamic-safe Town HUD and backpack UI atoms from the approved reference.

This cutter never redraws the approved art and never uses image generation.  It
only crops, mirrors, tiles, and clears dynamic content areas from
``2026-07-14-tencent-town-ui-source-2.png``.  The resulting atoms keep the
reference's paper/ink pixels while leaving HP, XP, icons, quantities, labels,
and inventory contents to UMG at runtime.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageOps


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT_ROOT / "docs" / "reference-assets" / "2026-07-14-tencent-town-ui-source-2.png"
OUTPUT_ROOT = PROJECT_ROOT / "docs" / "ui" / "town" / "source_art"

HUD_BARS: dict[str, dict[str, tuple[int, int, int, int]]] = {
    "experience": {
        "fill": (388, 502, 54, 4),
        "track": (450, 502, 39, 4),
    },
    "health": {
        "fill": (388, 523, 102, 4),
        "track": (450, 502, 39, 4),
    },
}

BACKPACK_CROPS: dict[str, tuple[int, int, int, int]] = {
    "backpack_header.png": (656, 720, 120, 36),
    "backpack_back_arrow.png": (664, 731, 32, 24),
    "backpack_button_sort.png": (823, 977, 73, 31),
    "backpack_button_disassemble.png": (914, 977, 73, 31),
    "backpack_tab_all.png": (700, 758, 57, 29),
    "backpack_tab_equipment.png": (761, 758, 59, 29),
    "backpack_tab_prop.png": (824, 758, 57, 29),
    "backpack_tab_material.png": (887, 758, 57, 29),
    "backpack_tab_task.png": (949, 758, 57, 29),
}

EXPECTED_OUTPUTS = (
    "HUD/hud_experience_bar_frame.png",
    "HUD/hud_experience_bar_fill.png",
    "HUD/hud_health_bar_frame.png",
    "HUD/hud_health_bar_fill.png",
    "Backpack/backpack_window_frame.png",
    "Backpack/backpack_header.png",
    "Backpack/backpack_back_arrow.png",
    "Backpack/backpack_slot.png",
    "Backpack/backpack_action_blank.png",
    "Backpack/backpack_button_sort.png",
    "Backpack/backpack_button_disassemble.png",
    "Backpack/backpack_tab_all.png",
    "Backpack/backpack_tab_equipment.png",
    "Backpack/backpack_tab_prop.png",
    "Backpack/backpack_tab_material.png",
    "Backpack/backpack_tab_task.png",
)


def crop(source: Image.Image, box: tuple[int, int, int, int]) -> Image.Image:
    x, y, width, height = box
    return source.crop((x, y, x + width, y + height)).convert("RGBA")


def tile(destination: Image.Image, source: Image.Image, left: int, top: int, width: int, height: int) -> None:
    """Tile only existing reference pixels into ``destination``."""
    for y in range(top, top + height, source.height):
        for x in range(left, left + width, source.width):
            tile_width = min(source.width, left + width - x)
            tile_height = min(source.height, top + height - y)
            destination.alpha_composite(source.crop((0, 0, tile_width, tile_height)), (x, y))


def create_bar_pair(source: Image.Image, donors: dict[str, tuple[int, int, int, int]]) -> tuple[Image.Image, Image.Image]:
    """Create a runtime bar from the reference's text-free color cores only."""
    canvas_size = (110, 10)
    inner_width = 102
    line_top = 3
    track_core = crop(source, donors["track"])
    fill_core = crop(source, donors["fill"])
    frame = Image.new("RGBA", canvas_size, (0, 0, 0, 0))
    fill = Image.new("RGBA", canvas_size, (0, 0, 0, 0))
    tile(frame, track_core, 4, line_top, inner_width, 4)
    tile(fill, fill_core, 4, line_top, inner_width, 4)
    return frame, fill


def create_backpack_frame(source: Image.Image) -> Image.Image:
    """Build a blank 9-slice parchment frame from direct safe donor regions."""
    size = (368, 304)
    destination = Image.new("RGBA", size, (0, 0, 0, 0))
    top_right = crop(source, (1000, 720, 24, 24))
    top_left = ImageOps.mirror(top_right)
    top_edge = crop(source, (760, 720, 160, 12))
    right_edge = crop(source, (1014, 800, 10, 170))
    left_edge = ImageOps.mirror(right_edge)
    bottom_right = crop(source, (1000, 1000, 24, 24))
    bottom_left = ImageOps.mirror(bottom_right)
    bottom_edge = crop(source, (760, 1010, 150, 14))
    paper = crop(source, (992, 800, 16, 160))

    tile(destination, paper, 10, 12, size[0] - 20, size[1] - 26)
    tile(destination, top_edge, 24, 0, size[0] - 48, 12)
    tile(destination, bottom_edge, 24, size[1] - 14, size[0] - 48, 14)
    tile(destination, right_edge, size[0] - 10, 24, 10, size[1] - 48)
    tile(destination, left_edge, 0, 24, 10, size[1] - 48)
    destination.alpha_composite(top_left, (0, 0))
    destination.alpha_composite(top_right, (size[0] - top_right.width, 0))
    destination.alpha_composite(bottom_left, (0, size[1] - bottom_left.height))
    destination.alpha_composite(bottom_right, (size[0] - bottom_right.width, size[1] - bottom_right.height))
    return destination


def create_slot(source: Image.Image) -> Image.Image:
    """Rebuild a blank slot from direct border and paper donors.

    The supplied reference has only occupied slots.  Its sword reaches the
    lower ring, so keep the clean top/side border pieces and mirror the clean
    top edge for the bottom instead of retaining any sample-item pixels.
    """
    source_slot = crop(source, (658, 789, 61, 56))
    slot = Image.new("RGBA", source_slot.size, (0, 0, 0, 0))
    paper = crop(source, (992, 800, 16, 160))
    border = 5
    tile(slot, paper, border, border, slot.width - border * 2, slot.height - border * 2)
    top = source_slot.crop((0, 0, source_slot.width, border))
    left = source_slot.crop((0, border, border, source_slot.height - border))
    right = source_slot.crop((source_slot.width - border, border, source_slot.width, source_slot.height - border))
    slot.alpha_composite(top, (0, 0))
    slot.alpha_composite(left, (0, border))
    slot.alpha_composite(right, (source_slot.width - border, border))
    slot.alpha_composite(ImageOps.flip(top), (0, source_slot.height - border))
    return slot


def create_action_blank(source: Image.Image) -> Image.Image:
    """Build an unlabeled action button from clean source button pixels only."""
    source_button = crop(source, (823, 977, 73, 31))
    button = Image.new("RGBA", source_button.size, (0, 0, 0, 0))
    border = 5
    # A short strip immediately below the top bevel has no baked text.
    clean_interior = source_button.crop((border, border, source_button.width - border, border + 3))
    tile(button, clean_interior, border, border, button.width - border * 2, button.height - border * 2)
    top = source_button.crop((0, 0, source_button.width, border))
    bottom = source_button.crop((0, source_button.height - border, source_button.width, source_button.height))
    left = source_button.crop((0, border, border, source_button.height - border))
    right = source_button.crop((source_button.width - border, border, source_button.width, source_button.height - border))
    button.alpha_composite(top, (0, 0))
    button.alpha_composite(bottom, (0, source_button.height - border))
    button.alpha_composite(left, (0, border))
    button.alpha_composite(right, (source_button.width - border, border))
    return button


def write_outputs() -> dict[str, tuple[int, int]]:
    if not SOURCE.is_file():
        raise FileNotFoundError(f"missing approved town UI source: {SOURCE}")
    source = Image.open(SOURCE).convert("RGBA")
    if source.size != (1024, 1024):
        raise RuntimeError(f"unexpected approved source dimensions: {source.size}")

    outputs: dict[Path, Image.Image] = {}
    for name, donors in HUD_BARS.items():
        frame, fill = create_bar_pair(source, donors)
        outputs[OUTPUT_ROOT / "HUD" / f"hud_{name}_bar_frame.png"] = frame
        outputs[OUTPUT_ROOT / "HUD" / f"hud_{name}_bar_fill.png"] = fill

    outputs[OUTPUT_ROOT / "Backpack" / "backpack_window_frame.png"] = create_backpack_frame(source)
    outputs[OUTPUT_ROOT / "Backpack" / "backpack_slot.png"] = create_slot(source)
    outputs[OUTPUT_ROOT / "Backpack" / "backpack_action_blank.png"] = create_action_blank(source)
    for filename, box in BACKPACK_CROPS.items():
        outputs[OUTPUT_ROOT / "Backpack" / filename] = crop(source, box)

    report: dict[str, tuple[int, int]] = {}
    for path, image in outputs.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        image.save(path)
        report[path.relative_to(OUTPUT_ROOT).as_posix()] = image.size
    return report


def validate_outputs() -> dict[str, tuple[int, int]]:
    report: dict[str, tuple[int, int]] = {}
    for relative in EXPECTED_OUTPUTS:
        path = OUTPUT_ROOT / relative
        if not path.is_file():
            raise FileNotFoundError(f"missing generated UI atom: {path}")
        with Image.open(path) as image:
            if image.mode != "RGBA":
                raise RuntimeError(f"UI atom must retain alpha: {path} ({image.mode})")
            if image.width < 2 or image.height < 2:
                raise RuntimeError(f"UI atom has invalid size: {path} ({image.size})")
            report[relative] = image.size

    # Dynamic composition requires alpha in both paths of every progress bar.
    for relative in (
        "HUD/hud_experience_bar_frame.png",
        "HUD/hud_experience_bar_fill.png",
        "HUD/hud_health_bar_frame.png",
        "HUD/hud_health_bar_fill.png",
    ):
        with Image.open(OUTPUT_ROOT / relative).convert("RGBA") as image:
            alpha_min, alpha_max = image.getchannel("A").getextrema()
            if alpha_min != 0 or alpha_max == 0:
                raise RuntimeError(f"dynamic bar alpha is invalid: {relative}")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="validate existing generated atoms without writing")
    args = parser.parse_args()
    if args.check:
        report = validate_outputs()
        print(json.dumps({"ok": True, "mode": "check", "outputs": report}, ensure_ascii=False))
        return 0
    report = write_outputs()
    report = validate_outputs()
    print(json.dumps({"ok": True, "mode": "write", "outputs": report}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
