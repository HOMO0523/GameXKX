"""Import the reviewed Tencent-town UI atoms into an isolated Unreal tree.

Run this file through the project UE MCP runner while the editor is open.  It
never overwrites the older Tasks, Inventory, or QuestDialog texture trees.
"""

from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = PROJECT_ROOT / "docs" / "ui" / "town" / "source_art"
DESTINATION_ROOT = "/Game/GameXXK/UI/Town/Textures"

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
    ("Companion", "all_selected.png", "T_TownCompanion_AllSelected"),
    ("Companion", "fairy.png", "T_TownCompanion_Fairy"),
    ("Companion", "demon.png", "T_TownCompanion_Demon"),
    ("Companion", "wanderer.png", "T_TownCompanion_Wanderer"),
    ("Companion", "rare_category_source.png", "T_TownCompanion_RareCategory"),
    ("HUD", "hud_profile_full.png", "T_TownHUD_ProfileFull"),
    ("HUD", "hud_profile_portrait.png", "T_TownHUD_ProfilePortrait"),
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
    ("Backpack", "backpack_tab_all.png", "T_TownBackpack_TabAll"),
    ("Backpack", "backpack_tab_equipment.png", "T_TownBackpack_TabEquipment"),
    ("Backpack", "backpack_tab_prop.png", "T_TownBackpack_TabProp"),
    ("Backpack", "backpack_tab_material.png", "T_TownBackpack_TabMaterial"),
    ("Backpack", "backpack_tab_task.png", "T_TownBackpack_TabTask"),
    ("Backpack", "backpack_button_sort.png", "T_TownBackpack_ButtonSort"),
    ("Backpack", "backpack_button_disassemble.png", "T_TownBackpack_ButtonDisassemble"),
)


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
    source = SOURCE_ROOT / group / filename
    if not source.is_file():
        raise RuntimeError(f"missing converted source image: {source}")

    destination = f"{DESTINATION_ROOT}/{group}"
    ensure_directory(destination)
    asset_path = f"{destination}/{asset_name}"
    object_path = f"{asset_path}.{asset_name}"
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = destination
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
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


def main() -> None:
    imported = [import_texture(group, filename, asset_name) for group, filename, asset_name in IMPORTS]
    unreal.EditorAssetLibrary.save_directory(DESTINATION_ROOT, only_if_is_dirty=False, recursive=True)
    print(json.dumps({"ok": True, "imported_count": len(imported), "imported": imported}, ensure_ascii=False))


if __name__ == "__main__":
    main()
