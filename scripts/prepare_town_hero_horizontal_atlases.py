#!/usr/bin/env python3
"""Build the approved left-facing town hero locomotion atlases.

The four corrected MOV files share one crop, scale and bottom-center anchor so
Idle -> WalkStart -> WalkLoop -> WalkStop never changes character size or
placement.  Every clip preserves its authored 30 fps source timing.
"""

from __future__ import annotations

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
SOURCE_ROOT = Path(
    r"C:\Users\shxuw\Downloads\动画优化\新建文件夹\8月27号序列帧\主角状态机\基础状态机"
)
OUTPUT_ROOT = PROJECT_ROOT / "SourceAssets/AnimationProcessing/TownHeroHorizontal"
REVIEW_ROOT = PROJECT_ROOT / "Saved/HarnessReports/town-hero-horizontal"

FPS = 30.0
GRID_COLUMNS = 8
GRID_ROWS = 8
ATLAS_SIZE = 2048
CELL_SIZE = ATLAS_SIZE // GRID_COLUMNS
# Keep the new subject close to the legacy town hero's ~184-195 pixel height
# inside its 205px cells, so replacing the clips does not resize the actor.
CONTENT_PADDING = 28
BOTTOM_MARGIN = 28

CLIPS: tuple[dict[str, Any], ...] = (
    {
        "id": "hero_town_idle_left",
        "source": "主角待机.mov",
        "flipbook": "FB_Hero_Town_Idle_Left",
        "paperzd": "PZD_Hero_Town_Idle",
        "loop": True,
    },
    {
        "id": "hero_town_walk_start_left",
        "source": "主角开始行走.mov",
        "flipbook": "FB_Hero_Town_WalkStart_Left",
        "paperzd": "PZD_Hero_Town_WalkStart",
        "loop": False,
    },
    {
        "id": "hero_town_walk_loop_left",
        "source": "主角行走中.mov",
        "flipbook": "FB_Hero_Town_WalkLoop_Left",
        "paperzd": "PZD_Hero_Town_WalkLoop",
        "loop": True,
    },
    {
        "id": "hero_town_walk_stop_left",
        "source": "主角急停.mov",
        "flipbook": "FB_Hero_Town_WalkStop_Left",
        "paperzd": "PZD_Hero_Town_WalkStop",
        "loop": False,
    },
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def project_relative(path: Path) -> str:
    return path.resolve().relative_to(PROJECT_ROOT.resolve()).as_posix()


def load_font(size: int) -> ImageFont.ImageFont:
    for candidate in (
        Path(r"C:\Windows\Fonts\msyh.ttc"),
        Path(r"C:\Windows\Fonts\simhei.ttf"),
    ):
        if candidate.is_file():
            return ImageFont.truetype(str(candidate), size)
    return ImageFont.load_default()


def decode_all_frames(video: Path) -> tuple[dict[str, Any], list[np.ndarray]]:
    reader = imageio_ffmpeg.read_frames(str(video), pix_fmt="rgb24")
    metadata = dict(next(reader))
    width, height = metadata["size"]
    frames: list[np.ndarray] = []
    try:
        for raw in reader:
            frames.append(
                np.frombuffer(raw, dtype=np.uint8).reshape(height, width, 3).copy()
            )
    finally:
        reader.close()
    if not frames:
        raise RuntimeError(f"video yielded no frames: {video}")
    source_fps = float(metadata.get("fps") or 0.0)
    if not math.isclose(source_fps, FPS, rel_tol=0.0, abs_tol=0.01):
        raise RuntimeError(f"{video.name}: expected {FPS:g} fps, got {source_fps:g}")
    if tuple(metadata.get("size") or ()) != (1440, 1440):
        raise RuntimeError(f"{video.name}: expected 1440x1440, got {metadata.get('size')}")
    metadata["decodedFrameCount"] = len(frames)
    metadata["decodedDuration"] = len(frames) / FPS
    return metadata, frames


def black_background_to_rgba(frame: np.ndarray) -> Image.Image:
    rgb = np.asarray(frame, dtype=np.uint8)
    signal = np.max(rgb, axis=2)
    seed = Image.fromarray(np.where(signal >= 12, 255, 0).astype(np.uint8))
    seed = seed.filter(ImageFilter.MedianFilter(5))
    expanded = seed.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.MinFilter(3))
    topology_alpha = np.asarray(
        expanded.filter(ImageFilter.GaussianBlur(0.85)), dtype=np.float32
    )
    color_support = np.asarray(expanded, dtype=np.uint8) > 0
    ratio = np.clip((signal.astype(np.float32) - 1.0) / 17.0, 0.0, 1.0)
    color_alpha = ratio * ratio * (3.0 - 2.0 * ratio) * 255.0
    color_alpha = np.where(color_support, color_alpha, 0.0)
    alpha = np.rint(np.maximum(topology_alpha, color_alpha)).clip(0, 255).astype(np.uint8)
    alpha[alpha <= 4] = 0
    rgba = np.dstack((rgb, alpha))
    rgba[rgba[..., 3] == 0, :3] = 0
    return Image.fromarray(rgba)


