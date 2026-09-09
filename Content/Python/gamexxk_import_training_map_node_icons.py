"""Import and validate the approved high-fill Training map node status icons."""

from __future__ import annotations

def save_asset_with_texture_budget(*args, **kwargs):
    from gamexxk_texture_budget import save_loaded_asset
    return save_loaded_asset(*args, unreal_module=unreal, **kwargs)


import json
from pathlib import Path

import unreal


ROOT = Path(__file__).resolve().parents[2]
DESTINATION = "/Game/GameXXK/UI/Training/MapNodes"
SOURCES = {
    "T_TrainingNode_Passed": ROOT / "SourceArt/UI/RouteMap/final/T_TrainingNode_Passed.png",
    "T_TrainingNode_Challenge": ROOT / "SourceArt/UI/RouteMap/final/T_TrainingNode_Challenge.png",
    "T_TrainingNode_Locked": ROOT / "SourceArt/UI/RouteMap/final/T_TrainingNode_Locked.png",
}


def configure(texture: unreal.Texture2D) -> None:
    settings = {
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        "compression_settings": unreal.TextureCompressionSettings.TC_BC7,
        "power_of_two_mode": unreal.TexturePowerOfTwoSetting.RESIZE_TO_SPECIFIC_RESOLUTION,
        "resize_during_build_x": 512,
        "resize_during_build_y": 512,
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


def main() -> None:
    imported: list[str] = []
    for name, source in SOURCES.items():
        if not source.is_file():
            raise RuntimeError(f"missing Training node icon source: {source}")
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = DESTINATION
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.replace_existing_settings = True
        task.save = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        asset = unreal.EditorAssetLibrary.load_asset(f"{DESTINATION}/{name}")
        if not isinstance(asset, unreal.Texture2D):
            raise RuntimeError(f"failed to import Training node texture {name}")
        configure(asset)
        if not save_asset_with_texture_budget(asset):
            raise RuntimeError(f"failed to save Training node texture {name}")
        if int(asset.blueprint_get_size_x()) != 512 or int(asset.blueprint_get_size_y()) != 512:
            raise RuntimeError(f"wrong Training node texture size {name}")
        imported.append(asset.get_path_name())
    print(json.dumps({"ok": True, "imported_count": len(imported), "imported": imported}, ensure_ascii=False))


if __name__ == "__main__":
    main()
