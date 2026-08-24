"""Import the approved 30 gem item icons without touching other assets."""

from __future__ import annotations

import json
from pathlib import Path
import struct

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
MANIFEST = PROJECT_ROOT / "SourceArt" / "UI" / "Items" / "Gems" / "gem_icon_manifest.json"
DESTINATION = "/Game/GameXXK/UI/Items/Gems"
EXPECTED_SIZE = (512, 512)


def png_dimensions(source: Path) -> tuple[int, int]:
    header = source.read_bytes()[:24]
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise RuntimeError(f"expected PNG source: {source}")
    return struct.unpack(">II", header[16:24])


def set_required(texture: unreal.Texture2D, name: str, value: object) -> None:
    texture.set_editor_property(name, value)
    if texture.get_editor_property(name) != value:
        raise RuntimeError(f"gem texture setting did not persist: {name}")


def configure(texture: unreal.Texture2D) -> None:
    set_required(texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    set_required(texture, "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    set_required(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    set_required(texture, "filter", unreal.TextureFilter.TF_BILINEAR)
    set_required(texture, "address_x", unreal.TextureAddress.TA_CLAMP)
    set_required(texture, "address_y", unreal.TextureAddress.TA_CLAMP)
    set_required(texture, "srgb", True)
    set_required(texture, "never_stream", True)
    set_required(texture, "compression_no_alpha", False)


def load_records() -> list[dict[str, object]]:
    payload = json.loads(MANIFEST.read_text(encoding="utf-8"))
    records = payload.get("records", [])
    if payload.get("icon_count") != 30 or len(records) != 30:
        raise RuntimeError("gem manifest must declare exactly 30 icons")
    return records


def import_record(record: dict[str, object]) -> str:
    source = PROJECT_ROOT / str(record["source_png"])
    expected_object_path = str(record["texture_path"])
    name = source.stem
    if png_dimensions(source) != EXPECTED_SIZE:
        raise RuntimeError(f"unexpected gem source size: {source}")
    expected_from_name = f"{DESTINATION}/{name}.{name}"
    if expected_object_path != expected_from_name:
        raise RuntimeError(f"gem manifest path mismatch: {expected_object_path}")
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        unreal.EditorAssetLibrary.make_directory(DESTINATION)

    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = DESTINATION
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    package_path = f"{DESTINATION}/{name}"
    texture = unreal.EditorAssetLibrary.load_asset(package_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"failed to import gem Texture2D: {package_path}")
    configure(texture)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError(f"failed to save gem texture: {package_path}")
    return texture.get_path_name()


def main() -> None:
    imported = [import_record(record) for record in load_records()]
    unreal.EditorAssetLibrary.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True)
    print(json.dumps({"ok": True, "imported_count": len(imported), "imported": imported}, ensure_ascii=False))


if __name__ == "__main__":
    main()
