"""Import the reviewed Tencent-town UI atoms into an isolated Unreal tree.

Run this file through the project UE MCP runner while the editor is open.  It
never overwrites the older Tasks, Inventory, or QuestDialog texture trees.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = PROJECT_ROOT / "docs" / "ui" / "town" / "source_art"
DESTINATION_ROOT = "/Game/GameXXK/UI/Town/Textures"

# 036 is the approved original hero detail cut from the reviewed PSD rebuild.  It
# deliberately stays outside the generated town source-art folder so import code
# cannot silently substitute a regenerated or hand-painted replacement.
HERO_DETAIL_036_SOURCE = Path(
    r"C:\Users\shxuw\Downloads\nw-studio-nwueball-https-github-com\nw-studio-nwueball-https-github-com\work\psd_rebuild\clean_assets_v2\036.png"
)
HERO_DETAIL_036_ASSET_NAME = "T_TownCharacter_HeroDetail036"
HERO_DETAIL_036_ASSET_PATH = f"{DESTINATION_ROOT}/Character/{HERO_DETAIL_036_ASSET_NAME}"
HERO_DETAIL_036_EXPECTED_SIZE = (454, 908)
HERO_DETAIL_036_EXPECTED_SHA256 = "a93b2f20e6702e13a997831b2d40679e34d9f81d7861afa0af4aa01455002789"

# These assets are source-locked imports: if an artist has already imported or
# tuned one in the editor, this script must leave it untouched.
MISSING_ONLY_IMPORT_ASSETS = frozenset({HERO_DETAIL_036_ASSET_NAME})

IMPORTS: tuple[tuple[str, str, str], ...] = (
    ("Task", "task_title.png", "T_TownTask_Title"),
    ("Task", "task_tab_main.png", "T_TownTask_TabMain"),
    ("Task", "task_tab_side.png", "T_TownTask_TabSide"),
    ("Task", "task_tab_daily.png", "T_TownTask_TabDaily"),
    ("Task", "task_tab_adventure.png", "T_TownTask_TabAdventure"),
    ("Task", "task_back_arrow.png", "T_TownTask_BackArrow"),
    ("Task", "task_action_track.png", "T_TownTask_ActionTrack"),
    ("Task", "task_action_go.png", "T_TownTask_ActionGo"),
    ("Task", "reward_coin_icon.png", "T_TownTask_RewardCoin"),
    ("Task", "reward_exp_logo.png", "T_TownTask_RewardExp"),
    ("Task", "reward_token_icon.png", "T_TownTask_RewardToken"),
    ("Character", "attribute_selected.png", "T_TownCharacter_AttributeSelected"),
    ("Character", "equipment.png", "T_TownCharacter_Equipment"),
    ("Character", "skills.png", "T_TownCharacter_Skills"),
    ("Character", "talent.png", "T_TownCharacter_Talent"),
    ("Character", "title.png", "T_TownCharacter_Title"),
    ("Character", "detail_attributes.png", "T_TownCharacter_DetailAttributes"),
    ("Character", "hero_detail_036.png", HERO_DETAIL_036_ASSET_NAME),
    ("Companion", "all_selected.png", "T_TownCompanion_AllSelected"),
    ("Companion", "fairy.png", "T_TownCompanion_Fairy"),
    ("Companion", "demon.png", "T_TownCompanion_Demon"),
    ("Companion", "wanderer.png", "T_TownCompanion_Wanderer"),
    ("Companion", "rare_category_source.png", "T_TownCompanion_RareCategory"),
    ("HUD", "hud_profile_full.png", "T_TownHUD_ProfileFull"),
    ("HUD", "hud_profile_portrait.png", "T_TownHUD_ProfilePortrait"),
    ("HUD", "hud_health_bar_frame.png", "T_TownHUD_HealthBarFrame"),
    ("HUD", "hud_health_bar_fill.png", "T_TownHUD_HealthBarFill"),
    ("HUD", "hud_experience_bar_frame.png", "T_TownHUD_ExperienceBarFrame"),
    ("HUD", "hud_experience_bar_fill.png", "T_TownHUD_ExperienceBarFill"),
    ("HUD", "hud_resource_coin_icon.png", "T_TownHUD_ResourceCoin"),
    ("HUD", "hud_resource_green_icon.png", "T_TownHUD_ResourceGreen"),
    ("HUD", "hud_resource_ingot_icon.png", "T_TownHUD_ResourceIngot"),
    ("HUD", "hud_resource_plus_button.png", "T_TownHUD_ResourcePlus"),
    ("Nav", "nav_sidebar_full.png", "T_TownNav_Sidebar"),
    ("Nav", "nav_task.png", "T_TownNav_Task"),
    ("Nav", "nav_inventory.png", "T_TownNav_Inventory"),
    ("Nav", "nav_refine.png", "T_TownNav_Refine"),
    ("Nav", "nav_map.png", "T_TownNav_Map"),
    ("Nav", "nav_friends.png", "T_TownNav_Friends"),
    ("Jianghu", "jianghu_banner.png", "T_TownJianghu_Banner"),
    ("Jianghu", "jianghu_explore_card.png", "T_TownJianghu_Explore"),
    ("Jianghu", "jianghu_adventure_card.png", "T_TownJianghu_Adventure"),
    ("Jianghu", "jianghu_challenge_card.png", "T_TownJianghu_Challenge"),
    ("Jianghu", "jianghu_sword_card.png", "T_TownJianghu_Sword"),
    ("Map", "map_back_arrow.png", "T_TownMap_BackArrow"),
    ("Backpack", "backpack_window_frame.png", "T_TownBackpack_WindowFrame"),
    ("Backpack", "backpack_header.png", "T_TownBackpack_Header"),
    ("Backpack", "backpack_back_arrow.png", "T_TownBackpack_BackArrow"),
    ("Backpack", "backpack_slot.png", "T_TownBackpack_Slot"),
    ("Backpack", "backpack_action_blank.png", "T_TownBackpack_ActionBlank"),
    ("Backpack", "backpack_tab_all.png", "T_TownBackpack_TabAll"),
    ("Backpack", "backpack_tab_equipment.png", "T_TownBackpack_TabEquipment"),
    ("Backpack", "backpack_tab_prop.png", "T_TownBackpack_TabProp"),
    ("Backpack", "backpack_tab_material.png", "T_TownBackpack_TabMaterial"),
    ("Backpack", "backpack_tab_task.png", "T_TownBackpack_TabTask"),
    ("Backpack", "backpack_button_sort.png", "T_TownBackpack_ButtonSort"),
    ("Backpack", "backpack_button_disassemble.png", "T_TownBackpack_ButtonDisassemble"),
)


def resolve_import_source(group: str, filename: str, asset_name: str) -> Path:
    """Return the reviewed source image for one town UI texture import."""
    if asset_name == HERO_DETAIL_036_ASSET_NAME:
        return HERO_DETAIL_036_SOURCE
    return SOURCE_ROOT / group / filename


def _read_png_dimensions(source: Path) -> tuple[int, int]:
    header = source.read_bytes()[:24]
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise RuntimeError(f"expected a PNG with IHDR header: {source}")
    return struct.unpack(">II", header[16:24])


def verify_missing_hero_detail_source() -> dict[str, object]:
    """Validate the one original source that may be imported only when absent."""
    source = resolve_import_source("Character", "hero_detail_036.png", HERO_DETAIL_036_ASSET_NAME)
    if not source.is_file():
        raise RuntimeError(f"missing locked hero detail source: {source}")
    width, height = _read_png_dimensions(source)
    sha256 = hashlib.sha256(source.read_bytes()).hexdigest()
    if (width, height) != HERO_DETAIL_036_EXPECTED_SIZE:
        raise RuntimeError(
            f"hero detail source dimensions changed: {(width, height)} != {HERO_DETAIL_036_EXPECTED_SIZE}"
        )
    if sha256 != HERO_DETAIL_036_EXPECTED_SHA256:
        raise RuntimeError("hero detail source hash changed; refuse to import a replacement")
    return {
        "name": source.name,
        "path": str(source),
        "width": width,
        "height": height,
        "sha256": sha256,
        "asset_path": HERO_DETAIL_036_ASSET_PATH,
    }


def ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def try_set(texture: unreal.Texture2D, name: str, value: object) -> None:
    try:
        texture.set_editor_property(name, value)
    except (AttributeError, RuntimeError):
        pass


def configure_texture(texture: unreal.Texture2D) -> None:
    try_set(texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    try_set(texture, "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    try_set(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    try_set(texture, "filter", unreal.TextureFilter.TF_BILINEAR)
    try_set(texture, "address_x", unreal.TextureAddress.TA_CLAMP)
    try_set(texture, "address_y", unreal.TextureAddress.TA_CLAMP)
    try_set(texture, "srgb", True)
    try_set(texture, "never_stream", True)
    try_set(texture, "compression_no_alpha", False)


def import_texture(group: str, filename: str, asset_name: str) -> str:
    source = resolve_import_source(group, filename, asset_name)
    if not source.is_file():
        raise RuntimeError(f"missing converted source image: {source}")

    if asset_name in MISSING_ONLY_IMPORT_ASSETS:
        verify_missing_hero_detail_source()

    destination = f"{DESTINATION_ROOT}/{group}"
    ensure_directory(destination)
    asset_path = f"{destination}/{asset_name}"
    object_path = f"{asset_path}.{asset_name}"
    if asset_name in MISSING_ONLY_IMPORT_ASSETS and unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        existing_texture = (
            unreal.EditorAssetLibrary.load_asset(object_path)
            or unreal.EditorAssetLibrary.load_asset(asset_path)
        )
        if not isinstance(existing_texture, unreal.Texture2D):
            loaded_class = existing_texture.get_class().get_name() if existing_texture else "None"
            raise RuntimeError(f"locked hero detail asset is not a Texture2D: {asset_path}; class={loaded_class}")
        return existing_texture.get_path_name()

    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = destination
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = asset_name not in MISSING_ONLY_IMPORT_ASSETS
    task.replace_existing_settings = asset_name not in MISSING_ONLY_IMPORT_ASSETS
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = (
        unreal.EditorAssetLibrary.load_asset(object_path)
        or unreal.EditorAssetLibrary.load_asset(asset_path)
    )
    if not isinstance(texture, unreal.Texture2D):
        loaded_class = texture.get_class().get_name() if texture else "None"
        raise RuntimeError(f"failed to import Texture2D: {asset_path}; class={loaded_class}")
    configure_texture(texture)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture.get_path_name()


def select_imports(
    groups: list[str] | None,
    asset_names: list[str] | None,
) -> tuple[tuple[str, str, str], ...]:
    if groups and asset_names:
        raise RuntimeError("use either --groups or --assets, not both")
    if asset_names:
        requested_assets = set(asset_names)
        available_assets = {asset_name for _, _, asset_name in IMPORTS}
        unknown_assets = requested_assets.difference(available_assets)
        if unknown_assets:
            raise RuntimeError(f"unknown UI source assets: {sorted(unknown_assets)}")
        return tuple(entry for entry in IMPORTS if entry[2] in requested_assets)
    if groups:
        requested_groups = set(groups)
        available_groups = {group for group, _, _ in IMPORTS}
        unknown_groups = requested_groups.difference(available_groups)
        if unknown_groups:
            raise RuntimeError(f"unknown UI source groups: {sorted(unknown_groups)}")
        return tuple(entry for entry in IMPORTS if entry[0] in requested_groups)
    return IMPORTS


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description="Import reviewed town UI atoms.")
    parser.add_argument(
        "--groups",
        nargs="+",
        default=None,
        help="Optional source-art groups to import. Defaults to all groups for backward compatibility.",
    )
    parser.add_argument(
        "--assets",
        nargs="+",
        default=None,
        help="Optional exact asset names to import. Mutually exclusive with --groups.",
    )
    args = parser.parse_args(argv)
    selected_imports = select_imports(args.groups, args.assets)
    imported = [import_texture(group, filename, asset_name) for group, filename, asset_name in selected_imports]
    imported_groups = sorted({group for group, _, _ in selected_imports})
    print(json.dumps({
        "ok": True,
        "groups": imported_groups,
        "imported_count": len(imported),
        "imported": imported,
    }, ensure_ascii=False))


if __name__ == "__main__":
    main()
