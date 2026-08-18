#!/usr/bin/env python3
"""Extract one Dreamina walk loop and build the project's 4K/2K/1K atlases.

This is an isolated review pipeline. It follows the existing battle-animation
contract (12 fps, 60 frames, 8x8 atlas, bottom-center 512 px cells) but writes
only beneath the caller-provided review directory; it never touches the
approved Production atlases or Unreal assets.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path
from typing import Any

import imageio_ffmpeg
import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
FPS = 12
FRAME_COUNT = 60
CELL_SIZE_4K = 512
GRID_COLUMNS = 8
GRID_ROWS = 8
BOTTOM_MARGIN = 41


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _as_rgba(rgb: np.ndarray) -> Image.Image:
    """Remove magenta chroma while retaining a small anti-aliased edge."""
    if rgb.ndim != 3 or rgb.shape[2] != 3:
        raise ValueError(f"expected HxWx3 RGB frame, got {rgb.shape}")
    rgb = np.asarray(rgb, dtype=np.uint8)
    h, w, _ = rgb.shape
    corners = np.asarray(
        [rgb[0, 0], rgb[0, w - 1], rgb[h - 1, 0], rgb[h - 1, w - 1]],
        dtype=np.float32,
    )
    key = np.median(corners, axis=0)
    delta = np.abs(rgb.astype(np.float32) - key)
    distance = np.max(delta, axis=2)

    # Match the shared imagegen chroma-key helper: soft alpha plus a second
    # dominance guard for pink pixels, then decontaminate the red/blue spill.
    transparent_threshold = 24.0
    opaque_threshold = 108.0
    ratio = np.clip(
        (distance - transparent_threshold) / (opaque_threshold - transparent_threshold),
        0.0,
        1.0,
    )
    soft_alpha = ratio * ratio * (3.0 - 2.0 * ratio) * 255.0
    key_strength = np.minimum(rgb[..., 0].astype(np.float32), rgb[..., 2].astype(np.float32))
    non_key_strength = rgb[..., 1].astype(np.float32)
    dominance = key_strength - non_key_strength
    dominance_alpha = np.where(
        dominance > 0.0,
        (1.0 - np.minimum(1.0, dominance / np.maximum(1.0, 255.0 - non_key_strength))) * 255.0,
        255.0,
    )
    key_like = (distance <= 32.0) | (dominance >= 16.0)
    alpha = np.where(key_like, np.minimum(soft_alpha, dominance_alpha), 255.0)
    alpha = np.rint(alpha).clip(0.0, 255.0).astype(np.uint8)
    alpha[alpha <= 8] = 0
    rgba_rgb = rgb.copy()
    spill = key_like & (alpha < 252)
    cap = np.maximum(0.0, rgb[..., 1].astype(np.float32) - 1.0)
    rgba_rgb[..., 0] = np.where(spill & (rgba_rgb[..., 0] > cap), cap, rgba_rgb[..., 0])
    rgba_rgb[..., 2] = np.where(spill & (rgba_rgb[..., 2] > cap), cap, rgba_rgb[..., 2])
    rgba = np.dstack((rgba_rgb, alpha))
    rgba[rgba[..., 3] == 0, :3] = 0
    return Image.fromarray(rgba)


def _decode_selected(video: Path) -> tuple[dict[str, Any], list[np.ndarray]]:
    reader = imageio_ffmpeg.read_frames(str(video), pix_fmt="rgb24")
    metadata = next(reader)
    source_fps = float(metadata.get("fps") or 24.0)
    estimated_total = max(2, int(round(float(metadata.get("duration") or 5.0) * source_fps)))
    wanted = {
        int(round(index * (estimated_total - 1) / (FRAME_COUNT - 1)))
        for index in range(FRAME_COUNT)
    }
    selected: dict[int, np.ndarray] = {}
    width, height = metadata["size"]
    try:
        for frame_index, raw in enumerate(reader):
            if frame_index not in wanted:
                continue
            selected[frame_index] = np.frombuffer(raw, dtype=np.uint8).reshape(height, width, 3).copy()
    finally:
        reader.close()

    if not selected:
        raise RuntimeError(f"video yielded no RGB frames: {video}")
    ordered: list[np.ndarray] = []
    selected_indices = sorted(selected)
    for index in range(FRAME_COUNT):
        target = int(round(index * (estimated_total - 1) / (FRAME_COUNT - 1)))
        nearest = min(selected_indices, key=lambda candidate: abs(candidate - target))
        ordered.append(selected[nearest])
    metadata = dict(metadata)
    metadata.update({"selectedFrameCount": len(ordered), "estimatedSourceFrameCount": estimated_total})
    return metadata, ordered


def _fit_frames(raw_frames: list[Image.Image], cell_size: int = CELL_SIZE_4K) -> list[Image.Image]:
    boxes = [frame.getchannel("A").getbbox() for frame in raw_frames]
    if any(box is None for box in boxes):
        raise RuntimeError("at least one walk frame became fully transparent")
    concrete = [box for box in boxes if box is not None]
    max_width = max(box[2] - box[0] for box in concrete)
    max_height = max(box[3] - box[1] for box in concrete)
    width_budget = cell_size - BOTTOM_MARGIN * 2
    height_budget = cell_size - BOTTOM_MARGIN
    scale = min(width_budget / max_width, height_budget / max_height)
    if scale <= 0:
        raise RuntimeError("walk frame scale is non-positive")

    fitted: list[Image.Image] = []
    for frame, box in zip(raw_frames, concrete):
        cropped = frame.crop(box)
        target = (
            max(1, round(cropped.width * scale)),
            max(1, round(cropped.height * scale)),
        )
        body = cropped.resize(target, Image.Resampling.LANCZOS)
        body_pixels = np.asarray(body).copy()
        body_alpha = body_pixels[..., 3]
        residual_key = (
            (body_alpha <= 8)
            | ((body_pixels[..., 0] >= 200) & (body_pixels[..., 2] >= 200) & (body_pixels[..., 1] <= 24))
        )
        body_pixels[residual_key] = (0, 0, 0, 0)
        body = Image.fromarray(body_pixels)
        canvas = Image.new("RGBA", (cell_size, cell_size), (0, 0, 0, 0))
        x = (cell_size - body.width) // 2
        y = cell_size - BOTTOM_MARGIN - body.height
        if x < 0 or y < 0 or x + body.width > cell_size or y + body.height > cell_size:
            raise RuntimeError(f"walk frame exceeds {cell_size}px cell: {body.size}")
        canvas.alpha_composite(body, (x, y))
        fitted.append(canvas)
    return fitted


def _pack(frames: list[Image.Image], atlas_size: int) -> Image.Image:
    cell = atlas_size // GRID_COLUMNS
    atlas = Image.new("RGBA", (atlas_size, atlas_size), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        resized = frame.resize((cell, cell), Image.Resampling.LANCZOS)
        x = (index % GRID_COLUMNS) * cell
        y = (index // GRID_COLUMNS) * cell
        atlas.alpha_composite(resized, (x, y))
    return atlas


def _contact_sheet(frames: list[Image.Image]) -> Image.Image:
    thumb = 160
    columns = 10
    rows = math.ceil(len(frames) / columns)
    sheet = Image.new("RGBA", (columns * thumb, rows * thumb), (24, 24, 28, 255))
    for index, frame in enumerate(frames):
        preview = frame.resize((thumb, thumb), Image.Resampling.NEAREST)
        sheet.alpha_composite(preview, ((index % columns) * thumb, (index // columns) * thumb))
    return sheet


def _relative(path: Path) -> str:
    return path.resolve().relative_to(ROOT.resolve()).as_posix()


def build(video: Path, reference: Path, out_root: Path) -> dict[str, Any]:
    metadata, selected = _decode_selected(video)
    width, height = metadata["size"]
    raw = [_as_rgba(frame) for frame in selected]

    with Image.open(reference) as reference_image:
        ref_rgb = np.asarray(reference_image.convert("RGB").resize((width, height), Image.Resampling.LANCZOS))
    reference_rgba = _as_rgba(ref_rgb)
    raw[0] = reference_rgba.copy()
    raw[-1] = reference_rgba.copy()
    fitted = _fit_frames(raw)
    fitted[-1] = fitted[0].copy()

    frames_dir = out_root / "frames"
    contact_path = out_root / "contact_sheet.png"
    atlas_paths: dict[str, Path] = {}
    frames_dir.mkdir(parents=True, exist_ok=True)
    for index, frame in enumerate(fitted):
        frame.save(frames_dir / f"frame_{index:04d}.png", format="PNG", optimize=True)
    _contact_sheet(fitted).save(contact_path, format="PNG", optimize=True)

    for label, size in (("4K", 4096), ("2K", 2048), ("1K", 1024)):
        atlas_dir = out_root / f"atlas_{label}"
        atlas_dir.mkdir(parents=True, exist_ok=True)
        path = atlas_dir / "character_00_hero_walk_left_atlas.png"
        _pack(fitted, size).save(path, format="PNG", optimize=True)
        atlas_paths[label] = path

    recipe = {
        "schemaVersion": 1,
        "sourceVideo": _relative(video),
        "referenceFrame": _relative(reference),
        "orientation": "left",
        "matting": {
            "mode": "chroma",
            "key": "#FF00FF",
            "hardDistance": 108,
            "softDistance": 58,
            "spillSuppress": True,
        },
        "output": {
            "fps": FPS,
            "durationSeconds": 5,
            "frameCount": FRAME_COUNT,
            "canvasSize": CELL_SIZE_4K,
            "atlasGrid": {"columns": GRID_COLUMNS, "rows": GRID_ROWS},
            "anchor": "bottom_center",
            "bottomMargin": BOTTOM_MARGIN,
            "firstLastForcedIdentical": True,
        },
    }
    (out_root / "recipe.json").write_text(json.dumps(recipe, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    manifest = {
        "schemaVersion": 1,
        "status": "review-only",
        "assetName": "character_00_hero_walk_left",
        "direction": "left",
        "sourceVideo": _relative(video),
        "referenceFrame": _relative(reference),
        "sourceVideoSha256": sha256(video),
        "referenceFrameSha256": sha256(reference),
        "fps": FPS,
        "durationSeconds": 5,
        "frameCount": FRAME_COUNT,
        "canvasSize": CELL_SIZE_4K,
        "atlasGrid": {"columns": GRID_COLUMNS, "rows": GRID_ROWS, "cellWidth": CELL_SIZE_4K, "cellHeight": CELL_SIZE_4K},
        "framesDirectory": _relative(frames_dir),
        "contactSheet": _relative(contact_path),
        "firstLastIdentical": fitted[0].tobytes() == fitted[-1].tobytes(),
        "firstFrameSha256": hashlib.sha256(fitted[0].tobytes()).hexdigest(),
        "lastFrameSha256": hashlib.sha256(fitted[-1].tobytes()).hexdigest(),
        "uniqueFrameCount": len({frame.tobytes() for frame in fitted}),
        "sourceVideoMetadata": metadata,
        "variants": {
            label: {
                "atlas": _relative(path),
                "size": list(Image.open(path).size),
                "cellSize": size // GRID_COLUMNS,
                "sha256": sha256(path),
            }
            for label, (path, size) in ((label, (atlas_paths[label], int(label[:-1]) * 1024)) for label in atlas_paths)
        },
    }
    (out_root / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--video", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--out-root", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = build(args.video.resolve(), args.reference.resolve(), (ROOT / args.out_root).resolve() if not args.out_root.is_absolute() else args.out_root.resolve())
    except Exception as exc:
        print(json.dumps({"ok": False, "error": str(exc)}, ensure_ascii=False))
        return 1
    print(json.dumps({"ok": True, "manifest": result}, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
