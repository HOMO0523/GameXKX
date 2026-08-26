"""Import and validate the approved 4x4 permanent-talent icon atlas."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "SourceArt/UI/Talents/final/T_TalentIconAtlas_v1.png"
DESTINATION = "/Game/GameXXK/UI/Talents"
NAME = "T_TalentIconAtlas_v1"


def main() -> None:
    if not SOURCE.is_file():
        raise RuntimeError(f"missing permanent-talent icon atlas: {SOURCE}")

    task = unreal.AssetImportTask()
    task.filename = str(SOURCE)
    task.destination_path = DESTINATION
    task.destination_name = NAME
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = f"{DESTINATION}/{NAME}"
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"failed to import permanent-talent atlas: {asset_path}")

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

    width = int(texture.blueprint_get_size_x())
    height = int(texture.blueprint_get_size_y())
    if width != height or width < 1024:
        raise RuntimeError(f"talent atlas must be square and at least 1024px, got {width}x{height}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError(f"failed to save permanent-talent atlas: {asset_path}")

    print(json.dumps({
        "ok": True,
        "asset": texture.get_path_name(),
        "size": [width, height],
        "grid": [4, 4],
    }, ensure_ascii=False))


if __name__ == "__main__":
    main()
