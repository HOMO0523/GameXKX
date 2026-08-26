from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "Training" / "IdleStrip" / "final"
MANIFEST_PATH = SOURCE_ROOT / "training_wave_marker_manifest.json"
DESTINATION = "/Game/GameXXK/UI/Training/IdleStrip"
REPORT_PATH = PROJECT_ROOT / "Saved" / "GameXXK" / "IdleStrip" / "wave_marker_import.json"


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


def main() -> None:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    if manifest.get("status") != "PASS":
        raise RuntimeError("wave marker source manifest is not PASS")
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        unreal.EditorAssetLibrary.make_directory(DESTINATION)

    imported = []
    for record in manifest["assets"]:
        asset_name = str(record["asset"])
        source = SOURCE_ROOT / str(record["file"])
        expected_hash = str(record["sha256"])
        if _sha256(source) != expected_hash:
            raise RuntimeError(f"hash mismatch for {asset_name}")

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
        imported.append({"asset": asset_name, "path": texture.get_path_name(), "sha256": expected_hash})

    unreal.EditorAssetLibrary.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True)
    report = {"status": "PASS", "destination": DESTINATION, "assets": imported}
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
