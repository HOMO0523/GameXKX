from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = PROJECT_ROOT / "docs" / "reference-assets"
DESTINATION = "/Game/GameXXK/UI/QuestDialog/Textures"

IMPORTS = [
    (SOURCE_DIR / "quest_dialog_frame.png", "T_QuestDialogFrame"),
    (SOURCE_DIR / "quest_dialog_accept.png", "T_QuestDialogAccept"),
    (SOURCE_DIR / "quest_dialog_leave.png", "T_QuestDialogLeave"),
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
    asset_path = f"{DESTINATION}/{asset_name}"
    asset_object_path = f"{asset_path}.{asset_name}"
    texture = None
    if unreal.EditorAssetLibrary.does_asset_exist(asset_object_path) or unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        texture = unreal.EditorAssetLibrary.load_asset(asset_object_path) or unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(texture, unreal.Texture2D):
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = DESTINATION
        task.destination_name = asset_name
        task.automated = True
        task.replace_existing = True
        task.save = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.EditorAssetLibrary.load_asset(asset_object_path) or unreal.EditorAssetLibrary.load_asset(asset_path)

    if not isinstance(texture, unreal.Texture2D):
        texture_class = texture.get_class().get_name() if texture else "None"
        raise RuntimeError(f"failed to import Texture2D: {asset_path}; loaded_class={texture_class}")

    _configure_texture(texture)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture.get_path_name()


def main() -> None:
    imported = [import_texture(source, name) for source, name in IMPORTS]
    unreal.EditorAssetLibrary.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True)
    print(json.dumps({"ok": True, "imported": imported, "imported_count": len(imported)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
