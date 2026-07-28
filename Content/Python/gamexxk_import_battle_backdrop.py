"""Import the approved generated battle backdrop without touching existing level assets.

This script is deliberately two-stage: the default invocation only validates the
immutable local provenance record, while ``--execute-import`` creates or
validates two isolated project-owned assets.  It never replaces an existing
texture or material with a different source.
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
MANIFEST_PATH = ASSET_ROOT / "battle-arena-manifest-v1.json"

TEXTURE_DIR = "/Game/GameXXK/UI/Battle/Textures"
TEXTURE_NAME = "T_BattleArena_Riverside_GeneratedV1"
TEXTURE_ASSET_PATH = "/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Riverside_GeneratedV1"

MATERIAL_DIR = "/Game/GameXXK/UI/Battle/Materials"
MATERIAL_NAME = "M_BattleArena_Riverside_GeneratedV1"
MATERIAL_ASSET_PATH = "/Game/GameXXK/UI/Battle/Materials/M_BattleArena_Riverside_GeneratedV1"

TEXTURE_PARAMETER_NAME = "BattleBackdropTexture"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


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
        raise ValueError(f"battle backdrop source is not a valid PNG: {path}")
    width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(
        ">IIBBBBB", raw[16:29]
    )
    if width <= 0 or height <= 0 or bit_depth != 8:
        raise ValueError(f"battle backdrop source PNG header is unsupported: {path}")
    if compression != 0 or filter_method != 0:
        raise ValueError(f"battle backdrop source PNG header is invalid: {path}")
    return {
        "width": width,
        "height": height,
        "color_type": color_type,
        "interlace": interlace,
    }


def _load_manifest() -> dict[str, Any]:
    if not MANIFEST_PATH.is_file():
        raise FileNotFoundError(f"battle backdrop manifest is missing: {MANIFEST_PATH}")
    return json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))


def validate_backdrop_plan() -> dict[str, Any]:
    """Validate only local source provenance and immutable destination names."""
    manifest = _load_manifest()
    expected = {
        "schema_version": 1,
        "generation_mode": "built_in_imagegen_style_reference",
        "asset_key": "BattleArena.Riverside.GeneratedV1",
        "source_alpha_policy": "opaque_background",
        "planned_unreal_asset": TEXTURE_ASSET_PATH,
        "planned_material_asset": MATERIAL_ASSET_PATH,
    }
    for key, value in expected.items():
        if manifest.get(key) != value:
            raise ValueError(f"battle backdrop manifest {key} changed: {manifest.get(key)!r}")

    source = ASSET_ROOT / str(manifest.get("source_image", ""))
    if source.parent != ASSET_ROOT or not source.is_file():
        raise FileNotFoundError(f"battle backdrop source is missing or escapes its asset root: {source}")
    actual_hash = _sha256(source)
    if actual_hash != manifest.get("source_sha256"):
        raise ValueError(f"battle backdrop source hash changed: {actual_hash}")

    image = _png_info(source)
    if [image["width"], image["height"]] != list(manifest.get("source_size", [])):
        raise ValueError(f"battle backdrop source dimensions changed: {image}")
    # The locked source is RGB/opaque.  An RGBA source would need an explicit
    # alpha compositing approval rather than silently changing the floor result.
    if image["color_type"] != 2:
        raise ValueError(f"battle backdrop source must be opaque RGB PNG, got color type {image['color_type']}")

    style_reference = manifest.get("style_reference", {})
    if "000.png" not in str(style_reference.get("path", "")):
        raise ValueError("battle backdrop manifest lost its approved PSD style reference")
    if "style reference only" not in str(style_reference.get("role", "")):
        raise ValueError("battle backdrop manifest style reference role is unsafe")

    return {
        "ok": True,
        "source": str(source),
        "source_sha256": actual_hash,
        "source_size": [image["width"], image["height"]],
        "source_color_type": image["color_type"],
        "texture_asset": TEXTURE_ASSET_PATH,
        "material_asset": MATERIAL_ASSET_PATH,
    }


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("UE Python is required to import the battle backdrop")


def _ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError(f"could not create battle backdrop asset directory: {path}")


def _configure_world_texture(texture: object) -> None:
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_World)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_TRILINEAR)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("never_stream", False)


def _validate_texture(texture: object, source: Path, expected_size: list[int]) -> dict[str, Any]:
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"battle backdrop is not a Texture2D: {_canonical_asset_path(texture)}")
    if _canonical_asset_path(texture) != TEXTURE_ASSET_PATH:
        raise RuntimeError(f"battle backdrop texture resolves outside the isolated asset path: {_canonical_asset_path(texture)}")
    size = [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())]
    if size != expected_size:
        raise RuntimeError(f"battle backdrop imported with unexpected dimensions: {size}")
    import_data = texture.get_editor_property("asset_import_data")
    imported_filename = str(import_data.get_first_filename()) if import_data else ""
    if not imported_filename or Path(imported_filename).resolve() != source.resolve():
        raise RuntimeError(f"battle backdrop texture source mismatch: {imported_filename}")
    if texture.get_editor_property("lod_group") != unreal.TextureGroup.TEXTUREGROUP_World:
        raise RuntimeError("battle backdrop texture must remain in the world texture group")
    return {"path": TEXTURE_ASSET_PATH, "size": size, "imported_filename": imported_filename}


def _import_or_validate_texture(plan: dict[str, Any]) -> tuple[object, bool, dict[str, Any]]:
    source = Path(plan["source"])
    if unreal.EditorAssetLibrary.does_asset_exist(TEXTURE_ASSET_PATH):
        texture = unreal.EditorAssetLibrary.load_asset(TEXTURE_ASSET_PATH)
        return texture, False, _validate_texture(texture, source, plan["source_size"])

    _ensure_directory(TEXTURE_DIR)
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = TEXTURE_DIR
    task.destination_name = TEXTURE_NAME
    task.automated = True
    task.save = False
    task.replace_existing = False
    task.replace_existing_settings = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = unreal.EditorAssetLibrary.load_asset(TEXTURE_ASSET_PATH)
    if texture is None:
        raise RuntimeError(f"battle backdrop import did not produce {TEXTURE_ASSET_PATH}")
    _configure_world_texture(texture)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError(f"could not save imported battle backdrop texture: {TEXTURE_ASSET_PATH}")
    return texture, True, _validate_texture(texture, source, plan["source_size"])


def _connect_expression(source: object, output_name: str, destination: object, input_names: tuple[str, ...]) -> str:
    for input_name in input_names:
        if unreal.MaterialEditingLibrary.connect_material_expressions(source, output_name, destination, input_name):
            return input_name
    raise RuntimeError(f"could not connect backdrop material input candidates: {input_names}")


def _validate_material(material: object, texture: object) -> dict[str, Any]:
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"battle backdrop is not a Material: {_canonical_asset_path(material)}")
    if _canonical_asset_path(material) != MATERIAL_ASSET_PATH:
        raise RuntimeError(f"battle backdrop material resolves outside the isolated asset path: {_canonical_asset_path(material)}")
    if material.get_editor_property("material_domain") != unreal.MaterialDomain.MD_SURFACE:
        raise RuntimeError("battle backdrop material must be a surface material")
    if material.get_blend_mode() != unreal.BlendMode.BLEND_OPAQUE:
        raise RuntimeError("battle backdrop material must remain opaque")

    expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))
    samples = [
        expression
        for expression in expressions
        if isinstance(expression, unreal.MaterialExpressionTextureSampleParameter2D)
        and str(expression.get_editor_property("parameter_name")) == TEXTURE_PARAMETER_NAME
        and _canonical_asset_path(expression.get_editor_property("texture")) == TEXTURE_ASSET_PATH
    ]
    if len(samples) != 1:
        raise RuntimeError("battle backdrop material must contain exactly one locked texture parameter")
    front = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material, unreal.MaterialProperty.MP_FRONT_MATERIAL
    )
    if not isinstance(front, unreal.MaterialExpressionSubstrateUnlitBSDF):
        raise RuntimeError("battle backdrop material front output must be a Substrate Unlit BSDF")
    return {"path": MATERIAL_ASSET_PATH, "texture_parameter": TEXTURE_PARAMETER_NAME}


def _create_or_validate_material(texture: object) -> tuple[object, bool, dict[str, Any]]:
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_ASSET_PATH):
        material = unreal.EditorAssetLibrary.load_asset(MATERIAL_ASSET_PATH)
        return material, False, _validate_material(material, texture)

    _ensure_directory(MATERIAL_DIR)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MATERIAL_NAME,
        MATERIAL_DIR,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if material is None:
        raise RuntimeError(f"could not create battle backdrop material: {MATERIAL_ASSET_PATH}")
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("two_sided", False)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    texture_node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        -420,
        0,
    )
    texture_node.set_editor_property("texture", texture)
    texture_node.set_editor_property("parameter_name", TEXTURE_PARAMETER_NAME)
    unlit_node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionSubstrateUnlitBSDF,
        20,
        0,
    )
    _connect_expression(texture_node, "RGB", unlit_node, ("Emissive Color", "EmissiveColor"))
    if not unreal.MaterialEditingLibrary.connect_material_property(
        unlit_node,
        "",
        unreal.MaterialProperty.MP_FRONT_MATERIAL,
    ):
        raise RuntimeError("could not connect battle backdrop Unlit BSDF to Front Material")

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    errors = [str(value) for value in unreal.MaterialEditingLibrary.recompile_material(material)]
    if errors:
        raise RuntimeError(f"battle backdrop material failed to compile: {errors}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"could not save battle backdrop material: {MATERIAL_ASSET_PATH}")
    return material, True, _validate_material(material, texture)


def import_verified_backdrop() -> dict[str, Any]:
    """Create only missing isolated assets, or validate exact pre-existing ones."""
    _require_unreal()
    plan = validate_backdrop_plan()
    texture, texture_created, texture_report = _import_or_validate_texture(plan)
    material, material_created, material_report = _create_or_validate_material(texture)
    return {
        **plan,
        "texture_created": texture_created,
        "material_created": material_created,
        "texture": texture_report,
        "material": material_report,
    }


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
    parser.add_argument(
        "--execute-import",
        action="store_true",
        help="Create only the verified isolated Texture2D and Substrate material.",
    )
    args = parser.parse_args(argv)
    result = import_verified_backdrop() if args.execute_import else validate_backdrop_plan()
    print(json.dumps(_jsonable(result), ensure_ascii=False, indent=2))
    return result


if __name__ == "__main__":
    main()