def shared_union_box(clips: list[list[Image.Image]]) -> tuple[int, int, int, int]:
    all_frames = [frame for clip in clips for frame in clip]
    masks = [
        np.max(np.asarray(frame, dtype=np.uint8)[..., :3], axis=2) > 40
        for frame in all_frames
    ]
    if any(not np.any(mask) for mask in masks):
        raise RuntimeError("at least one town hero frame became fully transparent")
    union = np.logical_or.reduce(masks)
    rows = np.where(np.count_nonzero(union, axis=1) >= 5)[0]
    columns = np.where(np.count_nonzero(union, axis=0) >= 5)[0]
    if rows.size == 0 or columns.size == 0:
        raise RuntimeError("town hero union has no dense foreground bounds")
    padding = 16
    return (
        max(0, int(columns[0]) - padding),
        max(0, int(rows[0]) - padding),
        min(union.shape[1], int(columns[-1]) + padding + 1),
        min(union.shape[0], int(rows[-1]) + padding + 1),
    )


def fit_shared(
    clips: list[list[Image.Image]], box: tuple[int, int, int, int]
) -> tuple[list[list[Image.Image]], dict[str, Any]]:
    width = box[2] - box[0]
    height = box[3] - box[1]
    scale = min(
        (CELL_SIZE - CONTENT_PADDING * 2) / width,
        (CELL_SIZE - CONTENT_PADDING - BOTTOM_MARGIN) / height,
    )
    target_size = (max(1, round(width * scale)), max(1, round(height * scale)))
    paste_x = (CELL_SIZE - target_size[0]) // 2
    paste_y = CELL_SIZE - BOTTOM_MARGIN - target_size[1]
    output: list[list[Image.Image]] = []
    for clip in clips:
        fitted_clip: list[Image.Image] = []
        for frame in clip:
            body = frame.crop(box).resize(target_size, Image.Resampling.LANCZOS)
            canvas = Image.new("RGBA", (CELL_SIZE, CELL_SIZE), (0, 0, 0, 0))
            canvas.alpha_composite(body, (paste_x, paste_y))
            fitted_clip.append(canvas)
        output.append(fitted_clip)
    return output, {
        "sharedSourceUnionBounds": list(box),
        "scale": scale,
        "contentSize": list(target_size),
        "canvasSize": CELL_SIZE,
        "anchor": "bottom_center",
        "bottomMargin": BOTTOM_MARGIN,
    }


def keep_largest_alpha_component(frame: Image.Image) -> Image.Image:
    """Remove HEVC ringing islands without touching the connected hero silhouette."""
    rgba = np.asarray(frame, dtype=np.uint8).copy()
    mask = rgba[..., 3] > 4
    height, width = mask.shape
    visited = np.zeros_like(mask, dtype=bool)
    largest: list[tuple[int, int]] = []
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
            if len(component) > len(largest):
                largest = component
    if not largest:
        raise RuntimeError("town hero alpha cleanup found no foreground component")
    keep = np.zeros_like(mask, dtype=bool)
    for y, x in largest:
        keep[y, x] = True
    rgba[~keep] = 0
    return Image.fromarray(rgba)


