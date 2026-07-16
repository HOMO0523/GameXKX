"""Import reviewed GameXXK WorldMap UI atoms into the owned Maps texture tree."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = PROJECT_ROOT / "docs" / "ui" / "maps" / "source_art"
DESTINATION_ROOT = "/Game/GameXXK/UI/Maps/Textures"

IMPORTS: tuple[tuple[str, str, str], ...] = (
    ("WorldMap", "world_map_terrain.png", "T_WorldMap_Terrain"),
    ("WorldMap", "world_map_region_paths.png", "T_WorldMap_RegionPaths"),
    ("WorldMap", "world_map_qingshan_marker.png", "T_WorldMap_QingshanMarker"),
    ("WorldMap", "world_map_locked_marker.png", "T_WorldMap_LockedMarker"),
    ("WorldMap", "world_map_player_marker.png", "T_WorldMap_PlayerMarker"),
    ("WorldMap", "world_map_label_plate.png", "T_WorldMap_RegionLabelPlate"),
)


def ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def try_set(texture: unreal.Texture2D, name: str, value: object) -> None:
    try:
        texture.set_editor_property(name, value)
    except (AttributeError, RuntimeError):
        pass


def configure_texture(texture: unreal.Texture2D) -> None:
    try_set(texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    try_set(texture, "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    try_set(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    try_set(texture, "filter", unreal.TextureFilter.TF_BILINEAR)
    try_set(texture, "address_x", unreal.TextureAddress.TA_CLAMP)
    try_set(texture, "address_y", unreal.TextureAddress.TA_CLAMP)
    try_set(texture, "srgb", True)
    try_set(texture, "never_stream", True)
    try_set(texture, "compression_no_alpha", False)


def save_texture(texture: unreal.Texture2D, asset_path: str) -> None:
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError(f"failed to save imported Texture2D: {asset_path}")


def import_texture(group: str, filename: str, asset_name: str) -> str:
    source = SOURCE_ROOT / group / filename
    if not source.is_file():
        raise RuntimeError(f"missing reviewed map source atom: {source}")

    destination = f"{DESTINATION_ROOT}/{group}"
    ensure_directory(destination)
    asset_path = f"{destination}/{asset_name}"
    object_path = f"{asset_path}.{asset_name}"
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = destination
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = (
        unreal.EditorAssetLibrary.load_asset(object_path)
        or unreal.EditorAssetLibrary.load_asset(asset_path)
    )
    if not isinstance(texture, unreal.Texture2D):
        loaded_class = texture.get_class().get_name() if texture else "None"
        raise RuntimeError(f"failed to import Texture2D: {asset_path}; class={loaded_class}")
    configure_texture(texture)
    save_texture(texture, asset_path)
    return texture.get_path_name()


def select_imports(
    groups: list[str] | None,
    asset_names: list[str] | None,
) -> tuple[tuple[str, str, str], ...]:
    if groups and asset_names:
        raise RuntimeError("use either --groups or --assets, not both")
    if asset_names:
        requested_assets = set(asset_names)
        available_assets = {asset_name for _, _, asset_name in IMPORTS}
        unknown_assets = requested_assets.difference(available_assets)
        if unknown_assets:
            raise RuntimeError(f"unknown UI source assets: {sorted(unknown_assets)}")
        return tuple(entry for entry in IMPORTS if entry[2] in requested_assets)
    if groups:
        requested_groups = set(groups)
        available_groups = {group for group, _, _ in IMPORTS}
        unknown_groups = requested_groups.difference(available_groups)
        if unknown_groups:
            raise RuntimeError(f"unknown UI source groups: {sorted(unknown_groups)}")
        return tuple(entry for entry in IMPORTS if entry[0] in requested_groups)
    return IMPORTS


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description="Import reviewed GameXXK WorldMap UI atoms.")
    parser.add_argument("--groups", nargs="+", default=None, help="Optional source-art groups to import.")
    parser.add_argument("--assets", nargs="+", default=None, help="Optional exact asset names to import.")
    args = parser.parse_args(argv)
    selected_imports = select_imports(args.groups, args.assets)
    imported = [import_texture(group, filename, asset_name) for group, filename, asset_name in selected_imports]
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    print(json.dumps({
        "ok": True,
        "groups": sorted({group for group, _, _ in selected_imports}),
        "imported_count": len(imported),
        "imported": imported,
    }, ensure_ascii=False))


if __name__ == "__main__":
    main()
