#!/usr/bin/env python3
"""Build grouped visual mapping boards for the corrected 2026-08-27 animation batch."""

from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Any

from PIL import Image, ImageDraw, ImageFont


PROJECT_ROOT = Path(__file__).resolve().parents[1]
STAGING_ROOT = PROJECT_ROOT / "SourceAssets/AnimationProduction/upgrade_20260827_corrected"
REVIEW_ROOT = PROJECT_ROOT / "Saved/HarnessReports/animation-upgrade-20260827-corrected/mapping-review"
TILE_WIDTH = 320
TILE_HEIGHT = 360
PREVIEW_SIZE = 256
HEADER_HEIGHT = 58
COLS = 4


GROUPS: tuple[tuple[str, tuple[dict[str, str], ...]], ...] = (
    (
        "01_主角_战斗与城镇战斗状态",
        (
            {"id": "character_00_hero_combat_idle_candidate", "name": "主角战斗待机", "target": "局内 hero_idle / 城镇 combat_idle", "policy": "局内替换 + 城镇状态"},
            {"id": "character_00_hero_attack_punch_candidate", "name": "主角打拳", "target": "局内 attack_punch / 城镇 punch", "policy": "局内变体 + 城镇状态"},
            {"id": "character_00_hero_attack_kick_candidate", "name": "主角踢腿", "target": "局内 attack_kick / 城镇 kick", "policy": "局内变体 + 城镇状态"},
        ),
    ),
    (
        "02_主角_城镇状态机",
        (
            {"id": "character_00_hero_state_idle_candidate", "name": "主角待机", "target": "hero_town_idle_left", "policy": "已接入城镇 Idle"},
            {"id": "character_00_hero_walk_start_candidate", "name": "主角开始行走", "target": "hero_town_walk_start_left", "policy": "已接入单次起步"},
            {"id": "character_00_hero_walk_loop_candidate", "name": "主角行走中", "target": "hero_town_walk_loop_left", "policy": "已接入循环行走"},
            {"id": "character_00_hero_walk_stop_candidate", "name": "主角急停", "target": "hero_town_walk_stop_left", "policy": "城镇停止动作→新 Idle"},
            {"id": "cinematic_hero_adjust_backpack", "name": "主角整理背包", "target": "hero_town_adjust_backpack", "policy": "城镇随机动作→新 Idle"},
            {"id": "cinematic_hero_deep_breath", "name": "主角深呼吸", "target": "hero_town_deep_breath", "policy": "城镇随机动作→新 Idle"},
            {"id": "cinematic_hero_collect_item_with_ui", "name": "主角采集物品", "target": "hero_town_collect_item", "policy": "城镇交互动作"},
        ),
    ),
    (
        "03_怪物",
        (
            {"id": "enemy_01_rooster_idle_candidate", "name": "公鸡待机", "target": "enemy_01_rooster_idle", "policy": "替换怪物 Idle"},
            {"id": "enemy_01_rooster_attack_candidate", "name": "公鸡攻击", "target": "enemy_01_rooster_attack", "policy": "替换怪物攻击"},
            {"id": "enemy_03_weasel_idle_candidate", "name": "黄鼠狼待机", "target": "enemy_03_weasel_idle", "policy": "替换怪物 Idle"},
            {"id": "enemy_03_weasel_attack_candidate", "name": "黄鼠狼攻击", "target": "enemy_03_weasel_attack", "policy": "替换怪物攻击"},
            {"id": "enemy_05_ironfeather_idle_candidate", "name": "铁公鸡待机", "target": "enemy_05_ironfeather_idle", "policy": "替换怪物 Idle"},
            {"id": "enemy_07_graywolf_idle_candidate", "name": "灰狼待机", "target": "enemy_07_graywolf_idle", "policy": "替换怪物 Idle"},
            {"id": "enemy_11_graymane_attack_candidate", "name": "狼王攻击", "target": "enemy_11_graymane_attack", "policy": "替换首领攻击"},
            {"id": "enemy_16_toad_idle_candidate", "name": "大青蛙待机", "target": "enemy_16_toad_idle", "policy": "替换怪物 Idle"},
            {"id": "enemy_18_deer_idle_candidate", "name": "鹿待机", "target": "enemy_18_deer_idle", "policy": "替换怪物 Idle"},
            {"id": "enemy_18_deer_attack_candidate", "name": "鹿攻击", "target": "enemy_18_deer_attack", "policy": "替换怪物攻击"},
        ),
    ),
    (
        "04_月白",
        (
            {"id": "candidate_yue_fire_intro", "name": "月白出场", "target": "character_09_yue_bai_intro", "policy": "剧情出场"},
            {"id": "candidate_yue_fire_idle", "name": "月白待机", "target": "character_09_yue_bai_idle", "policy": "替换月白 Idle"},
            {"id": "candidate_yue_fire_outro", "name": "月白离场", "target": "character_09_yue_bai_outro", "policy": "剧情离场"},
        ),
    ),
    (
        "05_马与马车_剧情",
        (
            {"id": "cinematic_horse_idle", "name": "马待机", "target": "cinematic_horse_idle", "policy": "序章剧情"},
            {"id": "cinematic_horse_start_run_stop", "name": "马起跑奔跑急停", "target": "cinematic_horse_start_run_stop", "policy": "序章剧情"},
            {"id": "cinematic_carriage_run_stop", "name": "马车奔跑急停", "target": "cinematic_carriage_run_stop", "policy": "序章剧情"},
            {"id": "cinematic_carriage_post_stop_idle", "name": "马车急停后待机", "target": "cinematic_carriage_post_stop_idle", "policy": "序章剧情"},
        ),
    ),
)


