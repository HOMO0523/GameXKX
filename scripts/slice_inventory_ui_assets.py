#!/usr/bin/env python3
"""Slice the generated GameXXK inventory UI image sheet into source art PNGs."""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ART_DIR = PROJECT_ROOT / "docs" / "ui" / "inventory" / "source_art"


def crop_resize(sheet: Image.Image, box: tuple[int, int, int, int], size: tuple[int, int]) -> Image.Image:
    return sheet.crop(box).resize(size, Image.Resampling.LANCZOS)


def save(image: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sheet", type=Path, help="Generated inventory UI sheet PNG")
    parser.add_argument("--out-dir", type=Path, default=SOURCE_ART_DIR)
    args = parser.parse_args()

    if not args.sheet.exists():
        raise SystemExit(f"missing generated sheet: {args.sheet}")

    sheet = Image.open(args.sheet).convert("RGBA")
    width, height = sheet.size
    sx = width / 1536.0
    sy = height / 1536.0

    def scaled(box: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
        return (
            round(box[0] * sx),
            round(box[1] * sy),
            round(box[2] * sx),
            round(box[3] * sy),
        )

    args.out_dir.mkdir(parents=True, exist_ok=True)
    copied_sheet = args.out_dir / "inventory_ui_sheet_imagegen_20260710.png"
    shutil.copy2(args.sheet, copied_sheet)

    outputs: dict[str, str] = {}

    crops = {
        "inventory_window_frame.png": (scaled((18, 18, 1032, 682)), (1024, 640)),
        "inventory_confirmation_dialog.png": (scaled((1078, 68, 1485, 414)), (520, 260)),
        "inventory_close_button.png": (scaled((1174, 427, 1378, 631)), (64, 64)),
        "inventory_slot_states.png": (scaled((690, 960, 1055, 1355)), (128, 128)),
    }
    for filename, (box, size) in crops.items():
        output = args.out_dir / filename
        save(crop_resize(sheet, box, size), output)
        outputs[filename] = str(output)

    equipment_sheet = crop_resize(sheet, scaled((20, 716, 490, 884)), (220, 86))
    equipment_path = args.out_dir / "inventory_equipment_slots.png"
    save(equipment_sheet, equipment_path)
    outputs[equipment_path.name] = str(equipment_path)

    action_sheet = crop_resize(sheet, scaled((20, 1252, 610, 1354)), (512, 56))
    action_path = args.out_dir / "inventory_action_buttons.png"
    save(action_sheet, action_path)
    outputs[action_path.name] = str(action_path)

    print(json.dumps({"ok": True, "source_sheet": str(args.sheet), "copied_sheet": str(copied_sheet), "outputs": outputs}, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
