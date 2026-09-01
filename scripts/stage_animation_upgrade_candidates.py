#!/usr/bin/env python3
"""Stage external optimized animation videos for review without touching runtime atlases.

The script copies every source into a hash-locked ASCII candidate directory and
builds deterministic source-frame contact sheets.  It deliberately does not
remove backgrounds, create production atlases, import UE assets, or overwrite
anything under AnimationProcessing/Production.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import shutil
from pathlib import Path
from typing import Any

import imageio_ffmpeg
import numpy as np
from PIL import Image, ImageDraw, ImageFont


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE_ROOT = Path(
    r"C:\Users\shxuw\Downloads\动画优化\新建文件夹\8月27号序列帧"
)
DEFAULT_STAGING_ROOT = (
    PROJECT_ROOT / "SourceAssets/AnimationProduction/upgrade_20260827_corrected"
)
DEFAULT_REVIEW_ROOT = (
    PROJECT_ROOT / "Saved/HarnessReports/animation-upgrade-20260827-corrected"
)
SAMPLE_COUNT = 9
THUMB_SIZE = 256
TITLE_HEIGHT = 46


# Runtime targets are suggestions only.  Empty targets are new cinematics or
# unidentified sources and must be named with the user before atlas creation.
SOURCE_SPECS: dict[str, dict[str, Any]] = {
    "怪物们/黄鼠狼待机.mov": {
        "id": "enemy_03_weasel_idle_candidate",
        "target": "enemy_03_weasel_idle",
    },
    "怪物们/黄鼠狼攻击.mov": {
        "id": "enemy_03_weasel_attack_candidate",
        "target": "enemy_03_weasel_attack",
    },
    "怪物们/鸡待机.mov": {
        "id": "enemy_01_rooster_idle_candidate",
        "target": "enemy_01_rooster_idle",
    },
    "怪物们/鸡攻击.mov": {
        "id": "enemy_01_rooster_attack_candidate",
        "target": "enemy_01_rooster_attack",
    },
    "怪物们/狼待机.mov": {
        "id": "enemy_07_graywolf_idle_candidate",
        "target": "enemy_07_graywolf_idle",
    },
    "怪物们/狼王攻击.mov": {
        "id": "enemy_11_graymane_attack_candidate",
        "target": "enemy_11_graymane_attack",
    },
    "怪物们/大青蛙动一动.mov": {
        "id": "enemy_16_toad_idle_candidate",
        "target": "enemy_16_toad_idle",
    },
    "怪物们/鹿待机.mov": {
        "id": "enemy_18_deer_idle_candidate",
        "target": "enemy_18_deer_idle",
    },
    "怪物们/鹿攻击.mov": {
        "id": "enemy_18_deer_attack_candidate",
        "target": "enemy_18_deer_attack",
    },
    "怪物们/铁公鸡待机.mov": {
        "id": "enemy_05_ironfeather_idle_candidate",
        "target": "enemy_05_ironfeather_idle",
    },
    "马和马车/马待机.mov": {"id": "cinematic_horse_idle", "target": "cinematic_horse_idle"},
    "马和马车/马起跑奔跑急停.mov": {
        "id": "cinematic_horse_start_run_stop",
        "target": "cinematic_horse_start_run_stop",
    },
    "马和马车/马车奔跑急停.mov": {
        "id": "cinematic_carriage_run_stop",
        "target": "cinematic_carriage_run_stop",
    },
    "马和马车/马车急停后待机.mov": {
        "id": "cinematic_carriage_post_stop_idle",
        "target": "cinematic_carriage_post_stop_idle",
    },
    "月火/月火出场.mov": {"id": "candidate_yue_fire_intro", "target": "character_09_yue_bai_intro"},
    "月火/月火离场.mov": {"id": "candidate_yue_fire_outro", "target": "character_09_yue_bai_outro"},
    "月火/月火idle.mov": {"id": "candidate_yue_fire_idle", "target": "character_09_yue_bai_idle"},
    "主角状态机/主角攻击/主角战斗待机.mov": {
        "id": "character_00_hero_combat_idle_candidate",
        "target": "character_00_hero_idle",
    },
    "主角状态机/主角攻击/主角打拳.mov": {
        "id": "character_00_hero_attack_punch_candidate",
        "target": "character_00_hero_attack",
    },
    "主角状态机/主角攻击/主角踢腿.mov": {
        "id": "character_00_hero_attack_kick_candidate",
        "target": "character_00_hero_attack",
    },
    "主角状态机/基础状态机/主角待机.mov": {
        "id": "character_00_hero_state_idle_candidate",
        "target": "hero_town_idle_left",
    },
    "主角状态机/基础状态机/主角开始行走.mov": {
        "id": "character_00_hero_walk_start_candidate",
        "target": "hero_town_walk_start_left",
    },
    "主角状态机/基础状态机/主角行走中.mov": {
        "id": "character_00_hero_walk_loop_candidate",
        "target": "hero_town_walk_loop_left",
    },
    "主角状态机/基础状态机/主角急停.mov": {
        "id": "character_00_hero_walk_stop_candidate",
        "target": "hero_town_walk_stop_left",
    },
    "主角状态机/角色采集物品需要弹出物品UI-1.mov": {
        "id": "cinematic_hero_collect_item_with_ui",
        "target": "hero_town_collect_item",
    },
    "主角状态机/主角随机播放呼吸状态机/主角调整背包.mov": {
        "id": "cinematic_hero_adjust_backpack",
        "target": "hero_town_adjust_backpack",
    },
    "主角状态机/主角随机播放呼吸状态机/主角深呼吸.mov": {
        "id": "cinematic_hero_deep_breath",
        "target": "hero_town_deep_breath",
    },
}

# Explicitly excluded by the user: this is an older deer clip and must not enter
# staging, atlas generation, UE import, or runtime mapping. The external source
# file is left untouched.
IGNORED_SOURCES = {
    "怪物们/鹿鞠躬.mov",
}


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


def decode_samples(video: Path) -> tuple[dict[str, Any], list[np.ndarray]]:
    reader = imageio_ffmpeg.read_frames(str(video), pix_fmt="rgb24")
    metadata = dict(next(reader))
    source_fps = float(metadata.get("fps") or 30.0)
    duration = float(metadata.get("duration") or 0.0)
    estimated_total = max(1, int(round(duration * source_fps)))
    wanted = [
        int(round(index * max(0, estimated_total - 1) / max(1, SAMPLE_COUNT - 1)))
        for index in range(SAMPLE_COUNT)
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
            "sampledSourceFrameIndices": wanted,
        }
    )
    return metadata, frames


def analyze_background(frame: np.ndarray) -> dict[str, Any]:
    height, width, _ = frame.shape
    inset = max(1, min(width, height) // 64)
    samples = np.asarray(
        [
            frame[inset, inset],
            frame[inset, width - inset - 1],
            frame[height - inset - 1, inset],
            frame[height - inset - 1, width - inset - 1],
        ],
        dtype=np.float32,
    )
    median = np.median(samples, axis=0)
    spread = np.max(np.abs(samples - median))
    red, green, blue = median.tolist()
    if red >= 190 and blue >= 190 and green <= 110:
        classification = "magenta_chroma"
    elif green >= 170 and green >= red + 45 and green >= blue + 35:
        classification = "green_chroma"
    elif max(red, green, blue) <= 28:
        classification = "black"
    elif min(red, green, blue) >= 225:
        classification = "white"
    else:
        classification = "other"
    if spread > 22:
        classification += "_nonuniform"
    return {
        "cornerMedianRgb": [int(round(value)) for value in median],
        "cornerMaximumDeviation": round(float(spread), 3),
        "classification": classification,
        "mattingStatus": "pending_visual_review",
    }


def fit_preview(frame: np.ndarray) -> Image.Image:
    source = Image.fromarray(frame)
    scale = min(THUMB_SIZE / source.width, THUMB_SIZE / source.height)
    resized = source.resize(
        (max(1, round(source.width * scale)), max(1, round(source.height * scale))),
        Image.Resampling.LANCZOS,
    )
    canvas = Image.new("RGB", (THUMB_SIZE, THUMB_SIZE), (30, 30, 34))
    canvas.paste(
        resized,
        ((THUMB_SIZE - resized.width) // 2, (THUMB_SIZE - resized.height) // 2),
    )
    return canvas


def make_contact_sheet(
    title: str,
    frames: list[np.ndarray],
    columns: int = 3,
) -> Image.Image:
    font = load_font(18)
    rows = math.ceil(len(frames) / columns)
    sheet = Image.new(
        "RGB",
        (columns * THUMB_SIZE, TITLE_HEIGHT + rows * THUMB_SIZE),
        (24, 24, 28),
    )
    draw = ImageDraw.Draw(sheet)
    draw.text((12, 10), title, fill=(238, 230, 210), font=font)
    for index, frame in enumerate(frames):
        preview = fit_preview(frame)
        x = (index % columns) * THUMB_SIZE
        y = TITLE_HEIGHT + (index // columns) * THUMB_SIZE
        sheet.paste(preview, (x, y))
        draw.rectangle((x, y, x + THUMB_SIZE - 1, y + THUMB_SIZE - 1), outline=(92, 88, 80))
        draw.text((x + 8, y + 6), f"{index + 1:02d}", fill=(245, 220, 92), font=font)
    return sheet


def stage_one(
    source_root: Path,
    staging_root: Path,
    review_root: Path,
    relative_path: str,
    spec: dict[str, Any],
) -> tuple[dict[str, Any], Image.Image]:
    source = source_root / Path(relative_path)
    if not source.is_file():
        raise FileNotFoundError(source)
    candidate_id = str(spec["id"])
    candidate_root = staging_root / candidate_id
    raw_root = candidate_root / "raw"
    raw_root.mkdir(parents=True, exist_ok=True)
    staged_video = raw_root / "source.mov"
    source_hash = sha256(source)
    if not staged_video.is_file() or sha256(staged_video) != source_hash:
        shutil.copy2(source, staged_video)

    metadata, frames = decode_samples(source)
    background = analyze_background(frames[0])
    candidate_review_root = review_root / candidate_id
    candidate_review_root.mkdir(parents=True, exist_ok=True)
    contact_path = candidate_review_root / "source_contact_sheet.png"
    contact = make_contact_sheet(relative_path, frames)
    contact.save(contact_path, format="PNG", optimize=True)
    candidate_manifest = {
        "schemaVersion": 1,
        "status": "review_pending",
        "candidateId": candidate_id,
        "sourceRelativePath": relative_path,
        "sourceAbsolutePath": str(source.resolve()),
        "sourceVideoSha256": source_hash,
        "stagedVideo": relative_to_project(staged_video),
        "replacementTargetSuggestion": spec.get("target"),
        "sourceMetadata": {
            "size": list(metadata.get("size") or []),
            "fps": metadata.get("fps"),
            "duration": metadata.get("duration"),
            "codec": metadata.get("codec"),
            "audioCodec": metadata.get("audio_codec"),
            "estimatedSourceFrameCount": metadata.get("estimatedSourceFrameCount"),
        },
        "background": background,
        "reviewContactSheet": relative_to_project(contact_path),
        "atlasStatus": "not_built_pending_mapping_and_visual_confirmation",
        "runtimeReplacementStatus": "forbidden_pending_user_confirmation",
    }
    manifest_path = candidate_root / "source_manifest.json"
    manifest_path.write_text(
        json.dumps(candidate_manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    return candidate_manifest, fit_preview(frames[len(frames) // 2])


def build_global_sheet(entries: list[tuple[dict[str, Any], Image.Image]]) -> Image.Image:
    columns = 4
    rows = math.ceil(len(entries) / columns)
    font = load_font(17)
    tile_height = THUMB_SIZE + 72
    sheet = Image.new("RGB", (columns * THUMB_SIZE, rows * tile_height), (20, 20, 24))
    draw = ImageDraw.Draw(sheet)
    for index, (entry, preview) in enumerate(entries):
        x = (index % columns) * THUMB_SIZE
        y = (index // columns) * tile_height
        sheet.paste(preview, (x, y))
        draw.rectangle((x, y, x + THUMB_SIZE - 1, y + THUMB_SIZE - 1), outline=(92, 88, 80))
        candidate_id = str(entry["candidateId"])
        target = entry.get("replacementTargetSuggestion") or "new / mapping pending"
        draw.text((x + 6, y + THUMB_SIZE + 5), candidate_id, fill=(236, 226, 202), font=font)
        draw.text((x + 6, y + THUMB_SIZE + 34), str(target), fill=(142, 188, 162), font=font)
    return sheet


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, default=DEFAULT_SOURCE_ROOT)
    parser.add_argument("--staging-root", type=Path, default=DEFAULT_STAGING_ROOT)
    parser.add_argument("--review-root", type=Path, default=DEFAULT_REVIEW_ROOT)
    args = parser.parse_args()

    source_root = args.source_root.resolve()
    staging_root = args.staging_root.resolve()
    review_root = args.review_root.resolve()
    if not source_root.is_dir():
        raise FileNotFoundError(source_root)
    staging_root.mkdir(parents=True, exist_ok=True)
    review_root.mkdir(parents=True, exist_ok=True)

    discovered = {
        path.relative_to(source_root).as_posix()
        for path in source_root.rglob("*")
        if path.is_file() and path.suffix.lower() in {".mov", ".mp4", ".avi", ".webm"}
    }
    expected = set(SOURCE_SPECS)
    if discovered != expected | IGNORED_SOURCES:
        raise RuntimeError(
            json.dumps(
                {
                    "missing": sorted(expected - discovered),
                    "unexpected": sorted(discovered - expected - IGNORED_SOURCES),
                    "ignoredMissing": sorted(IGNORED_SOURCES - discovered),
                },
                ensure_ascii=False,
            )
        )

    staged: list[tuple[dict[str, Any], Image.Image]] = []
    for relative_path in sorted(SOURCE_SPECS):
        staged.append(
            stage_one(
                source_root,
                staging_root,
                review_root,
                relative_path,
                SOURCE_SPECS[relative_path],
            )
        )
    global_contact = review_root / "all_candidates_contact_sheet.png"
    build_global_sheet(staged).save(global_contact, format="PNG", optimize=True)
    manifest = {
        "schemaVersion": 1,
        "status": "review_pending",
        "sourceRoot": str(source_root),
        "candidateCount": len(staged),
        "ignoredSources": sorted(IGNORED_SOURCES),
        "globalContactSheet": relative_to_project(global_contact),
        "runtimeReplacementStatus": "forbidden_pending_user_confirmation",
        "candidates": [entry for entry, _ in staged],
    }
    manifest_path = staging_root / "manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(
        json.dumps(
            {
                "ok": True,
                "candidateCount": len(staged),
                "manifest": relative_to_project(manifest_path),
                "contactSheet": relative_to_project(global_contact),
            },
            ensure_ascii=False,
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
