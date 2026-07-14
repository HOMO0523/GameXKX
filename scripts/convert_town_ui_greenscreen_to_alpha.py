"""Convert the reviewed Tencent-town UI atom sheets from chroma key to alpha.

The green-screen originals under ``Saved/Codex`` are review sources and are
never changed.  This script writes the canonical project-bound PNG sources
under ``docs/ui/town/source_art`` for Unreal import.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CANDIDATE_ROOT = PROJECT_ROOT / "Saved" / "Codex" / "ui_greenscreen_candidates" / "atoms"
OUTPUT_ROOT = PROJECT_ROOT / "docs" / "ui" / "town" / "source_art"
CHROMA_KEY_HELPER = (
    Path.home()
    / ".codex"
    / "skills"
    / ".system"
    / "imagegen"
    / "scripts"
    / "remove_chroma_key.py"
)

# Only approved high-resolution atom sources belong here. Contact sheets and
# threshold/preview probes intentionally stay outside this manifest.
ASSETS: tuple[tuple[str, str], ...] = (
    ("task/task_title.png", "Task/task_title.png"),
    ("task/task_tab_main.png", "Task/task_tab_main.png"),
    ("task/task_tab_side.png", "Task/task_tab_side.png"),
    ("task/task_tab_daily.png", "Task/task_tab_daily.png"),
    ("task/task_tab_adventure.png", "Task/task_tab_adventure.png"),
    ("task/task_back_arrow.png", "Task/task_back_arrow.png"),
    ("task/task_action_track.png", "Task/task_action_track.png"),
    ("task/task_action_go.png", "Task/task_action_go.png"),
    ("task/reward_coin_icon.png", "Task/reward_coin_icon.png"),
    ("task/reward_exp_logo.png", "Task/reward_exp_logo.png"),
    ("task/reward_token_icon.png", "Task/reward_token_icon.png"),
    ("character/attribute_selected.png", "Character/attribute_selected.png"),
    ("character/equipment.png", "Character/equipment.png"),
    ("character/skills.png", "Character/skills.png"),
    ("character/talent.png", "Character/talent.png"),
    ("character/title.png", "Character/title.png"),
    ("character/detail_attributes.png", "Character/detail_attributes.png"),
    ("companion/all_selected.png", "Companion/all_selected.png"),
    ("companion/fairy.png", "Companion/fairy.png"),
    ("companion/demon.png", "Companion/demon.png"),
    ("companion/wanderer.png", "Companion/wanderer.png"),
    ("companion/rare_category_source.png", "Companion/rare_category_source.png"),
    ("hud_nav/hud_profile_full.png", "HUD/hud_profile_full.png"),
    ("hud_nav/hud_resource_coin_icon.png", "HUD/hud_resource_coin_icon.png"),
    ("hud_nav/hud_resource_green_icon.png", "HUD/hud_resource_green_icon.png"),
    ("hud_nav/hud_resource_ingot_icon.png", "HUD/hud_resource_ingot_icon.png"),
    ("hud_nav/hud_resource_plus_button.png", "HUD/hud_resource_plus_button.png"),
    ("hud_nav/nav_sidebar_full.png", "Nav/nav_sidebar_full.png"),
    ("hud_nav/nav_task.png", "Nav/nav_task.png"),
    ("hud_nav/nav_inventory.png", "Nav/nav_inventory.png"),
    ("hud_nav/nav_refine.png", "Nav/nav_refine.png"),
    ("hud_nav/nav_map.png", "Nav/nav_map.png"),
    ("hud_nav/nav_friends.png", "Nav/nav_friends.png"),
    ("hud_nav/jianghu_banner.png", "Jianghu/jianghu_banner.png"),
    ("hud_nav/jianghu_explore_card.png", "Jianghu/jianghu_explore_card.png"),
    ("hud_nav/jianghu_adventure_card.png", "Jianghu/jianghu_adventure_card.png"),
    ("hud_nav/jianghu_challenge_card.png", "Jianghu/jianghu_challenge_card.png"),
    ("hud_nav/jianghu_sword_card.png", "Jianghu/jianghu_sword_card.png"),
    ("map_backpack/map_back_arrow.png", "Map/map_back_arrow.png"),
    ("map_backpack/backpack_tab_all.png", "Backpack/backpack_tab_all.png"),
    ("map_backpack/backpack_tab_equipment.png", "Backpack/backpack_tab_equipment.png"),
    ("map_backpack/backpack_tab_prop.png", "Backpack/backpack_tab_prop.png"),
    ("map_backpack/backpack_tab_material.png", "Backpack/backpack_tab_material.png"),
    ("map_backpack/backpack_tab_task.png", "Backpack/backpack_tab_task.png"),
    ("map_backpack/backpack_button_sort.png", "Backpack/backpack_button_sort.png"),
    ("map_backpack/backpack_button_disassemble.png", "Backpack/backpack_button_disassemble.png"),
)

# Source panels sometimes include static example numbers.  The HUD uses this
# portrait-only crop while live UMG text renders level, XP, power and currency.
CROPS: tuple[tuple[str, str, tuple[int, int, int, int]], ...] = (
    ("HUD/hud_profile_full.png", "HUD/hud_profile_portrait.png", (44, 52, 500, 548)),
)


def validate_alpha(path: Path, require_transparent_corners: bool = True) -> None:
    """Validate the deterministic result without changing the artwork."""
    with Image.open(path) as image:
        image = image.convert("RGBA")
        width, height = image.size
        if width < 2 or height < 2:
            raise RuntimeError(f"invalid UI atom dimensions: {path} ({width}x{height})")
        alpha = image.getchannel("A")
        corners = (
            alpha.getpixel((0, 0)),
            alpha.getpixel((width - 1, 0)),
            alpha.getpixel((0, height - 1)),
            alpha.getpixel((width - 1, height - 1)),
        )
        if require_transparent_corners and any(value != 0 for value in corners):
            raise RuntimeError(f"green-screen corners were not transparent: {path}")
        alpha_min, alpha_max = alpha.getextrema()
        if alpha_min != 0 or alpha_max == 0:
            raise RuntimeError(f"alpha matte is not usable: {path}")


def convert_one(source_relative: str, output_relative: str, check_only: bool) -> Path:
    source = CANDIDATE_ROOT / source_relative
    output = OUTPUT_ROOT / output_relative
    if not source.is_file():
        raise FileNotFoundError(f"missing reviewed green-screen source: {source}")
    if check_only:
        validate_alpha(output)
        return output

    output.parent.mkdir(parents=True, exist_ok=True)
    command = [
        sys.executable,
        str(CHROMA_KEY_HELPER),
        "--input",
        str(source),
        "--out",
        str(output),
        "--key-color",
        "#00ff00",
        "--soft-matte",
        "--transparent-threshold",
        "10",
        "--opaque-threshold",
        "80",
        "--despill",
        "--force",
    ]
    subprocess.run(command, check=True)
    validate_alpha(output)
    return output


def crop_one(source_relative: str, output_relative: str, crop_box: tuple[int, int, int, int], check_only: bool) -> Path:
    source = OUTPUT_ROOT / source_relative
    output = OUTPUT_ROOT / output_relative
    if not source.is_file():
        raise FileNotFoundError(f"missing alpha source for crop: {source}")
    if check_only:
        validate_alpha(output, require_transparent_corners=False)
        return output

    output.parent.mkdir(parents=True, exist_ok=True)
    with Image.open(source) as image:
        image.convert("RGBA").crop(crop_box).save(output)
    validate_alpha(output, require_transparent_corners=False)
    return output


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--check-only",
        action="store_true",
        help="Only validate the already converted alpha PNGs.",
    )
    args = parser.parse_args()

    if not CHROMA_KEY_HELPER.is_file():
        raise FileNotFoundError(f"missing chroma-key helper: {CHROMA_KEY_HELPER}")

    outputs = [convert_one(source, output, args.check_only) for source, output in ASSETS]
    outputs.extend(crop_one(source, output, crop_box, args.check_only) for source, output, crop_box in CROPS)
    print(f"{{\"ok\": true, \"converted_count\": {len(outputs)}}}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
