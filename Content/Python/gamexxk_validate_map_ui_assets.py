"""Validate imported GameXXK WorldMap UI textures inside Unreal Editor."""

from __future__ import annotations

import json
import sys

import unreal


DESTINATION_ROOT = "/Game/GameXXK/UI/Maps/Textures"
WORLD_MAP_ROOT = f"{DESTINATION_ROOT}/WorldMap"
TEXTURE_PATHS: tuple[str, ...] = (
    f"{WORLD_MAP_ROOT}/T_WorldMap_Terrain",
    f"{WORLD_MAP_ROOT}/T_WorldMap_RegionPaths",
    f"{WORLD_MAP_ROOT}/T_WorldMap_QingshanMarker",
    f"{WORLD_MAP_ROOT}/T_WorldMap_LockedMarker",
    f"{WORLD_MAP_ROOT}/T_WorldMap_PlayerMarker",
    f"{WORLD_MAP_ROOT}/T_WorldMap_RegionLabelPlate",
)

EXPECTED_PROPERTIES = {
    "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
    "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
    "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
    "filter": unreal.TextureFilter.TF_BILINEAR,
    "address_x": unreal.TextureAddress.TA_CLAMP,
    "address_y": unreal.TextureAddress.TA_CLAMP,
    "srgb": True,
    "never_stream": True,
    "compression_no_alpha": False,
}


def get_editor_property(asset: object, name: str) -> object | None:
    try:
        return asset.get_editor_property(name)
    except (AttributeError, RuntimeError):
        return None


def get_source_filename(texture: unreal.Texture2D) -> str:
    import_data = get_editor_property(texture, "asset_import_data")
    get_first_filename = getattr(import_data, "get_first_filename", None)
    if not callable(get_first_filename):
        return ""
    try:
        return str(get_first_filename())
    except RuntimeError:
        return ""


def get_object_path(texture: unreal.Texture2D) -> str:
    try:
        return str(texture.get_path_name())
    except (AttributeError, RuntimeError):
        return ""


def validate_texture(path: str, texture: object) -> list[str]:
    errors: list[str] = []
    if not path.startswith(f"{WORLD_MAP_ROOT}/"):
        errors.append(f"{path} must remain under {DESTINATION_ROOT}")
    if not isinstance(texture, unreal.Texture2D):
        errors.append(f"{path} is not a Texture2D")
        return errors

    object_path = get_object_path(texture)
    if not object_path.startswith(f"{WORLD_MAP_ROOT}/"):
        errors.append(f"{path} actual object path must remain under {WORLD_MAP_ROOT}")
    if object_path.startswith("/Game/1Game/"):
        errors.append(f"{path} resolves to forbidden /Game/1Game content")

    source_filename = get_source_filename(texture)
    if not source_filename:
        errors.append(f"{path} has no import provenance")
    elif "094.png" in source_filename.casefold() or "完整底图" in source_filename:
        errors.append(f"{path} imports the baked PSD map")

    for property_name, expected_value in EXPECTED_PROPERTIES.items():
        actual_value = get_editor_property(texture, property_name)
        if actual_value != expected_value:
            errors.append(f"{path} {property_name} is not configured for UI")
    return errors


def main() -> None:
    missing: list[str] = []
    errors: list[str] = []
    validated: list[str] = []
    for path in TEXTURE_PATHS:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset:
            missing.append(path)
            continue
        asset_errors = validate_texture(path, asset)
        if asset_errors:
            errors.extend(asset_errors)
            continue
        validated.append(path)

    result = {
        "ok": not missing and not errors,
        "missing": missing,
        "errors": errors,
        "validated": validated,
        "validated_count": len(validated),
        "expected_count": len(TEXTURE_PATHS),
    }
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
