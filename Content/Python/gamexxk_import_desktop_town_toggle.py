from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "DesktopOverlay"
DESTINATION = "/Game/GameXXK/UI/DesktopOverlay"
REPORT = PROJECT_ROOT / "Saved" / "GameXXK" / "DesktopOverlay" / "town_toggle_import.json"
ASSETS = {
    "T_DesktopTownEnterButton": "cb3c40ddf3b3ea5e2bddc1b30726ec4258cce3f0dc9ff735bf2b6be075123042",
    "T_DesktopTownExitButton": "e291a0e07953b7a66375ad99bd20e66f93f7664c547f036c288990669027244c",
    "T_DesktopStoryQuestButton": "182a5003cf948f7aa5957e63354381d1ad001e11cc17b41a25c9dd852a8c89f9",
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
    return unreal.EditorAssetLibrary.load_asset(
        f"{package_path}.{asset_name}"
    ) or unreal.EditorAssetLibrary.load_asset(package_path)


def _import_texture(asset_name: str, expected_hash: str) -> unreal.Texture2D:
    source = SOURCE_ROOT / f"{asset_name}.png"
    actual_hash = _sha256(source)
    if actual_hash != expected_hash:
        raise RuntimeError(
            f"hash mismatch for {asset_name}: {actual_hash} != {expected_hash}"
        )

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
    for asset_name, expected_hash in ASSETS.items():
        texture = _import_texture(asset_name, expected_hash)
        imported.append(
            {
                "name": asset_name,
                "assetPath": texture.get_path_name(),
                "sourceSha256": expected_hash,
                "width": 512,
                "height": 512,
            }
        )
    unreal.EditorAssetLibrary.save_directory(
        DESTINATION,
        only_if_is_dirty=False,
        recursive=True,
    )
    report = {"status": "PASS", "destination": DESTINATION, "assets": imported}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
