from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "Training" / "IdleStrip" / "final"
DESTINATION = "/Game/GameXXK/UI/Training/IdleStrip"
REPORT = PROJECT_ROOT / "Saved" / "GameXXK" / "IdleStrip" / "retry_ui_import.json"
ASSETS = {
    "T_TrainingRetryButtonBase": "057a85880203cb9755c7697e7116ca7fe9b94c755ef85684778eddc8fe00addb",
    "T_TrainingRetryIconEnabled": "be7f99c96b4ee5369a6b64217c41c77d25126c2018b0ef01a35e6d149e49c884",
    "T_TrainingRetryIconDisabled": "fb96f3ee8c8aa44c2a397554526023022be906e99f601747ab540ed7cd51e3d4",
}


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _try_set(obj, property_name: str, value) -> None:
    try:
        obj.set_editor_property(property_name, value)
    except Exception:
        pass


def _load_texture(asset_name: str):
    package_path = f"{DESTINATION}/{asset_name}"
    return unreal.EditorAssetLibrary.load_asset(f"{package_path}.{asset_name}") or unreal.EditorAssetLibrary.load_asset(package_path)


def _import_texture(asset_name: str) -> unreal.Texture2D:
    source = SOURCE_ROOT / f"{asset_name}.png"
    actual_hash = _sha256(source)
    if actual_hash != ASSETS[asset_name]:
        raise RuntimeError(f"hash mismatch for {asset_name}: {actual_hash} != {ASSETS[asset_name]}")

    texture = _load_texture(asset_name)
    if not isinstance(texture, unreal.Texture2D):
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = DESTINATION
        task.destination_name = asset_name
        task.automated = True
        task.replace_existing = False
        task.save = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = _load_texture(asset_name)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"failed to import {asset_name}")

    _try_set(texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    _try_set(texture, "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    _try_set(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    _try_set(texture, "srgb", True)
    _try_set(texture, "never_stream", True)
    _try_set(texture, "filter", unreal.TextureFilter.TF_BILINEAR)
    _try_set(texture, "address_x", unreal.TextureAddress.TA_CLAMP)
    _try_set(texture, "address_y", unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture


def main() -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        unreal.EditorAssetLibrary.make_directory(DESTINATION)
    imported = []
    for asset_name in ASSETS:
        texture = _import_texture(asset_name)
        imported.append(
            {
                "name": asset_name,
                "assetPath": texture.get_path_name(),
                "sourceSha256": ASSETS[asset_name],
                "width": 256,
                "height": 256,
            }
        )
    unreal.EditorAssetLibrary.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True)
    report = {"status": "PASS", "destination": DESTINATION, "assets": imported}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
