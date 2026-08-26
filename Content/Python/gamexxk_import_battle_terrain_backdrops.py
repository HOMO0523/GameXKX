"""Import the seven approved terrain-v2 battle backdrops for Formation Master terrain adaptation.

Two-stage by design: the default invocation only validates the immutable local
provenance record (manifest hashes and PNG headers), while ``--execute-import``
creates or validates the seven isolated project-owned textures. It never
replaces an existing texture with a different source.
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
except ModuleNotFoundError:  # Allows local provenance checks outside the editor.
    unreal = None


PROJECT_ROOT = Path(__file__).resolve().parents[2]
ASSET_ROOT = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "battle-backdrop"
MANIFEST_PATH = ASSET_ROOT / "battle-terrain-manifest-v2.json"

TEXTURE_DIR = "/Game/GameXXK/UI/Battle/Textures"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

EXPECTED_MANIFEST_HEADER = {
    "schema_version": 1,
    "generation_mode": "built_in_imagegen_terrain_set",
    "asset_key": "BattleArena.Terrain.GeneratedV2",
    "source_alpha_policy": "opaque_background",
}

STYLE_REFERENCE_PATH_FRAGMENT = "000.png"
STYLE_REFERENCE_ROLE_FRAGMENT = "style reference only"


def _canonical_asset_path(asset_or_path: object) -> str:
    if hasattr(asset_or_path, "get_path_name"):
        value = str(asset_or_path.get_path_name())
    else:
        value = str(asset_or_path)
    return value.split(".", 1)[0]


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _png_info(path: Path) -> dict[str, int]:
    raw = path.read_bytes()
    if len(raw) < 29 or raw[:8] != PNG_SIGNATURE or raw[12:16] != b"IHDR":
        raise ValueError(f"terrain backdrop source is not a valid PNG: {path}")
    width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(
        ">IIBBBBB", raw[16:29]
    )
    if width <= 0 or height <= 0 or bit_depth != 8:
        raise ValueError(f"terrain backdrop source PNG header is unsupported: {path}")
    if compression != 0 or filter_method != 0:
        raise ValueError(f"terrain backdrop source PNG header is invalid: {path}")
    return {
        "width": width,
        "height": height,
        "color_type": color_type,
        "interlace": interlace,
    }


def _load_manifest() -> dict[str, Any]:
    if not MANIFEST_PATH.is_file():
        raise FileNotFoundError(f"terrain backdrop manifest is missing: {MANIFEST_PATH}")
    return json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))


def validate_terrain_backdrop_plan() -> dict[str, Any]:
    """Validate only local source provenance and immutable destination names."""
    manifest = _load_manifest()
    for key, value in EXPECTED_MANIFEST_HEADER.items():
        if manifest.get(key) != value:
            raise ValueError(f"terrain backdrop manifest {key} changed: {manifest.get(key)!r}")

    entries = manifest.get("terrain_textures", [])
    if not isinstance(entries, list) or len(entries) != 7:
        raise ValueError(f"terrain backdrop manifest must list exactly 7 terrain textures, got {len(entries)}")

    terrain_names: set[str] = set()
    plan: list[dict[str, Any]] = []
    for entry in entries:
        terrain = str(entry.get("terrain", ""))
        if not terrain or terrain in terrain_names:
            raise ValueError(f"duplicate or missing terrain name in manifest: {terrain!r}")
        terrain_names.add(terrain)

        asset_path = str(entry.get("unreal_asset", ""))
        expected_asset = f"/Game/GameXXK/UI/Battle/Textures/T_BattleArena_{terrain}_GeneratedV2"
        if asset_path != expected_asset:
            raise ValueError(f"terrain backdrop asset path changed: {asset_path!r}")

        source = ASSET_ROOT / str(entry.get("source_image", ""))
        if source.parent != ASSET_ROOT / "terrain-v2" or not source.is_file():
            raise FileNotFoundError(f"terrain backdrop source is missing or escapes its asset root: {source}")
        actual_hash = _sha256(source)
        if actual_hash != entry.get("source_sha256"):
            raise ValueError(f"terrain backdrop source hash changed: {source}")

        image = _png_info(source)
        if [image["width"], image["height"]] != list(entry.get("source_size", [])):
            raise ValueError(f"terrain backdrop source dimensions changed: {image}")
        if image["color_type"] != 2:
            raise ValueError(
                f"terrain backdrop source must be opaque RGB PNG, got color type {image['color_type']}: {source}"
            )

        plan.append(
            {
                "terrain": terrain,
                "source": str(source),
                "source_sha256": actual_hash,
                "source_size": [image["width"], image["height"]],
                "texture_asset": asset_path,
            }
        )

    style_reference = manifest.get("style_reference", {})
    if STYLE_REFERENCE_PATH_FRAGMENT not in str(style_reference.get("path", "")):
        raise ValueError("terrain backdrop manifest lost its approved PSD style reference")
    if STYLE_REFERENCE_ROLE_FRAGMENT not in str(style_reference.get("role", "")):
        raise ValueError("terrain backdrop manifest style reference role is unsafe")

    return {"ok": True, "manifest": str(MANIFEST_PATH), "plan": plan}


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("UE Python is required to import the terrain backdrops")


def _ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError(f"could not create terrain backdrop asset directory: {path}")


def _configure_world_texture(texture: object) -> None:
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_World)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_TRILINEAR)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("never_stream", False)


def _validate_texture(texture: object, source: Path, asset_path: str, expected_size: list[int]) -> dict[str, Any]:
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"terrain backdrop is not a Texture2D: {_canonical_asset_path(texture)}")
    if _canonical_asset_path(texture) != asset_path:
        raise RuntimeError(
            f"terrain backdrop texture resolves outside the isolated asset path: {_canonical_asset_path(texture)}"
        )
    size = [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())]
    if size != expected_size:
        raise RuntimeError(f"terrain backdrop imported with unexpected dimensions: {size}")
    import_data = texture.get_editor_property("asset_import_data")
    imported_filename = str(import_data.get_first_filename()) if import_data else ""
    if not imported_filename or Path(imported_filename).resolve() != source.resolve():
        raise RuntimeError(f"terrain backdrop texture source mismatch: {imported_filename}")
    if texture.get_editor_property("lod_group") != unreal.TextureGroup.TEXTUREGROUP_World:
        raise RuntimeError("terrain backdrop texture must remain in the world texture group")
    return {"path": asset_path, "size": size, "imported_filename": imported_filename}


def _import_or_validate_texture(entry: dict[str, Any]) -> tuple[object, bool, dict[str, Any]]:
    source = Path(entry["source"])
    asset_path = entry["texture_asset"]
    asset_name = asset_path.rsplit("/", 1)[-1]
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        texture = unreal.EditorAssetLibrary.load_asset(asset_path)
        return texture, False, _validate_texture(texture, source, asset_path, entry["source_size"])

    _ensure_directory(TEXTURE_DIR)
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = TEXTURE_DIR
    task.destination_name = asset_name
    task.automated = True
    task.save = False
    task.replace_existing = False
    task.replace_existing_settings = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if texture is None:
        raise RuntimeError(f"terrain backdrop import did not produce {asset_path}")
    _configure_world_texture(texture)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError(f"could not save imported terrain backdrop texture: {asset_path}")
    return texture, True, _validate_texture(texture, source, asset_path, entry["source_size"])


def execute_terrain_backdrop_import() -> dict[str, Any]:
    """Create or validate all seven terrain backdrop textures inside the editor."""
    _require_unreal()
    plan = validate_terrain_backdrop_plan()
    results: list[dict[str, Any]] = []
    for entry in plan["plan"]:
        _, created, validation = _import_or_validate_texture(entry)
        results.append({"terrain": entry["terrain"], "created": created, "validation": validation})
    return {"ok": True, "results": results}


def main() -> int:
    parser = argparse.ArgumentParser(description="Import GameXXK terrain-v2 battle backdrops")
    parser.add_argument("--execute-import", action="store_true", help="create the assets inside a running editor")
    args = parser.parse_args()
    if args.execute_import:
        print(json.dumps(execute_terrain_backdrop_import(), ensure_ascii=False, indent=2))
    else:
        print(json.dumps(validate_terrain_backdrop_plan(), ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
