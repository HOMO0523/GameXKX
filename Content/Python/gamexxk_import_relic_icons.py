"""Import the reviewed 30-icon route-relic set without touching other UI assets."""

from __future__ import annotations

import json
from pathlib import Path
import struct

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = PROJECT_ROOT / "SourceArt" / "UI" / "Relics" / "final"
DESTINATION = "/Game/GameXXK/UI/Relics/Icons"
SLUGS = (
    "AncientCoin", "JadeBell", "BambooTally", "TigerSeal", "MedicineGourd",
    "InkTalisman", "CloudMirror", "StoneBead", "CraneFeather", "IronKnot",
    "TeaBrick", "Compass", "RedCord", "BronzeNeedle", "RainCape",
    "ChessStone", "DrumCharm", "LotusSeed", "SwordGuard", "OldMap",
    "PineCone", "RiverPearl", "CandleStub", "FoxMask", "StoneLion",
    "WineCup", "HerbBasket", "PaperCrane", "BrokenArrow", "MoonDisc",
)
EXPECTED_SIZE = (512, 512)


def _png_dimensions(source: Path) -> tuple[int, int]:
    header = source.read_bytes()[:24]
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise RuntimeError(f"expected PNG source: {source}")
    return struct.unpack(">II", header[16:24])


def _try_set(texture: unreal.Texture2D, name: str, value: object) -> None:
    try:
        texture.set_editor_property(name, value)
    except (AttributeError, RuntimeError):
        pass


def _configure(texture: unreal.Texture2D) -> None:
    _try_set(texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    _try_set(texture, "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    _try_set(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    _try_set(texture, "filter", unreal.TextureFilter.TF_BILINEAR)
    _try_set(texture, "address_x", unreal.TextureAddress.TA_CLAMP)
    _try_set(texture, "address_y", unreal.TextureAddress.TA_CLAMP)
    _try_set(texture, "srgb", True)
    _try_set(texture, "never_stream", True)
    _try_set(texture, "compression_no_alpha", False)


def _import(slug: str) -> str:
    name = f"T_Relic_{slug}"
    source = SOURCE_DIR / f"{name}.png"
    if not source.is_file():
        raise RuntimeError(f"missing approved relic source: {source}")
    if _png_dimensions(source) != EXPECTED_SIZE:
        raise RuntimeError(f"unexpected relic source size: {source}")
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

    asset_path = f"{DESTINATION}/{name}"
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(texture, unreal.Texture2D):
        actual = texture.get_class().get_name() if texture else "None"
        raise RuntimeError(f"failed to import Texture2D: {asset_path}; class={actual}")
    _configure(texture)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError(f"failed to save imported relic texture: {asset_path}")
    return texture.get_path_name()


def main() -> None:
    imported = [_import(slug) for slug in SLUGS]
    unreal.EditorAssetLibrary.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True)
    print(json.dumps({"ok": True, "imported_count": len(imported), "imported": imported}, ensure_ascii=False))


if __name__ == "__main__":
    main()
