#!/usr/bin/env python3
"""Normalize the four approved RGBA hit-effect sheets into isolated 8x8 atlases."""

from __future__ import annotations

import hashlib
import json
import math
import shutil
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = Path(r"C:\Users\shxuw\Downloads\动画优化\特效序列帧")
OUTPUT_ROOT = PROJECT_ROOT / "SourceAssets/AnimationProcessing/HitEffects"
REVIEW_ROOT = PROJECT_ROOT / "Saved/HarnessReports/battle-hit-effects"
GRID_COLUMNS = 8
GRID_ROWS = 8
NORMALIZED_CELL_SIZE = 512

SPECS = (
    ("battle_hit_effect_01", "huaban-7251655128.png", 3, 2, 6),
    ("battle_hit_effect_02", "huaban-7251655302.png", 3, 3, 9),
    ("battle_hit_effect_03", "huaban-7251655374.png", 3, 3, 9),
    ("battle_hit_effect_04", "huaban-7251655489.png", 4, 3, 12),
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relative(path: Path) -> str:
    return path.resolve().relative_to(PROJECT_ROOT.resolve()).as_posix()


def font(size: int) -> ImageFont.ImageFont:
    path = Path(r"C:\Windows\Fonts\msyh.ttc")
    return ImageFont.truetype(str(path), size) if path.is_file() else ImageFont.load_default()


def slice_sheet(
    sheet: Image.Image,
    columns: int,
    rows: int,
    frame_count: int,
) -> list[Image.Image]:
    frames: list[Image.Image] = []
    for index in range(frame_count):
        column = index % columns
        row = index // columns
        left = round(column * sheet.width / columns)
        right = round((column + 1) * sheet.width / columns)
        top = round(row * sheet.height / rows)
        bottom = round((row + 1) * sheet.height / rows)
        cell = sheet.crop((left, top, right, bottom))
        scale = min(
            NORMALIZED_CELL_SIZE / max(1, cell.width),
            NORMALIZED_CELL_SIZE / max(1, cell.height),
        )
        resized = cell.resize(
            (max(1, round(cell.width * scale)), max(1, round(cell.height * scale))),
            Image.Resampling.LANCZOS,
        )
        canvas = Image.new("RGBA", (NORMALIZED_CELL_SIZE, NORMALIZED_CELL_SIZE), (0, 0, 0, 0))
        canvas.alpha_composite(
            resized,
            ((NORMALIZED_CELL_SIZE - resized.width) // 2, (NORMALIZED_CELL_SIZE - resized.height) // 2),
        )
        frames.append(canvas)
    return frames


def pack(frames: list[Image.Image], atlas_size: int) -> Image.Image:
    cell_size = atlas_size // GRID_COLUMNS
    atlas = Image.new("RGBA", (atlas_size, atlas_size), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        atlas.alpha_composite(
            frame.resize((cell_size, cell_size), Image.Resampling.LANCZOS),
            ((index % GRID_COLUMNS) * cell_size, (index // GRID_COLUMNS) * cell_size),
        )
    return atlas


def on_dark(frame: Image.Image, size: int) -> Image.Image:
    background = Image.new("RGBA", (size, size), (29, 30, 34, 255))
    background.alpha_composite(frame.resize((size, size), Image.Resampling.NEAREST))
    return background


def build_contact(rows: list[tuple[str, list[Image.Image]]]) -> Image.Image:
    thumb = 150
    label_width = 280
    row_height = thumb + 16
    maximum_frames = max(len(frames) for _, frames in rows)
    sheet = Image.new(
        "RGBA",
        (label_width + maximum_frames * thumb, len(rows) * row_height),
        (20, 21, 24, 255),
    )
    draw = ImageDraw.Draw(sheet)
    title_font = font(22)
    index_font = font(16)
    for row_index, (asset_id, frames) in enumerate(rows):
        y = row_index * row_height
        draw.text((12, y + 52), asset_id, fill=(238, 224, 194, 255), font=title_font)
        for frame_index, frame in enumerate(frames):
            x = label_width + frame_index * thumb
            sheet.alpha_composite(on_dark(frame, thumb), (x, y))
            draw.rectangle((x, y, x + thumb - 1, y + thumb - 1), outline=(80, 78, 72, 255))
            draw.text((x + 5, y + 4), str(frame_index), fill=(255, 222, 70, 255), font=index_font)
    return sheet


def main() -> int:
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    REVIEW_ROOT.mkdir(parents=True, exist_ok=True)
    contact_rows: list[tuple[str, list[Image.Image]]] = []
    records = []
    for asset_id, filename, source_columns, source_rows, frame_count in SPECS:
        source = SOURCE_ROOT / filename
        if not source.is_file():
            raise FileNotFoundError(source)
        with Image.open(source) as opened:
            sheet = opened.convert("RGBA")
        if sheet.getchannel("A").getextrema() != (0, 255):
            raise RuntimeError(f"{filename}: source has no real alpha range")
        frames = slice_sheet(sheet, source_columns, source_rows, frame_count)
        asset_root = OUTPUT_ROOT / asset_id
        frames_root = asset_root / "frames"
        frames_root.mkdir(parents=True, exist_ok=True)
        staged_source = asset_root / "source.png"
        shutil.copy2(source, staged_source)
        for index, frame in enumerate(frames):
            frame.save(frames_root / f"frame_{index:02d}.png", format="PNG", optimize=True)
        variants = {}
        for label, atlas_size in (("2K", 2048), ("1K", 1024)):
            atlas_path = asset_root / f"atlas_{label}" / f"{asset_id}_atlas.png"
            atlas_path.parent.mkdir(parents=True, exist_ok=True)
            pack(frames, atlas_size).save(atlas_path, format="PNG", optimize=True)
            variants[label] = {
                "atlas": relative(atlas_path),
                "size": [atlas_size, atlas_size],
                "cellSize": atlas_size // GRID_COLUMNS,
                "sha256": sha256(atlas_path),
            }
        record = {
            "assetId": asset_id,
            "status": "review_pending",
            "sourceFile": str(source),
            "stagedSource": relative(staged_source),
            "sourceSha256": sha256(source),
            "sourceSize": list(sheet.size),
            "sourceGrid": {"columns": source_columns, "rows": source_rows},
            "frameCount": frame_count,
            "runtimeGrid": {"columns": GRID_COLUMNS, "rows": GRID_ROWS},
            "framesPerSecond": 30.0,
            "runtimeReplacementStatus": "new-vfx-pending-user-confirmation",
            "variants": variants,
        }
        (asset_root / "manifest.json").write_text(
            json.dumps(record, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        records.append(record)
        contact_rows.append((asset_id, frames))

    contact_path = REVIEW_ROOT / "battle_hit_effects_contact_sheet.png"
    build_contact(contact_rows).save(contact_path, format="PNG", optimize=True)
    manifest = {
        "schemaVersion": 1,
        "status": "review_pending",
        "selectionPolicy": "stable event id modulo four",
        "anchorPolicy": "centered on the hit enemy",
        "contactSheet": relative(contact_path),
        "effects": records,
    }
    manifest_path = OUTPUT_ROOT / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(json.dumps({"ok": True, "manifest": relative(manifest_path), "contactSheet": relative(contact_path)}, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
