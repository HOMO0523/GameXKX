"""Import the approved generated paper/ink scrollbar for PartyDeck lists.

The approved PSD reconstruction does not contain a scroll track or thumb layer.
These two non-text elements therefore follow the locked two-phase process:
image generation on a chroma key, then local alpha extraction/cropping.  The
source manifest pins every source hash, crop result and destination.  Legacy
PSD cuts 019/051 must never be imported or referenced as scrollbars.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path
from typing import Any

try:
    import unreal
except ImportError:
    unreal = None


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "ui-scrollbar"
MANIFEST_PATH = SOURCE_ROOT / "scrollbar_generated_manifest_v1.json"
DESTINATION_ROOT = "/Game/GameXXK/UI/PartyDeck/Scrollbars"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _png_size(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != PNG_SIGNATURE or header[12:16] != b"IHDR":
        raise ValueError(f"not a readable PNG: {path}")
    return struct.unpack(">II", header[16:24])


def _load_manifest() -> dict[str, Any]:
    if not MANIFEST_PATH.is_file():
        raise FileNotFoundError(f"scrollbar source manifest is missing: {MANIFEST_PATH}")
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    if manifest.get("schemaVersion") != 1:
        raise ValueError("unexpected generated scrollbar manifest schema")
    if "019" in json.dumps(manifest, ensure_ascii=False) or "051" in json.dumps(manifest, ensure_ascii=False):
        raise ValueError("legacy non-scrollbar PSD cuts are forbidden")
    return manifest


def _validate_source(path: Path, expected_hash: str, expected_pixels: list[int]) -> None:
    if not path.is_file():
        raise FileNotFoundError(f"generated scrollbar source is missing: {path}")
    if list(_png_size(path)) != list(expected_pixels):
        raise ValueError(f"generated scrollbar dimensions changed: {path}")
    if _sha256(path).lower() != expected_hash.lower():
        raise ValueError(f"generated scrollbar hash changed: {path}")


def validate_scrollbar_plan() -> dict[str, Any]:
    manifest = _load_manifest()
    phase1 = manifest["phase1"]
    phase2 = manifest["phase2"]
    _validate_source(SOURCE_ROOT / phase1["source"], phase1["sha256"], phase1["pixels"])
    _validate_source(SOURCE_ROOT / phase2["alphaSource"], phase2["sha256"], phase2["pixels"])

    records: list[dict[str, Any]] = []
    for component in phase2["components"]:
        source = SOURCE_ROOT / component["derivedSource"]
        _validate_source(source, component["sha256"], component["pixels"])
        record = dict(component)
        record["source"] = source
        record["asset_path"] = f"{DESTINATION_ROOT}/{component['assetName']}"
        records.append(record)
    return {"ok": True, "destination_root": DESTINATION_ROOT, "asset_count": len(records), "assets": records}


def _configure(texture: object) -> None:
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("never_stream", True)


def _validate_imported(texture: object, record: dict[str, Any]) -> None:
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"scrollbar asset is not Texture2D: {record['asset_path']}")
    if [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())] != record["pixels"]:
        raise RuntimeError(f"scrollbar texture has unexpected size: {record['asset_path']}")
    import_data = texture.get_editor_property("asset_import_data")
    imported = Path(str(import_data.get_first_filename())) if import_data else None
    if imported is None or imported.resolve() != Path(record["source"]).resolve():
        raise RuntimeError(f"scrollbar texture source mismatch: {record['asset_path']}")


def import_verified_scrollbar_assets() -> dict[str, Any]:
    if unreal is None:
        raise RuntimeError("UE Python is required to import scrollbar textures")
    plan = validate_scrollbar_plan()
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION_ROOT):
        if not unreal.EditorAssetLibrary.make_directory(DESTINATION_ROOT):
            raise RuntimeError(f"could not create scrollbar asset directory: {DESTINATION_ROOT}")
    imported: list[str] = []
    validated_existing: list[str] = []
    for record in plan["assets"]:
        asset_path = record["asset_path"]
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            _validate_imported(unreal.EditorAssetLibrary.load_asset(asset_path), record)
            validated_existing.append(asset_path)
            continue
        task = unreal.AssetImportTask()
        task.filename = str(record["source"])
        task.destination_path = DESTINATION_ROOT
        task.destination_name = record["assetName"]
        task.automated = True
        task.save = False
        task.replace_existing = False
        task.replace_existing_settings = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.EditorAssetLibrary.load_asset(asset_path)
        _configure(texture)
        if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
            raise RuntimeError(f"failed to save scrollbar texture: {asset_path}")
        _validate_imported(texture, record)
        imported.append(asset_path)
    return {**plan, "imported_count": len(imported), "validated_existing_count": len(validated_existing)}


def _jsonable(value: Any) -> Any:
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, dict):
        return {key: _jsonable(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_jsonable(item) for item in value]
    return value


def main(argv: list[str] | None = None) -> dict[str, Any]:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute-import", action="store_true")
    args = parser.parse_args(argv)
    result = import_verified_scrollbar_assets() if args.execute_import else validate_scrollbar_plan()
    print(json.dumps(_jsonable(result), ensure_ascii=False, indent=2))
    return result


if __name__ == "__main__":
    main()
