from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = PROJECT_ROOT / "Content" / "GameXXK" / "SourceArt" / "UI" / "Inventory"
DESTINATION = "/Game/GameXXK/UI/Inventory/Textures"

IMPORTS = [
    (SOURCE_DIR / "ink_backpack_slot.png", "T_InkBackpackSlot"),
    (SOURCE_DIR / "item_healing_powder.png", "T_ItemHealingPowder"),
    (SOURCE_DIR / "item_lingzhi_powder.png", "T_ItemLingzhiPowder"),
    (SOURCE_DIR / "item_qingxin_tea.png", "T_ItemQingxinTea"),
    (SOURCE_DIR / "item_crane_sachet.png", "T_ItemCraneSachet"),
    (SOURCE_DIR / "item_crane_pattern_talisman.png", "T_ItemCranePatternTalisman"),
    (SOURCE_DIR / "item_inkstone_pendant.png", "T_ItemInkstonePendant"),
    (SOURCE_DIR / "item_qingfeng_short_sword.png", "T_ItemQingfengShortSword"),
    (SOURCE_DIR / "item_bamboo_light_armor.png", "T_ItemBambooLightArmor"),
]


def _ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def _configure_texture(texture) -> None:
    if not texture:
        return
    enum_props = {
        "mip_gen_settings": ("TextureMipGenSettings", "TMGS_NO_MIPMAPS"),
        "compression_settings": ("TextureCompressionSettings", "TC_EDITOR_ICON"),
    }
    for prop_name, enum_info in enum_props.items():
        enum_type = getattr(unreal, enum_info[0], None)
        if enum_type is not None and hasattr(enum_type, enum_info[1]):
            texture.set_editor_property(prop_name, getattr(enum_type, enum_info[1]))
    for prop_name, value in (("srgb", True), ("never_stream", True)):
        try:
            texture.set_editor_property(prop_name, value)
        except Exception:
            pass


def import_texture(source: Path, asset_name: str) -> str:
    if not source.exists():
        raise RuntimeError(f"missing source image: {source}")
    _ensure_directory(DESTINATION)
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = DESTINATION
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = f"{DESTINATION}/{asset_name}"
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"failed to import Texture2D: {asset_path}")
    _configure_texture(texture)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture.get_path_name()


def main() -> None:
    imported = [import_texture(source, name) for source, name in IMPORTS]
    unreal.EditorAssetLibrary.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True)
    print(json.dumps({"ok": True, "imported": imported, "imported_count": len(imported)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