def pack_atlas(frames: list[Image.Image]) -> Image.Image:
    if len(frames) > GRID_COLUMNS * GRID_ROWS:
        raise RuntimeError(f"{len(frames)} frames exceed the 8x8 atlas")
    atlas = Image.new("RGBA", (ATLAS_SIZE, ATLAS_SIZE), (0, 0, 0, 0))
    for index, frame in enumerate(frames):
        atlas.alpha_composite(
            frame,
            ((index % GRID_COLUMNS) * CELL_SIZE, (index // GRID_COLUMNS) * CELL_SIZE),
        )
    return atlas


def checker(size: tuple[int, int], cell: int = 16) -> Image.Image:
    output = Image.new("RGBA", size, (0, 0, 0, 255))
    draw = ImageDraw.Draw(output)
    colors = ((214, 207, 191, 255), (169, 162, 150, 255))
    for y in range(0, size[1], cell):
        for x in range(0, size[0], cell):
            draw.rectangle(
                (x, y, min(size[0], x + cell), min(size[1], y + cell)),
                fill=colors[((x // cell) + (y // cell)) % 2],
            )
    return output


def contact_sheet(clips: list[list[Image.Image]]) -> Image.Image:
    thumb = 192
    label_width = 210
    samples = 6
    font = load_font(18)
    sheet = Image.new(
        "RGBA",
        (label_width + samples * thumb, len(clips) * thumb),
        (24, 24, 28, 255),
    )
    draw = ImageDraw.Draw(sheet)
    for row, (spec, frames) in enumerate(zip(CLIPS, clips)):
        y = row * thumb
        draw.text((12, y + 14), str(spec["id"]), fill=(240, 232, 210, 255), font=font)
        draw.text(
            (12, y + 45),
            f"{len(frames)}f @ {FPS:g}fps\nloop={str(spec['loop']).lower()}",
            fill=(142, 188, 162, 255),
            font=font,
        )
        indices = [round(i * (len(frames) - 1) / (samples - 1)) for i in range(samples)]
        for column, frame_index in enumerate(indices):
            x = label_width + column * thumb
            preview = checker((thumb, thumb))
            preview.alpha_composite(frames[frame_index].resize((thumb, thumb), Image.Resampling.NEAREST))
            sheet.alpha_composite(preview, (x, y))
            draw.rectangle((x, y, x + thumb - 1, y + thumb - 1), outline=(72, 68, 61, 255))
            draw.text((x + 6, y + 5), f"{frame_index:02d}", fill=(255, 224, 72, 255), font=font)
    return sheet


def mean_difference(first: Image.Image, second: Image.Image) -> float:
    a = np.asarray(first, dtype=np.int16)
    b = np.asarray(second, dtype=np.int16)
    return round(float(np.mean(np.abs(a - b))), 4)


def main() -> int:
    if not SOURCE_ROOT.is_dir():
        raise FileNotFoundError(SOURCE_ROOT)

    decoded_clips: list[list[np.ndarray]] = []
    metadata: list[dict[str, Any]] = []
    for spec in CLIPS:
        source = SOURCE_ROOT / str(spec["source"])
        if not source.is_file():
            raise FileNotFoundError(source)
        clip_metadata, frames = decode_all_frames(source)
        clip_metadata["sourceSha256"] = sha256(source)
        decoded_clips.append(frames)
        metadata.append(clip_metadata)

    rgba_clips = [
        [black_background_to_rgba(frame) for frame in frames]
        for frames in decoded_clips
    ]
    box = shared_union_box(rgba_clips)
    fitted_clips, placement = fit_shared(rgba_clips, box)
    fitted_clips = [
        [keep_largest_alpha_component(frame) for frame in clip]
        for clip in fitted_clips
    ]

    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    REVIEW_ROOT.mkdir(parents=True, exist_ok=True)
    clip_reports: list[dict[str, Any]] = []
    for spec, clip_metadata, frames in zip(CLIPS, metadata, fitted_clips):
        clip_root = OUTPUT_ROOT / str(spec["id"])
        frames_root = clip_root / "frames"
        frames_root.mkdir(parents=True, exist_ok=True)
        for index, frame in enumerate(frames):
            frame.save(frames_root / f"frame_{index:03d}.png", format="PNG", optimize=True)
        atlas_path = clip_root / f"{spec['id']}_atlas.png"
        pack_atlas(frames).save(atlas_path, format="PNG", optimize=True)
        edge_alpha = {
            "left": max(frame.getchannel("A").crop((0, 0, 1, CELL_SIZE)).getextrema()[1] for frame in frames),
            "top": max(frame.getchannel("A").crop((0, 0, CELL_SIZE, 1)).getextrema()[1] for frame in frames),
            "right": max(frame.getchannel("A").crop((CELL_SIZE - 1, 0, CELL_SIZE, CELL_SIZE)).getextrema()[1] for frame in frames),
            "bottom": max(frame.getchannel("A").crop((0, CELL_SIZE - 1, CELL_SIZE, CELL_SIZE)).getextrema()[1] for frame in frames),
        }
        if any(edge_alpha.values()):
            raise RuntimeError(f"{spec['id']}: visible alpha touches an atlas cell edge: {edge_alpha}")
        clip_reports.append(
            {
                **spec,
                "sourcePath": str((SOURCE_ROOT / str(spec["source"])).resolve()),
                "sourceSha256": clip_metadata["sourceSha256"],
                "frameCount": len(frames),
                "fps": FPS,
                "duration": len(frames) / FPS,
                "atlas": project_relative(atlas_path),
                "atlasSha256": sha256(atlas_path),
                "framesDirectory": project_relative(frames_root),
                "edgeAlphaMax": edge_alpha,
                "sourceMetadata": clip_metadata,
            }
        )

    review_path = REVIEW_ROOT / "town_hero_horizontal_contact_sheet.png"
    contact_sheet(fitted_clips).save(review_path, format="PNG", optimize=True)
    transitions = {
        "idleLastToWalkStartFirst": mean_difference(fitted_clips[0][-1], fitted_clips[1][0]),
        "walkStartLastToWalkLoopFirst": mean_difference(fitted_clips[1][-1], fitted_clips[2][0]),
        "walkLoopLastToFirst": mean_difference(fitted_clips[2][-1], fitted_clips[2][0]),
        "walkLoopLastToWalkStopFirst": mean_difference(fitted_clips[2][-1], fitted_clips[3][0]),
        "walkStopLastToIdleFirst": mean_difference(fitted_clips[3][-1], fitted_clips[0][0]),
    }
    manifest = {
        "schemaVersion": 1,
        "status": "approved_runtime_source_processed",
        "directionPolicy": "source_faces_left; right_uses_horizontal_component_mirror",
        "locomotionPolicy": "idle_loop -> walk_start_once -> walk_loop; release -> walk_stop_once -> idle",
        "excludedSources": [],
        "timing": {"fps": FPS, "preserveSourceFrameCount": True},
        "atlas": {
            "size": [ATLAS_SIZE, ATLAS_SIZE],
            "columns": GRID_COLUMNS,
            "rows": GRID_ROWS,
            "cellSize": CELL_SIZE,
        },
        "placement": placement,
        "transitionMeanAbsoluteDifference": transitions,
        "reviewContactSheet": project_relative(review_path),
        "clips": clip_reports,
    }
    manifest_path = OUTPUT_ROOT / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(
        json.dumps(
            {
                "ok": True,
                "manifest": project_relative(manifest_path),
                "contactSheet": project_relative(review_path),
                "frameCounts": {report["id"]: report["frameCount"] for report in clip_reports},
                "transitions": transitions,
            },
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
