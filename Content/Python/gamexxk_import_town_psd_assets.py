"""Import the authoritative town PSD atoms into a separate UE texture tree.

Run through the project UE MCP runner while the editor is open.  New textures
live under /Game/GameXXK/UI/Town/Textures/PSD and never overwrite legacy trees.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS_ROOT = PROJECT_ROOT / "scripts"
if str(SCRIPTS_ROOT) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_ROOT))

from town_psd_import_manifest import TownPsdImport, build_import_plan


PACKAGE_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "PSD" / "town-v2"
DESTINATION_ROOT = "/Game/GameXXK/UI/Town/Textures/PSD"


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


def import_texture(item: TownPsdImport, *, replace_existing: bool) -> tuple[str, bool]:
    destination = f"{DESTINATION_ROOT}/{item.group}"
    ensure_directory(destination)
    asset_path = f"{destination}/{item.asset_name}"
    object_path = f"{asset_path}.{item.asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path) and not replace_existing:
        return asset_path, False

    task = unreal.AssetImportTask()
    task.filename = str(item.source)
    task.destination_path = destination
    task.destination_name = item.asset_name
    task.automated = True
    task.replace_existing = replace_existing
    task.replace_existing_settings = replace_existing
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
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture.get_path_name(), True


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--groups", nargs="+", default=None, help="Optional import groups")
    parser.add_argument(
        "--replace-existing",
        action="store_true",
        help="Replace only prior PSD-tree imports; defaults to preserving existing assets.",
    )
    args = parser.parse_args(argv)
    plan = build_import_plan(PACKAGE_ROOT)
    selected_groups = set(args.groups) if args.groups else None
    if selected_groups:
        available_groups = {item.group for item in plan}
        unknown_groups = selected_groups.difference(available_groups)
        if unknown_groups:
            raise RuntimeError(f"unknown PSD groups: {sorted(unknown_groups)}")
        plan = tuple(item for item in plan if item.group in selected_groups)
    imported = [import_texture(item, replace_existing=args.replace_existing) for item in plan]
    print(json.dumps({
        "ok": True,
        "destination_root": DESTINATION_ROOT,
        "imported_count": sum(1 for _, did_import in imported if did_import),
        "preserved_count": sum(1 for _, did_import in imported if not did_import),
        "assets": [asset_path for asset_path, _ in imported],
    }, ensure_ascii=False))


if __name__ == "__main__":
    main()
