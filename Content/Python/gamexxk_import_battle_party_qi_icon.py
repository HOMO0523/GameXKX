"""Import the approved shared-Qi battle HUD icon with UI-safe texture settings.

The source is intentionally separate from the generated keyed source: only the
chroma-cleaned RGBA PNG below is imported into Unreal.  The widget overlays the
runtime value itself, so this texture must never contain a baked numeral.
"""

from __future__ import annotations

import json
from pathlib import Path
import struct

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE = PROJECT_ROOT / "SourceArt" / "UI" / "Battle" / "PartyQi" / "battle_party_qi_soul_orb_v1.png"
DESTINATION = "/Game/GameXXK/UI/Battle/PartyQi"
ASSET_NAME = "T_BattlePartyQi_SoulOrb"
EXPECTED_SIZE = (1254, 1254)


def _read_png_dimensions(source: Path) -> tuple[int, int]:
    header = source.read_bytes()[:24]
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise RuntimeError(f"expected PNG source: {source}")
    return struct.unpack(">II", header[16:24])


def _verify_source() -> None:
    if not SOURCE.is_file():
        raise RuntimeError(f"missing approved Party Qi source: {SOURCE}")
    if _read_png_dimensions(SOURCE) != EXPECTED_SIZE:
        raise RuntimeError(f"unexpected Party Qi source size: {SOURCE}")


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


def main() -> None:
    _verify_source()
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        unreal.EditorAssetLibrary.make_directory(DESTINATION)

    task = unreal.AssetImportTask()
    task.filename = str(SOURCE)
    task.destination_path = DESTINATION
    task.destination_name = ASSET_NAME
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = f"{DESTINATION}/{ASSET_NAME}"
    texture = (
        unreal.EditorAssetLibrary.load_asset(f"{asset_path}.{ASSET_NAME}")
        or unreal.EditorAssetLibrary.load_asset(asset_path)
    )
    if not isinstance(texture, unreal.Texture2D):
        loaded_class = texture.get_class().get_name() if texture else "None"
        raise RuntimeError(f"failed to import Texture2D: {asset_path}; class={loaded_class}")

    _configure_texture(texture)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    unreal.EditorAssetLibrary.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True)
    print(json.dumps({"ok": True, "asset": texture.get_path_name()}, ensure_ascii=False))


if __name__ == "__main__":
    main()
