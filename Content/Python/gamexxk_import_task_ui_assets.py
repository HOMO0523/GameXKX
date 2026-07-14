from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = PROJECT_ROOT / "docs" / "ui" / "tasks" / "source_art"
DESTINATION = "/Game/GameXXK/UI/Tasks/Textures"

IMPORTS = [
    ("task_panel_frame.png", "T_TaskPanelFrame"),
    ("task_panel_back_arrow.png", "T_TaskPanelBackArrow"),
    ("task_panel_title.png", "T_TaskPanelTitle"),
    ("task_tab_selected.png", "T_TaskTabSelected"),
    ("task_tab_normal.png", "T_TaskTabNormal"),
    ("task_tab_label_main.png", "T_TaskTabLabelMain"),
    ("task_tab_label_side.png", "T_TaskTabLabelSide"),
    ("task_entry_frame.png", "T_TaskEntryFrame"),
    ("task_action_track.png", "T_TaskActionTrack"),
    ("task_action_go.png", "T_TaskActionGo"),
    ("reward_coin.png", "T_RewardCoin"),
    ("reward_exp.png", "T_RewardExp"),
    ("reward_token.png", "T_RewardToken"),
]


def _ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def _configure_texture(texture: unreal.Texture2D) -> None:
    texture.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    )
    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON
    )
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)


def import_texture(filename: str, asset_name: str) -> str:
    source = SOURCE_DIR / filename
    if not source.exists():
        raise RuntimeError(f"missing source image: {source}")

    _ensure_directory(DESTINATION)
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    asset_registry.scan_paths_synchronous([DESTINATION], True)
    asset_path = f"{DESTINATION}/{asset_name}"
    object_path = f"{asset_path}.{asset_name}"
    # Always import with replacement so the commandlet updates existing task
    # textures after their source PNG has been revised.
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = DESTINATION
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
        raise RuntimeError(
            f"failed to import Texture2D: {asset_path}; loaded_class={loaded_class}"
        )

    _configure_texture(texture)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture.get_path_name()


def main() -> None:
    imported = [import_texture(filename, asset_name) for filename, asset_name in IMPORTS]
    unreal.EditorAssetLibrary.save_directory(
        DESTINATION, only_if_is_dirty=False, recursive=True
    )
    print(
        json.dumps(
            {"ok": True, "imported": imported, "imported_count": len(imported)},
            ensure_ascii=False,
        )
    )


if __name__ == "__main__":
    main()
