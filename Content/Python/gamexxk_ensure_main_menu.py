from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
REPORT_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-main-menu-ensure.json"

SOURCE_DIR = PROJECT_ROOT / "docs" / "ui" / "main_menu" / "source_art"
TEXTURE_DIR = "/Game/GameXXK/UI/MainMenu/Textures"
MAIN_MAP = "/Game/GameXXK/Maps/L_Main"

TEXTURES = [
    {
        "source": SOURCE_DIR / "main_menu_cover_tiger_duel.png",
        "asset": "T_MainMenuCover",
    },
    {
        "source": SOURCE_DIR / "ink_button_base.png",
        "asset": "T_InkButtonBase",
    },
]


def _ensure_directory(path: str) -> bool:
    if unreal.EditorAssetLibrary.does_directory_exist(path):
        return False
    return bool(unreal.EditorAssetLibrary.make_directory(path))


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


def _import_texture(source_file: Path, asset_name: str) -> str:
    if not source_file.exists():
        raise RuntimeError(f"Missing main menu source texture: {source_file}")
    _ensure_directory(TEXTURE_DIR)
    task = unreal.AssetImportTask()
    task.filename = str(source_file)
    task.destination_path = TEXTURE_DIR
    task.destination_name = asset_name
    task.automated = True
    task.save = False
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = f"{TEXTURE_DIR}/{asset_name}"
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not texture:
        raise RuntimeError(f"Could not load imported main menu texture: {asset_path}")
    _configure_texture(texture)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture.get_path_name()


def _load_main_map() -> None:
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAIN_MAP):
        raise RuntimeError(f"Could not load main menu map: {MAIN_MAP}")


def _get_editor_world():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if subsystem and hasattr(subsystem, "get_editor_world"):
        return subsystem.get_editor_world()
    if hasattr(unreal, "EditorLevelLibrary"):
        return unreal.EditorLevelLibrary.get_editor_world()
    return None


def _set_main_map_game_mode() -> str:
    _load_main_map()
    world = _get_editor_world()
    if not world:
        raise RuntimeError("Could not resolve editor world for L_Main")
    world_settings = world.get_world_settings()
    if not world_settings:
        raise RuntimeError("L_Main has no world settings")
    world_settings.set_editor_property("default_game_mode", unreal.GameXXKFlowMapGameMode.static_class())
    unreal.EditorLoadingAndSavingUtils.save_map(world, MAIN_MAP)
    return world_settings.get_editor_property("default_game_mode").get_path_name()


def main() -> dict:
    unreal.EditorAssetLibrary.make_directory("/Game/GameXXK/UI")
    unreal.EditorAssetLibrary.make_directory("/Game/GameXXK/UI/MainMenu")
    imported = {}
    for spec in TEXTURES:
        imported[spec["asset"]] = _import_texture(spec["source"], spec["asset"])

    default_game_mode = _set_main_map_game_mode()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    report = {
        "status": "ok",
        "textures": imported,
        "main_map": MAIN_MAP,
        "main_map_default_game_mode": default_game_mode,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    return report


if __name__ == "__main__":
    main()
