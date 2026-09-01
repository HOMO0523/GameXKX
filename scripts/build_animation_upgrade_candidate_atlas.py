#!/usr/bin/env python3
"""Build isolated RGBA/atlas candidates from the staged black-background MOV files.

This review-only tool never writes to AnimationProcessing/Production or UE Content.
It preserves dark ink by expanding a color-signal silhouette a few pixels before
feathering, then applies one fixed union crop to every frame so authored motion
does not jitter from frame-local recentering.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from collections import deque
from pathlib import Path
from typing import Any

import imageio_ffmpeg
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_STAGING_ROOT = (
    PROJECT_ROOT / "SourceAssets/AnimationProduction/upgrade_20260827_corrected"
)
DEFAULT_REVIEW_ROOT = (
    PROJECT_ROOT / "Saved/HarnessReports/animation-upgrade-20260827-corrected"
)
FRAME_COUNT = 60
GRID_COLUMNS = 8
GRID_ROWS = 8
CELL_SIZE = 512
BOTTOM_MARGIN = 41
CONTENT_PADDING = 41


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relative_to_project(path: Path) -> str:
    return path.resolve().relative_to(PROJECT_ROOT.resolve()).as_posix()


def load_font(size: int) -> ImageFont.ImageFont:
    for candidate in (
        Path(r"C:\Windows\Fonts\msyh.ttc"),
        Path(r"C:\Windows\Fonts\simhei.ttf"),
    ):
        if candidate.is_file():
            return ImageFont.truetype(str(candidate), size)
    return ImageFont.load_default()


def decode_frames(video: Path) -> tuple[dict[str, Any], list[np.ndarray]]:
    reader = imageio_ffmpeg.read_frames(str(video), pix_fmt="rgb24")
    metadata = dict(next(reader))
    source_fps = float(metadata.get("fps") or 30.0)
    duration = float(metadata.get("duration") or 0.0)
    estimated_total = max(1, int(round(duration * source_fps)))
    wanted = [
        int(round(index * max(0, estimated_total - 1) / (FRAME_COUNT - 1)))
        for index in range(FRAME_COUNT)
    ]
    wanted_set = set(wanted)
    selected: dict[int, np.ndarray] = {}
    width, height = metadata["size"]
    try:
        for frame_index, raw in enumerate(reader):
            if frame_index in wanted_set:
                selected[frame_index] = np.frombuffer(raw, dtype=np.uint8).reshape(
                    height, width, 3
                ).copy()
            if frame_index >= wanted[-1]:
                break
    finally:
        reader.close()
    if not selected:
        raise RuntimeError(f"video yielded no frames: {video}")
    available = sorted(selected)
    frames = [
        selected[min(available, key=lambda value: abs(value - target))]
        for target in wanted
    ]
    metadata.update(
        {
            "estimatedSourceFrameCount": estimated_total,
            "selectedSourceFrameIndices": wanted,
        }
    )
    return metadata, frames


def black_background_to_rgba(frame: np.ndarray) -> Image.Image:
    rgb = np.asarray(frame, dtype=np.uint8)
    signal = np.max(rgb, axis=2)

    # Colored/watercolor pixels seed the silhouette. A small dilation preserves
    # adjoining pure-black outline pixels that are indistinguishable from the
    # encoded black background by color alone.
    seed = Image.fromarray(np.where(signal >= 12, 255, 0).astype(np.uint8))
    # HEVC can scatter isolated bright ringing pixels over the otherwise black
    # field. Median filtering removes those islands before the silhouette grows.
    seed = seed.filter(ImageFilter.MedianFilter(5))
    expanded = seed.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.MinFilter(3))
    feathered = expanded.filter(ImageFilter.GaussianBlur(0.85))
    topology_alpha = np.asarray(feathered, dtype=np.float32)
    color_support = np.asarray(expanded, dtype=np.uint8) > 0

    # Premultiplied-looking edge colors provide a second soft-alpha signal.
    ratio = np.clip((signal.astype(np.float32) - 1.0) / 17.0, 0.0, 1.0)
    color_alpha = ratio * ratio * (3.0 - 2.0 * ratio) * 255.0
    color_alpha = np.where(color_support, color_alpha, 0.0)
    alpha = np.maximum(topology_alpha, color_alpha)
    alpha = np.rint(alpha).clip(0.0, 255.0).astype(np.uint8)
    alpha[alpha <= 4] = 0
    rgba = np.dstack((rgb, alpha))
    rgba[rgba[..., 3] == 0, :3] = 0
    return Image.fromarray(rgba)


def union_alpha_box(frames: list[Image.Image]) -> tuple[int, int, int, int]:
    # Use high-confidence colored pixels for placement. Alpha includes a soft
    # recovery band for black ink, and that band can also contain faint codec
    # ringing too sparse to define the authored motion bounds.
    masks = [
        np.max(np.asarray(frame, dtype=np.uint8)[..., :3], axis=2) > 40
        for frame in frames
    ]
    if any(not np.any(mask) for mask in masks):
        raise RuntimeError("at least one candidate frame became fully transparent")
    union = np.logical_or.reduce(masks)
    # Ignore sparse HEVC ringing pixels in the black field. Authored subjects
    # produce dense occupied scanlines; isolated codec flecks do not.
    occupied_rows = np.where(np.count_nonzero(union, axis=1) >= 5)[0]
    occupied_columns = np.where(np.count_nonzero(union, axis=0) >= 5)[0]
    if occupied_rows.size == 0 or occupied_columns.size == 0:
        raise RuntimeError("candidate union has no dense foreground bounds")
    padding = 14
    return (
        max(0, int(occupied_columns[0]) - padding),
        max(0, int(occupied_rows[0]) - padding),
        min(union.shape[1], int(occupied_columns[-1]) + padding + 1),
        min(union.shape[0], int(occupied_rows[-1]) + padding + 1),
    )


def fit_frames(
    frames: list[Image.Image],
) -> tuple[list[Image.Image], dict[str, Any]]:
    box = union_alpha_box(frames)
    source_width = box[2] - box[0]
    source_height = box[3] - box[1]
    scale = min(
        (CELL_SIZE - CONTENT_PADDING * 2) / source_width,
        (CELL_SIZE - BOTTOM_MARGIN - CONTENT_PADDING) / source_height,
    )
    target_size = (
        max(1, round(source_width * scale)),
        max(1, round(source_height * scale)),
    )
    x = (CELL_SIZE - target_size[0]) // 2
    y = CELL_SIZE - BOTTOM_MARGIN - target_size[1]
    fitted: list[Image.Image] = []
    for frame in frames:
        body = frame.crop(box).resize(target_size, Image.Resampling.LANCZOS)
        canvas = Image.new("RGBA", (CELL_SIZE, CELL_SIZE), (0, 0, 0, 0))
        canvas.alpha_composite(body, (x, y))
        fitted.append(canvas)
    return fitted, {
        "sourceUnionBounds": list(box),
        "scale": scale,
        "contentSize": list(target_size),
        "canvasSize": CELL_SIZE,
        "anchor": "bottom_center",
        "bottomMargin": BOTTOM_MARGIN,
    }


def remove_codec_alpha_islands(frame: Image.Image, minimum_fraction: float) -> Image.Image:
    """Drop disconnected HEVC ringing clusters while preserving authored FX islands."""
    rgba = np.asarray(frame, dtype=np.uint8).copy()
    mask = rgba[..., 3] > 4
    height, width = mask.shape
    visited = np.zeros_like(mask, dtype=bool)
    components: list[list[tuple[int, int]]] = []
    for y in range(height):
        for x in range(width):
            if not mask[y, x] or visited[y, x]:
                continue
            component: list[tuple[int, int]] = []
            queue: deque[tuple[int, int]] = deque([(y, x)])
            visited[y, x] = True
            while queue:
                current_y, current_x = queue.pop()
                component.append((current_y, current_x))
                for delta_y, delta_x in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    next_y = current_y + delta_y
                    next_x = current_x + delta_x
                    if (
                        0 <= next_y < height
                        and 0 <= next_x < width
                        and mask[next_y, next_x]
                        and not visited[next_y, next_x]
                    ):
                        visited[next_y, next_x] = True
                        queue.append((next_y, next_x))
            components.append(component)
    if not components:
        raise RuntimeError("candidate alpha cleanup found no foreground")
    largest = max(len(component) for component in components)
    threshold = max(8, int(math.ceil(largest * minimum_fraction)))
    keep = np.zeros_like(mask, dtype=bool)
    for component in components:
        if len(component) < threshold:
            continue
        for y, x in component:
            keep[y, x] = True
    rgba[~keep] = 0
    return Image.fromarray(rgba)


def pack(frames: list[Image.Image], atlas_size: int) -> Image.Image:
    cell = atlas_size // GRID_COLUMNS
    atlas = Image.new("RGBA", (atlas_size, atlas_size), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        resized = frame.resize((cell, cell), Image.Resampling.LANCZOS)
        atlas.alpha_composite(
            resized,
            ((index % GRID_COLUMNS) * cell, (index // GRID_COLUMNS) * cell),
        )
    return atlas


def checker(size: tuple[int, int], cell: int = 16) -> Image.Image:
    width, height = size
    output = Image.new("RGBA", size, (0, 0, 0, 255))
    draw = ImageDraw.Draw(output)
    colors = ((214, 207, 191, 255), (169, 162, 150, 255))
    for y in range(0, height, cell):
        for x in range(0, width, cell):
            draw.rectangle(
                (x, y, min(width, x + cell), min(height, y + cell)),
                fill=colors[((x // cell) + (y // cell)) % 2],
            )
    return output


def make_contact_sheet(frames: list[Image.Image], title: str) -> Image.Image:
    indices = [0, 5, 11, 17, 23, 29, 35, 41, 47, 53, 57, 59]
    thumb = 224
    columns = 4
    rows = math.ceil(len(indices) / columns)
    title_height = 52
    font = load_font(18)
    sheet = Image.new(
        "RGBA", (columns * thumb, title_height + rows * thumb), (24, 24, 28, 255)
    )
    draw = ImageDraw.Draw(sheet)
    draw.text((12, 12), title, fill=(240, 232, 210, 255), font=font)
    for slot, frame_index in enumerate(indices):
        x = (slot % columns) * thumb
        y = title_height + (slot // columns) * thumb
        background = checker((thumb, thumb))
        preview = frames[frame_index].resize((thumb, thumb), Image.Resampling.NEAREST)
        background.alpha_composite(preview)
        sheet.alpha_composite(background, (x, y))
        draw.rectangle((x, y, x + thumb - 1, y + thumb - 1), outline=(72, 68, 61, 255))
        draw.text((x + 7, y + 5), f"{frame_index:02d}", fill=(255, 224, 72, 255), font=font)
    return sheet


def make_old_new_comparison(
    target_id: str | None,
    candidate_frames: list[Image.Image],
    output_path: Path,
) -> str | None:
    if not target_id:
        return None
    old_root = PROJECT_ROOT / "SourceAssets/AnimationProcessing/Production" / target_id / "frames"
    old_paths = [old_root / f"frame_{index:04d}.png" for index in (0, 15, 30, 45, 59)]
    if not all(path.is_file() for path in old_paths):
        return None
    thumb = 256
    label_height = 44
    font = load_font(18)
    sheet = Image.new("RGBA", (5 * thumb, label_height + 2 * thumb), (24, 24, 28, 255))
    draw = ImageDraw.Draw(sheet)
    draw.text((10, 10), f"上排：旧 {target_id}　下排：新候选（透明检查）", fill=(232, 210, 166, 255), font=font)
    for column, (old_path, frame_index) in enumerate(zip(old_paths, (0, 15, 30, 45, 59))):
        old = Image.open(old_path).convert("RGBA").resize((thumb, thumb), Image.Resampling.NEAREST)
        new = candidate_frames[frame_index].resize((thumb, thumb), Image.Resampling.NEAREST)
        old_bg = checker((thumb, thumb))
        new_bg = checker((thumb, thumb))
        old_bg.alpha_composite(old)
        new_bg.alpha_composite(new)
        sheet.alpha_composite(old_bg, (column * thumb, label_height))
        sheet.alpha_composite(new_bg, (column * thumb, label_height + thumb))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output_path, format="PNG", optimize=True)
    return relative_to_project(output_path)


def build_candidate(
    candidate_id: str,
    staging_root: Path,
    review_root: Path,
) -> dict[str, Any]:
    candidate_root = staging_root / candidate_id
    source_manifest_path = candidate_root / "source_manifest.json"
    if not source_manifest_path.is_file():
        raise FileNotFoundError(source_manifest_path)
    source_manifest = json.loads(source_manifest_path.read_text(encoding="utf-8"))
    if source_manifest.get("background", {}).get("classification") != "black":
        raise RuntimeError(f"{candidate_id}: current candidate builder only accepts uniform black sources")
    video = PROJECT_ROOT / source_manifest["stagedVideo"]
    metadata, decoded = decode_frames(video)
    rgba = [black_background_to_rgba(frame) for frame in decoded]
    frames, placement = fit_frames(rgba)
    minimum_component_fraction = 0.005 if candidate_id.startswith((
        "candidate_yue_fire_",
        "cinematic_horse_",
        "cinematic_carriage_",
        "cinematic_hero_collect_",
    )) else 0.0075
    frames = [
        remove_codec_alpha_islands(frame, minimum_component_fraction)
        for frame in frames
    ]

    output_root = candidate_root / "candidate_atlas"
    frames_root = output_root / "frames"
    frames_root.mkdir(parents=True, exist_ok=True)
    for index, frame in enumerate(frames):
        frame.save(frames_root / f"frame_{index:04d}.png", format="PNG", optimize=True)

    atlases: dict[str, dict[str, Any]] = {}
    for label, size in (("2K", 2048), ("1K", 1024)):
        atlas_path = output_root / f"atlas_{label}" / f"{candidate_id}_atlas.png"
        atlas_path.parent.mkdir(parents=True, exist_ok=True)
        pack(frames, size).save(atlas_path, format="PNG", optimize=True)
        atlases[label] = {
            "atlas": relative_to_project(atlas_path),
            "size": [size, size],
            "cellSize": size // GRID_COLUMNS,
            "sha256": sha256(atlas_path),
        }

    candidate_review = review_root / candidate_id
    candidate_review.mkdir(parents=True, exist_ok=True)
    contact_path = candidate_review / "candidate_alpha_contact_sheet.png"
    make_contact_sheet(frames, candidate_id).save(contact_path, format="PNG", optimize=True)
    comparison_path = candidate_review / "old_new_alpha_comparison.png"
    comparison = make_old_new_comparison(
        source_manifest.get("replacementTargetSuggestion"),
        frames,
        comparison_path,
    )

    edge_alpha = {
        "left": max(frame.getchannel("A").crop((0, 0, 1, CELL_SIZE)).getextrema()[1] for frame in frames),
        "top": max(frame.getchannel("A").crop((0, 0, CELL_SIZE, 1)).getextrema()[1] for frame in frames),
        "right": max(frame.getchannel("A").crop((CELL_SIZE - 1, 0, CELL_SIZE, CELL_SIZE)).getextrema()[1] for frame in frames),
        "bottom": max(frame.getchannel("A").crop((0, CELL_SIZE - 1, CELL_SIZE, CELL_SIZE)).getextrema()[1] for frame in frames),
    }
    first = np.asarray(frames[0], dtype=np.int16)
    last = np.asarray(frames[-1], dtype=np.int16)
    source_duration = float(metadata.get("duration") or 0.0)
    if source_duration <= 0.0:
        raise RuntimeError(f"{candidate_id}: source duration is unavailable")
    playback_fps = FRAME_COUNT / source_duration
    manifest = {
        "schemaVersion": 1,
        "status": "candidate_review_only",
        "candidateId": candidate_id,
        "replacementTargetSuggestion": source_manifest.get("replacementTargetSuggestion"),
        "sourceManifest": relative_to_project(source_manifest_path),
        "matting": {
            "mode": "black_background_color_signal_with_outline_dilation",
            "signalThreshold": 12,
            "outlineExpansionPixelsAt1440": 2,
            "featherRadiusAt1440": 0.85,
        },
        "fps": playback_fps,
        "durationSeconds": source_duration,
        "frameCount": FRAME_COUNT,
        "canvasSize": CELL_SIZE,
        "atlasGrid": {
            "columns": GRID_COLUMNS,
            "rows": GRID_ROWS,
            "cellWidth": CELL_SIZE,
            "cellHeight": CELL_SIZE,
        },
        "sourceMetadata": metadata,
        "placement": placement,
        "framesDirectory": relative_to_project(frames_root),
        "contactSheet": relative_to_project(contact_path),
        "comparisonSheet": comparison,
        "atlases": atlases,
        "edgeAlphaMax": edge_alpha,
        "firstLastMeanAbsoluteDifference": round(float(np.mean(np.abs(first - last))), 4),
        "runtimeReplacementStatus": "forbidden_pending_user_confirmation",
    }
    manifest_path = output_root / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("candidate_ids", nargs="+")
    parser.add_argument("--staging-root", type=Path, default=DEFAULT_STAGING_ROOT)
    parser.add_argument("--review-root", type=Path, default=DEFAULT_REVIEW_ROOT)
    args = parser.parse_args()
    results = [
        build_candidate(candidate_id, args.staging_root.resolve(), args.review_root.resolve())
        for candidate_id in args.candidate_ids
    ]
    print(
        json.dumps(
            {
                "ok": True,
                "results": [
                    {
                        "candidateId": result["candidateId"],
                        "contactSheet": result["contactSheet"],
                        "comparisonSheet": result["comparisonSheet"],
                        "edgeAlphaMax": result["edgeAlphaMax"],
                    }
                    for result in results
                ],
            },
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