def font(size: int) -> ImageFont.ImageFont:
    for candidate in (Path(r"C:\Windows\Fonts\msyh.ttc"), Path(r"C:\Windows\Fonts\simhei.ttf")):
        if candidate.is_file():
            return ImageFont.truetype(str(candidate), size)
    return ImageFont.load_default()


def checker() -> Image.Image:
    image = Image.new("RGBA", (PREVIEW_SIZE, PREVIEW_SIZE), (0, 0, 0, 255))
    draw = ImageDraw.Draw(image)
    colors = ((64, 64, 68, 255), (95, 92, 86, 255))
    cell = 16
    for y in range(0, PREVIEW_SIZE, cell):
        for x in range(0, PREVIEW_SIZE, cell):
            draw.rectangle((x, y, x + cell, y + cell), fill=colors[((x // cell) + (y // cell)) % 2])
    return image


def candidate_data(entry: dict[str, str]) -> dict[str, Any]:
    candidate_root = STAGING_ROOT / entry["id"]
    source_manifest = json.loads((candidate_root / "source_manifest.json").read_text(encoding="utf-8"))
    atlas_manifest = json.loads((candidate_root / "candidate_atlas/manifest.json").read_text(encoding="utf-8"))
    frame_count = int(atlas_manifest["frameCount"])
    representative = candidate_root / "candidate_atlas/frames" / f"frame_{frame_count // 2:04d}.png"
    return {
        **entry,
        "source": source_manifest["sourceRelativePath"],
        "sourceSha256": source_manifest["sourceVideoSha256"],
        "frameCount": frame_count,
        "fps": float(atlas_manifest["fps"]),
        "duration": float(atlas_manifest["durationSeconds"]),
        "atlas1K": atlas_manifest["atlases"]["1K"]["atlas"],
        "atlas2K": atlas_manifest["atlases"]["2K"]["atlas"],
        "edgeAlphaMax": atlas_manifest["edgeAlphaMax"],
        "representative": representative,
    }


def build_board(group_name: str, entries: list[dict[str, Any]]) -> Path:
    rows = math.ceil(len(entries) / COLS)
    board = Image.new("RGBA", (COLS * TILE_WIDTH, HEADER_HEIGHT + rows * TILE_HEIGHT), (22, 22, 26, 255))
    draw = ImageDraw.Draw(board)
    title_font = font(28)
    name_font = font(21)
    detail_font = font(15)
    draw.text((16, 12), group_name.replace("_", " · "), font=title_font, fill=(238, 224, 190, 255))
    for index, entry in enumerate(entries):
        x = (index % COLS) * TILE_WIDTH
        y = HEADER_HEIGHT + (index // COLS) * TILE_HEIGHT
        preview = checker()
        frame = Image.open(entry["representative"]).convert("RGBA").resize((PREVIEW_SIZE, PREVIEW_SIZE), Image.Resampling.NEAREST)
        preview.alpha_composite(frame)
        board.alpha_composite(preview, (x + 32, y))
        draw.rectangle((x + 32, y, x + 32 + PREVIEW_SIZE - 1, y + PREVIEW_SIZE - 1), outline=(128, 116, 93, 255), width=2)
        draw.text((x + 12, y + 262), entry["name"], font=name_font, fill=(244, 218, 121, 255))
        draw.text((x + 12, y + 292), entry["target"], font=detail_font, fill=(163, 207, 185, 255))
        draw.text((x + 12, y + 316), entry["policy"], font=detail_font, fill=(224, 224, 224, 255))
        draw.text(
            (x + 12, y + 338),
            f"60帧 · {entry['duration']:.2f}秒 · {entry['fps']:.2f}fps",
            font=detail_font,
            fill=(168, 168, 174, 255),
        )
    output = REVIEW_ROOT / f"{group_name}.png"
    output.parent.mkdir(parents=True, exist_ok=True)
    board.save(output, format="PNG", optimize=True)
    return output


def main() -> int:
    all_entries: list[dict[str, Any]] = []
    boards: list[str] = []
    for group_name, specs in GROUPS:
        entries = [candidate_data(dict(spec)) for spec in specs]
        all_entries.extend({**entry, "group": group_name} for entry in entries)
        boards.append(str(build_board(group_name, entries).relative_to(PROJECT_ROOT)).replace("\\", "/"))
    if len(all_entries) != 27:
        raise RuntimeError(f"mapping review must contain all 27 approved clips, got {len(all_entries)}")
    report = {
        "schemaVersion": 1,
        "ok": True,
        "sourceRoot": r"C:\Users\shxuw\Downloads\动画优化\新建文件夹\8月27号序列帧",
        "excludedSources": [
            "怪物们/鹿鞠躬.mov",
        ],
        "clipCount": len(all_entries),
        "boards": boards,
        "entries": [
            {key: value for key, value in entry.items() if key != "representative"}
            for entry in all_entries
        ],
    }
    report_path = REVIEW_ROOT / "mapping-review.json"
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"ok": True, "clipCount": len(all_entries), "boards": boards, "report": str(report_path.relative_to(PROJECT_ROOT)).replace("\\", "/")}, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
