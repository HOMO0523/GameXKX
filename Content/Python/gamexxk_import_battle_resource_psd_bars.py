"""Import PSD battle HP/MP Track + Full textures into the UI asset tree."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = PROJECT_ROOT / "SourceArt" / "UI" / "Battle" / "ResourceBars"
DESTINATION = "/Game/GameXXK/UI/Battle/ResourceBars"
EXPECTED_SIZE = (420, 32)
IMPORTS: tuple[tuple[str, str], ...] = (
    ("battle_psd_health_track.png", "T_BattlePsd_HealthTrack"),
    ("battle_psd_health_full.png", "T_BattlePsd_HealthFull"),
    ("battle_psd_mana_track.png", "T_BattlePsd_ManaTrack"),
    ("battle_psd_mana_full.png", "T_BattlePsd_ManaFull"),
)


def _read_png_dimensions(path: Path) -> tuple[int, int]:
    header = path.read_bytes()[:24]
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise RuntimeError(f"Expected PNG source: {path}")
    return int.from_bytes(header[16:20], "big"), int.from_bytes(header[20:24], "big")


def _try_set(texture: unreal.Texture2D, name: str, value: object) -> None:
    try:
        texture.set_editor_property(name, value)
    except (AttributeError, RuntimeError):
        pass


def _configure_texture(texture: unreal.Texture2D) -> None:
    _try_set(texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    _try_set(texture, "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    _try_set(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    _try_set(texture, "filter", unreal.TextureFilter.TF_BILINEAR)
    _try_set(texture, "address_x", unreal.TextureAddress.TA_CLAMP)
    _try_set(texture, "address_y", unreal.TextureAddress.TA_CLAMP)
    _try_set(texture, "srgb", True)
    _try_set(texture, "never_stream", True)
    _try_set(texture, "compression_no_alpha", False)


def _import_texture(filename: str, asset_name: str) -> str:
    source = SOURCE_DIR / filename
    if not source.is_file():
        raise RuntimeError(f"Missing generated PSD resource source: {source}")
    if _read_png_dimensions(source) != EXPECTED_SIZE:
        raise RuntimeError(f"Unexpected PSD resource bar size for {source}: {_read_png_dimensions(source)}")
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        unreal.EditorAssetLibrary.make_directory(DESTINATION)

    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = DESTINATION
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = f"{DESTINATION}/{asset_name}"
    texture = unreal.EditorAssetLibrary.load_asset(f"{asset_path}.{asset_name}") or unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Failed to import Texture2D: {asset_path}")
    _configure_texture(texture)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture.get_path_name()


def main() -> None:
    imported = [_import_texture(filename, asset_name) for filename, asset_name in IMPORTS]
    unreal.EditorAssetLibrary.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True)
    print(json.dumps({"ok": True, "imported_count": len(imported), "imported": imported}, ensure_ascii=False))


if __name__ == "__main__":
    main()
