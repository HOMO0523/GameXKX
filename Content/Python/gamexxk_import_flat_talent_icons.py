"""Import the approved flat permanent-talent icon kit."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = ROOT / "SourceArt/UI/Talents/flat"
DESTINATION = "/Game/GameXXK/UI/Talents/Flat"
ICON_NAMES = (
    "Attack", "Health", "Defense", "Critical", "Movement", "Backpack",
    "Gold", "Experience", "Offline", "Time", "Chest",
)


def configure(texture: unreal.Texture2D) -> None:
    settings = {
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
        "filter": unreal.TextureFilter.TF_BILINEAR,
        "address_x": unreal.TextureAddress.TA_CLAMP,
        "address_y": unreal.TextureAddress.TA_CLAMP,
        "srgb": True,
        "never_stream": True,
        "compression_no_alpha": False,
    }
    for name, value in settings.items():
        texture.set_editor_property(name, value)


def import_texture(source: Path, destination_name: str) -> unreal.Texture2D:
    if not source.is_file():
        raise RuntimeError(f"missing flat talent source: {source}")
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = DESTINATION
    task.destination_name = destination_name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(f"{DESTINATION}/{destination_name}")
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"failed to import flat talent texture: {destination_name}")
    configure(texture)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError(f"failed to save flat talent texture: {destination_name}")
    return texture


def main() -> None:
    imported: list[dict[str, object]] = []
    for icon_name in ICON_NAMES:
        asset_name = f"T_TalentFlat_{icon_name}"
        texture = import_texture(SOURCE_DIR / f"{asset_name}.png", asset_name)
        size = [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())]
        if size != [512, 512]:
            raise RuntimeError(f"wrong flat talent size for {asset_name}: {size}")
        imported.append({"asset": texture.get_path_name(), "size": size})

    print(json.dumps({
        "ok": True,
        "destination": DESTINATION,
        "count": len(imported),
        "imported": imported,
    }, ensure_ascii=False))


if __name__ == "__main__":
    main()
