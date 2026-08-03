"""Create the isolated fullscreen battle backdrop and atlas UI material.

The script is read-only unless ``--execute-import`` is supplied.  Existing
targets are treated as collisions: they must already satisfy the complete
generated-asset contract, otherwise execution stops without modifying them.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path
from typing import Any, Iterable

try:
    import unreal
except ImportError:
    unreal = None


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_IMAGE = (
    PROJECT_ROOT
    / "SourceAssets"
    / "PartyDeck"
    / "battle-backdrop"
    / "battle_arena_riverside_source_v1.png"
)
SOURCE_IMAGE_SHA256 = "ab8b882de676b74693cb2d3c279ca11f2126aaa4fdf382dedaa810b34ac1a3a8"
SOURCE_IMAGE_SIZE = (1672, 941)
SOURCE_IMAGE_MODE = "RGB"

BACKDROP_PACKAGE = "/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Riverside_GeneratedV1"
MATERIAL_PACKAGE = "/Game/GameXXK/UI/Battle/Materials/M_BattleUnitAtlasUI"
BACKDROP_DESTINATION, BACKDROP_NAME = BACKDROP_PACKAGE.rsplit("/", 1)
MATERIAL_DESTINATION, MATERIAL_NAME = MATERIAL_PACKAGE.rsplit("/", 1)

ATLAS_COLUMNS = 8
ATLAS_ROWS = 8
ENGINE_DEFAULT_TEXTURE = "/Engine/EngineResources/DefaultTexture.DefaultTexture"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"

TEXTURE_POLICY = {
    "lod_group": "TEXTUREGROUP_UI",
    "mip_gen_settings": "TMGS_NO_MIPMAPS",
    "filter": "TF_BILINEAR",
    "srgb": True,
    "address_x": "TA_CLAMP",
    "address_y": "TA_CLAMP",
}
MATERIAL_POLICY = {
    "material_domain": "MD_UI",
    "blend_mode": "BLEND_TRANSLUCENT",
    "texture_parameter": "AtlasTexture",
    "scalar_parameters": {
        "FrameColumn": 0.0,
        "FrameRow": 0.0,
    },
    "atlas_columns": ATLAS_COLUMNS,
    "atlas_rows": ATLAS_ROWS,
    "color_output": "MP_EMISSIVE_COLOR",
    "alpha_output": "MP_OPACITY",
}


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _png_metadata(path: Path) -> tuple[tuple[int, int], str, bool]:
    data = path.read_bytes()
    if len(data) < 33 or data[:8] != PNG_SIGNATURE or data[12:16] != b"IHDR":
        raise ValueError(f"source is not a readable PNG: {path}")

    width, height, bit_depth, color_type = struct.unpack(">IIBB", data[16:26])
    modes = {0: "L", 2: "RGB", 3: "P", 4: "LA", 6: "RGBA"}
    mode = modes.get(color_type, f"PNG_COLOR_TYPE_{color_type}")

    chunk_names: list[bytes] = []
    cursor = 8
    while cursor + 12 <= len(data):
        chunk_length = struct.unpack(">I", data[cursor : cursor + 4])[0]
        chunk_name = data[cursor + 4 : cursor + 8]
        chunk_names.append(chunk_name)
        cursor += 12 + chunk_length
        if chunk_name == b"IEND":
            break
    opaque = color_type in (0, 2) and b"tRNS" not in chunk_names
    if bit_depth != 8:
        raise ValueError(f"source PNG bit depth changed: expected 8, got {bit_depth}")
    return (width, height), mode, opaque


def validate_source_image() -> dict[str, Any]:
    if not SOURCE_IMAGE.is_file():
        raise FileNotFoundError(f"fullscreen battle backdrop source is missing: {SOURCE_IMAGE}")
    observed_hash = _sha256(SOURCE_IMAGE)
    if observed_hash != SOURCE_IMAGE_SHA256:
        raise ValueError(
            "fullscreen battle backdrop source hash changed: "
            f"expected {SOURCE_IMAGE_SHA256}, got {observed_hash}"
        )
    size, mode, opaque = _png_metadata(SOURCE_IMAGE)
    if size != SOURCE_IMAGE_SIZE or mode != SOURCE_IMAGE_MODE or not opaque:
        raise ValueError(
            "fullscreen battle backdrop source format changed: "
            f"expected size={SOURCE_IMAGE_SIZE} mode={SOURCE_IMAGE_MODE} opaque=True, "
            f"got size={size} mode={mode} opaque={opaque}"
        )
    return {
        "path": str(SOURCE_IMAGE),
        "sha256": observed_hash,
        "size": list(size),
        "mode": mode,
        "opaque": opaque,
    }


def build_asset_plan(backdrop_exists: bool, material_exists: bool) -> dict[str, Any]:
    existence = (
        (BACKDROP_PACKAGE, bool(backdrop_exists)),
        (MATERIAL_PACKAGE, bool(material_exists)),
    )
    create = [package for package, exists in existence if not exists]
    validate = [package for package, exists in existence if exists]
    return {"create": create, "validate": validate, "write_count": len(create)}


def classify_target_action(package_persisted: bool, asset_exists: bool) -> str:
    if package_persisted:
        return "validate"
    return "recover" if asset_exists else "create"


def _package_file(package: str) -> Path:
    prefix = "/Game/"
    if not package.startswith(prefix):
        raise ValueError(f"target is not a project content package: {package}")
    return PROJECT_ROOT / "Content" / Path(*package[len(prefix) :].split("/")).with_suffix(
        ".uasset"
    )


def parse_arguments(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--execute-import",
        action="store_true",
        help="Create missing targets after validating the source and all collisions.",
    )
    return parser.parse_args(argv)


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("UE Python is required when --execute-import is supplied")


def _set_required(obj: object, property_name: str, value: object) -> None:
    try:
        obj.set_editor_property(property_name, value)
    except (AttributeError, RuntimeError) as exc:
        raise RuntimeError(f"could not set {property_name} on {obj}: {exc}") from exc


def _set_if_supported(obj: object, property_name: str, value: object) -> bool:
    try:
        obj.set_editor_property(property_name, value)
    except (AttributeError, RuntimeError):
        return False
    return True


def _get_if_supported(obj: object, property_name: str) -> tuple[bool, object | None]:
    try:
        return True, obj.get_editor_property(property_name)
    except (AttributeError, RuntimeError):
        return False, None


def _enum_label(value: object, expected_label: str) -> str:
    text = str(value)
    return expected_label if expected_label in text else text


def _asset_object_path(asset: object) -> str:
    get_path_name = getattr(asset, "get_path_name", None)
    return str(get_path_name()) if callable(get_path_name) else str(asset)


def is_allowed_default_texture(object_path: str) -> bool:
    return str(object_path) == ENGINE_DEFAULT_TEXTURE


def _normalized_input_name(value: str) -> str:
    return "".join(character for character in str(value).casefold() if character.isalnum())


def material_input_candidates(
    preferred_inputs: Iterable[str], reflected_inputs: Iterable[str]
) -> list[str]:
    preferred = [str(value) for value in preferred_inputs]
    reflected = [str(value) for value in reflected_inputs]
    reflected_by_name = {
        _normalized_input_name(value): value for value in reflected
    }
    candidates: list[str] = []
    for value in preferred:
        reflected_value = reflected_by_name.get(_normalized_input_name(value))
        if reflected_value is not None and reflected_value not in candidates:
            candidates.append(reflected_value)
    for value in preferred:
        if value not in candidates:
            candidates.append(value)
    return candidates


def material_input_bindings(
    input_names: Iterable[str], input_sources: Iterable[object | None]
) -> list[tuple[str, object | None]]:
    names = [str(value) for value in input_names]
    sources = list(input_sources)
    return [
        (name, sources[index] if index < len(sources) else None)
        for index, name in enumerate(names)
    ]


def _package_name(package: object) -> str:
    for method_name in ("get_name", "get_path_name"):
        method = getattr(package, method_name, None)
        if callable(method):
            return str(method()).split(".", 1)[0]
    return str(package).split(".", 1)[0]


def _dirty_packages_after_save() -> dict[str, list[str]]:
    content = sorted(
        _package_name(package)
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    )
    maps = sorted(
        _package_name(package)
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    )
    return {
        "dirty_content_packages_after_save": content,
        "dirty_map_packages_after_save": maps,
    }


def parse_dimensions_tag(value: str) -> tuple[int, int]:
    parts = [part.strip() for part in str(value).lower().split("x")]
    if len(parts) != 2 or not all(part.isdigit() for part in parts):
        raise ValueError(f"invalid Texture2D Dimensions tag: {value!r}")
    width, height = int(parts[0]), int(parts[1])
    if width <= 0 or height <= 0:
        raise ValueError(f"invalid Texture2D Dimensions tag: {value!r}")
    return width, height


def _registry_texture_dimensions(texture: object) -> tuple[int, int] | None:
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    data = registry.get_asset_by_object_path(_asset_object_path(texture))
    if not data:
        return None
    try:
        tags = {str(key): str(value) for key, value in data.tags_and_values.items()}
    except (AttributeError, RuntimeError):
        return None
    dimensions = tags.get("Dimensions")
    return parse_dimensions_tag(dimensions) if dimensions else None


def _texture_dimensions(texture: object) -> tuple[int, int]:
    registry_dimensions = _registry_texture_dimensions(texture)
    if registry_dimensions is not None:
        return registry_dimensions
    for x_name, y_name in (
        ("blueprint_get_size_x", "blueprint_get_size_y"),
        ("get_size_x", "get_size_y"),
    ):
        get_x = getattr(texture, x_name, None)
        get_y = getattr(texture, y_name, None)
        if callable(get_x) and callable(get_y):
            return int(get_x()), int(get_y())
    raise RuntimeError(f"could not inspect Texture2D dimensions: {_asset_object_path(texture)}")


def _texture_import_source(texture: object) -> str:
    supported, import_data = _get_if_supported(texture, "asset_import_data")
    if not supported or import_data is None:
        raise RuntimeError(f"Texture2D has no import data: {_asset_object_path(texture)}")
    get_first_filename = getattr(import_data, "get_first_filename", None)
    if not callable(get_first_filename):
        raise RuntimeError(f"Texture2D import data has no filename: {_asset_object_path(texture)}")
    return str(get_first_filename())


def _require_property(obj: object, property_name: str, expected: object, context: str) -> object:
    supported, actual = _get_if_supported(obj, property_name)
    if not supported:
        raise RuntimeError(f"{context} does not expose required property {property_name}")
    if actual != expected:
        raise RuntimeError(
            f"{context} has wrong {property_name}: expected {expected}, got {actual}"
        )
    return actual


def _validate_backdrop_asset(texture: object) -> dict[str, Any]:
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"{BACKDROP_PACKAGE} exists but is not a Texture2D")
    if _texture_dimensions(texture) != SOURCE_IMAGE_SIZE:
        raise RuntimeError(
            f"{BACKDROP_PACKAGE} has wrong dimensions: "
            f"expected {SOURCE_IMAGE_SIZE}, got {_texture_dimensions(texture)}"
        )

    imported_source = _texture_import_source(texture)
    if Path(imported_source).resolve() != SOURCE_IMAGE.resolve():
        raise RuntimeError(
            f"{BACKDROP_PACKAGE} has wrong import source: "
            f"expected {SOURCE_IMAGE}, got {imported_source}"
        )

    _require_property(
        texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI, BACKDROP_PACKAGE
    )
    _require_property(
        texture,
        "mip_gen_settings",
        unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        BACKDROP_PACKAGE,
    )
    _require_property(
        texture, "filter", unreal.TextureFilter.TF_BILINEAR, BACKDROP_PACKAGE
    )
    _require_property(texture, "srgb", True, BACKDROP_PACKAGE)

    address_report: dict[str, str] = {}
    for property_name in ("address_x", "address_y"):
        supported, actual = _get_if_supported(texture, property_name)
        if supported:
            if actual != unreal.TextureAddress.TA_CLAMP:
                raise RuntimeError(
                    f"{BACKDROP_PACKAGE} has wrong {property_name}: "
                    f"expected {unreal.TextureAddress.TA_CLAMP}, got {actual}"
                )
            address_report[property_name] = _enum_label(actual, "TA_CLAMP")
        else:
            address_report[property_name] = "unsupported"

    return {
        "package": BACKDROP_PACKAGE,
        "object_path": _asset_object_path(texture),
        "class": texture.get_class().get_name(),
        "size": list(SOURCE_IMAGE_SIZE),
        "source": imported_source,
        "policy": {
            "lod_group": "TEXTUREGROUP_UI",
            "mip_gen_settings": "TMGS_NO_MIPMAPS",
            "filter": "TF_BILINEAR",
            "srgb": True,
            **address_report,
        },
    }


def _configure_new_backdrop(texture: object) -> None:
    _set_required(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    _set_required(
        texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    )
    _set_required(texture, "filter", unreal.TextureFilter.TF_BILINEAR)
    _set_required(texture, "srgb", True)
    _set_if_supported(texture, "address_x", unreal.TextureAddress.TA_CLAMP)
    _set_if_supported(texture, "address_y", unreal.TextureAddress.TA_CLAMP)


def _ensure_directory(path: str) -> None:
    if unreal.EditorAssetLibrary.does_directory_exist(path):
        return
    if not unreal.EditorAssetLibrary.make_directory(path):
        raise RuntimeError(f"could not create asset directory: {path}")


def _import_new_backdrop() -> tuple[object, dict[str, Any]]:
    if unreal.EditorAssetLibrary.does_asset_exist(BACKDROP_PACKAGE):
        raise RuntimeError(f"refusing to import over a newly appeared target: {BACKDROP_PACKAGE}")
    _ensure_directory(BACKDROP_DESTINATION)

    task = unreal.AssetImportTask()
    task.filename = str(SOURCE_IMAGE)
    task.destination_path = BACKDROP_DESTINATION
    task.destination_name = BACKDROP_NAME
    task.automated = True
    task.replace_existing = False
    task.replace_existing_settings = False
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = unreal.EditorAssetLibrary.load_asset(BACKDROP_PACKAGE)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"failed to import Texture2D: {BACKDROP_PACKAGE}")
    _configure_new_backdrop(texture)
    observation = _validate_backdrop_asset(texture)
    _save_backdrop(texture)
    return texture, observation


def _save_backdrop(texture: object) -> None:
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError(f"failed to save generated Texture2D: {BACKDROP_PACKAGE}")


def _node(material: object, node_class: object, x: int, y: int) -> object:
    node = unreal.MaterialEditingLibrary.create_material_expression(
        material, node_class, x, y
    )
    if node is None:
        raise RuntimeError(f"could not create material expression {node_class}")
    return node


def _connect(
    source: object,
    outputs: Iterable[str],
    destination: object,
    preferred_inputs: Iterable[str],
) -> str:
    reflected = [
        str(value)
        for value in unreal.MaterialEditingLibrary.get_material_expression_input_names(
            destination
        )
    ]
    preferred = list(preferred_inputs)
    candidates = material_input_candidates(preferred, reflected)
    for output in outputs:
        for input_name in candidates:
            if unreal.MaterialEditingLibrary.connect_material_expressions(
                source, output, destination, input_name
            ):
                return input_name
    raise RuntimeError(
        f"could not connect {_asset_object_path(source)} to "
        f"{_asset_object_path(destination)}; preferred={preferred}, reflected={reflected}"
    )


def _connect_property(
    source: object, outputs: Iterable[str], material_property: object
) -> str:
    for output in outputs:
        if unreal.MaterialEditingLibrary.connect_material_property(
            source, output, material_property
        ):
            return output
    raise RuntimeError(
        f"could not connect {_asset_object_path(source)} to material property {material_property}"
    )


def _scalar_parameter(
    material: object, name: str, default: float, x: int, y: int
) -> object:
    expression = _node(material, unreal.MaterialExpressionScalarParameter, x, y)
    _set_required(expression, "parameter_name", name)
    _set_required(expression, "default_value", float(default))
    return expression


def _input_nodes(material: object, expression: object) -> list[object]:
    return [
        node
        for node in unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
            material, expression
        )
        if node is not None
    ]


def _material_input_bindings(
    material: object, expression: object
) -> list[tuple[str, object | None]]:
    names = unreal.MaterialEditingLibrary.get_material_expression_input_names(
        expression
    )
    sources = unreal.MaterialEditingLibrary.get_inputs_for_material_expression(
        material, expression
    )
    return material_input_bindings(names, sources)


def _expression_class(expression: object) -> str:
    return str(expression.get_class().get_name())


def _parameter_name(expression: object) -> str:
    return str(expression.get_editor_property("parameter_name"))


def _material_property_connection(
    material: object, material_property: object
) -> tuple[object | None, str]:
    node = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material, material_property
    )
    if node is None:
        return None, ""
    output_name = unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(
        material, material_property
    )
    return node, "" if output_name is None else str(output_name)


def _validate_material_asset(material: object) -> dict[str, Any]:
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"{MATERIAL_PACKAGE} exists but is not a Material")
    _require_property(
        material, "material_domain", unreal.MaterialDomain.MD_UI, MATERIAL_PACKAGE
    )
    _require_property(
        material, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT, MATERIAL_PACKAGE
    )

    expressions = list(unreal.MaterialEditingLibrary.get_material_expressions(material))
    class_names = sorted(_expression_class(expression) for expression in expressions)
    expected_classes = sorted(
        [
            "MaterialExpressionTextureCoordinate",
            "MaterialExpressionScalarParameter",
            "MaterialExpressionScalarParameter",
            "MaterialExpressionAppendVector",
            "MaterialExpressionConstant",
            "MaterialExpressionDivide",
            "MaterialExpressionDivide",
            "MaterialExpressionAdd",
            "MaterialExpressionTextureSampleParameter2D",
        ]
    )
    if class_names != expected_classes:
        raise RuntimeError(
            f"{MATERIAL_PACKAGE} has wrong expression classes: "
            f"expected {expected_classes}, got {class_names}"
        )

    scalars = {
        _parameter_name(expression): expression
        for expression in expressions
        if _expression_class(expression) == "MaterialExpressionScalarParameter"
    }
    if set(scalars) != {"FrameColumn", "FrameRow"}:
        raise RuntimeError(
            f"{MATERIAL_PACKAGE} has wrong scalar parameters: {sorted(scalars)}"
        )
    scalar_defaults = {
        name: float(expression.get_editor_property("default_value"))
        for name, expression in scalars.items()
    }
    if scalar_defaults != {"FrameColumn": 0.0, "FrameRow": 0.0}:
        raise RuntimeError(
            f"{MATERIAL_PACKAGE} has wrong scalar defaults: {scalar_defaults}"
        )

    samples = [
        expression
        for expression in expressions
        if _expression_class(expression) == "MaterialExpressionTextureSampleParameter2D"
    ]
    sample = samples[0]
    if _parameter_name(sample) != "AtlasTexture":
        raise RuntimeError(
            f"{MATERIAL_PACKAGE} has wrong texture parameter: {_parameter_name(sample)}"
        )
    supported, default_texture = _get_if_supported(sample, "texture")
    if not supported or default_texture is None:
        raise RuntimeError(f"{MATERIAL_PACKAGE} texture parameter has no safe default")
    default_texture_path = _asset_object_path(default_texture)
    if not is_allowed_default_texture(default_texture_path):
        raise RuntimeError(
            f"{MATERIAL_PACKAGE} texture parameter has wrong default: "
            f"expected {ENGINE_DEFAULT_TEXTURE}, got {default_texture_path}"
        )

    constants = [
        expression
        for expression in expressions
        if _expression_class(expression) == "MaterialExpressionConstant"
    ]
    if len(constants) != 1 or float(constants[0].get_editor_property("r")) != 8.0:
        raise RuntimeError(f"{MATERIAL_PACKAGE} must use one constant divisor of 8")

    append_nodes = [
        expression
        for expression in expressions
        if _expression_class(expression) == "MaterialExpressionAppendVector"
    ]
    append_inputs = _input_nodes(material, append_nodes[0])
    if [_parameter_name(node) for node in append_inputs] != ["FrameColumn", "FrameRow"]:
        raise RuntimeError(
            f"{MATERIAL_PACKAGE} frame append order is wrong: "
            f"{[_parameter_name(node) for node in append_inputs]}"
        )

    divide_nodes = [
        expression
        for expression in expressions
        if _expression_class(expression) == "MaterialExpressionDivide"
    ]
    divide_topologies = sorted(
        tuple(_expression_class(node) for node in _input_nodes(material, divide))
        for divide in divide_nodes
    )
    expected_divide_topologies = sorted(
        [
            ("MaterialExpressionTextureCoordinate", "MaterialExpressionConstant"),
            ("MaterialExpressionAppendVector", "MaterialExpressionConstant"),
        ]
    )
    if divide_topologies != expected_divide_topologies:
        raise RuntimeError(
            f"{MATERIAL_PACKAGE} divide topology is wrong: {divide_topologies}"
        )

    add_nodes = [
        expression
        for expression in expressions
        if _expression_class(expression) == "MaterialExpressionAdd"
    ]
    add_inputs = _input_nodes(material, add_nodes[0])
    if [_expression_class(node) for node in add_inputs] != [
        "MaterialExpressionDivide",
        "MaterialExpressionDivide",
    ]:
        raise RuntimeError(f"{MATERIAL_PACKAGE} UV add topology is wrong")
    sample_bindings = _material_input_bindings(material, sample)
    connected_sample_inputs = [
        (input_name, node)
        for input_name, node in sample_bindings
        if node is not None
    ]
    if (
        len(connected_sample_inputs) != 1
        or _normalized_input_name(connected_sample_inputs[0][0])
        not in {"uvs", "coordinates"}
        or _expression_class(connected_sample_inputs[0][1])
        != "MaterialExpressionAdd"
    ):
        observed = [
            (input_name, _expression_class(node))
            for input_name, node in connected_sample_inputs
        ]
        raise RuntimeError(
            f"{MATERIAL_PACKAGE} texture UV pin is wrong: {observed}"
        )

    emissive_node, emissive_output = _material_property_connection(
        material, unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    opacity_node, opacity_output = _material_property_connection(
        material, unreal.MaterialProperty.MP_OPACITY
    )
    if emissive_node is not sample or emissive_output not in ("RGB", ""):
        raise RuntimeError(
            f"{MATERIAL_PACKAGE} emissive output is wrong: {emissive_output}"
        )
    if opacity_node is not sample or opacity_output not in ("A", "Alpha"):
        raise RuntimeError(
            f"{MATERIAL_PACKAGE} opacity output is wrong: {opacity_output}"
        )

    return {
        "package": MATERIAL_PACKAGE,
        "object_path": _asset_object_path(material),
        "class": material.get_class().get_name(),
        "policy": MATERIAL_POLICY,
        "expression_count": len(expressions),
        "expression_classes": class_names,
        "default_texture": default_texture_path,
        "formula": "TexCoord / 8 + Append(FrameColumn, FrameRow) / 8",
        "emissive_output": emissive_output,
        "opacity_output": opacity_output,
    }


def _create_new_material() -> tuple[object, dict[str, Any]]:
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_PACKAGE):
        raise RuntimeError(f"refusing to create over a newly appeared target: {MATERIAL_PACKAGE}")
    _ensure_directory(MATERIAL_DESTINATION)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MATERIAL_NAME,
        MATERIAL_DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"failed to create UI material: {MATERIAL_PACKAGE}")

    _set_required(material, "material_domain", unreal.MaterialDomain.MD_UI)
    _set_required(material, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    texcoord = _node(
        material, unreal.MaterialExpressionTextureCoordinate, -1100, -220
    )
    frame_column = _scalar_parameter(material, "FrameColumn", 0.0, -1100, 80)
    frame_row = _scalar_parameter(material, "FrameRow", 0.0, -1100, 220)
    frame_pair = _node(
        material, unreal.MaterialExpressionAppendVector, -820, 140
    )
    _connect(frame_column, ("",), frame_pair, ("A",))
    _connect(frame_row, ("",), frame_pair, ("B",))

    divisor = _node(material, unreal.MaterialExpressionConstant, -820, 360)
    _set_required(divisor, "r", 8.0)
    scaled_uv = _node(material, unreal.MaterialExpressionDivide, -560, -180)
    _connect(texcoord, ("",), scaled_uv, ("A",))
    _connect(divisor, ("",), scaled_uv, ("B",))
    frame_offset = _node(material, unreal.MaterialExpressionDivide, -560, 120)
    _connect(frame_pair, ("",), frame_offset, ("A",))
    _connect(divisor, ("",), frame_offset, ("B",))
    atlas_uv = _node(material, unreal.MaterialExpressionAdd, -260, -80)
    _connect(scaled_uv, ("",), atlas_uv, ("A",))
    _connect(frame_offset, ("",), atlas_uv, ("B",))

    sample = _node(
        material, unreal.MaterialExpressionTextureSampleParameter2D, 30, -80
    )
    _set_required(sample, "parameter_name", "AtlasTexture")
    default_texture = unreal.EditorAssetLibrary.load_asset(ENGINE_DEFAULT_TEXTURE)
    if not isinstance(default_texture, unreal.Texture2D):
        raise RuntimeError(
            f"engine default Texture2D is unavailable: {ENGINE_DEFAULT_TEXTURE}"
        )
    _set_required(sample, "texture", default_texture)
    uv_input = _connect(atlas_uv, ("",), sample, ("UVs", "Coordinates"))
    if _normalized_input_name(uv_input) not in {"uvs", "coordinates"}:
        raise RuntimeError(f"texture atlas UV connected to wrong input pin: {uv_input}")
    _connect_property(
        sample, ("RGB", ""), unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    _connect_property(sample, ("A", "Alpha"), unreal.MaterialProperty.MP_OPACITY)

    compile_errors = [
        str(value)
        for value in unreal.MaterialEditingLibrary.recompile_material(material) or []
    ]
    if compile_errors:
        raise RuntimeError(f"UI atlas material compilation failed: {compile_errors}")
    observation = _validate_material_asset(material)
    _save_material(material)
    return material, observation


def _save_material(material: object) -> None:
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError(f"failed to save generated material: {MATERIAL_PACKAGE}")


def _preflight_assets() -> tuple[
    dict[str, str], dict[str, dict[str, Any]], dict[str, object]
]:
    actions: dict[str, str] = {}
    observations: dict[str, dict[str, Any]] = {}
    loaded: dict[str, object] = {}
    for package in (BACKDROP_PACKAGE, MATERIAL_PACKAGE):
        asset_exists = bool(unreal.EditorAssetLibrary.does_asset_exist(package))
        action = classify_target_action(_package_file(package).is_file(), asset_exists)
        actions[package] = action
        if action != "create":
            asset = unreal.EditorAssetLibrary.load_asset(package)
            if asset is None:
                raise RuntimeError(f"could not load persisted target package: {package}")
            loaded[package] = asset

    if BACKDROP_PACKAGE in loaded:
        observations[BACKDROP_PACKAGE] = _validate_backdrop_asset(
            loaded[BACKDROP_PACKAGE]
        )
    if MATERIAL_PACKAGE in loaded:
        observations[MATERIAL_PACKAGE] = _validate_material_asset(
            loaded[MATERIAL_PACKAGE]
        )
    return actions, observations, loaded


def execute_import() -> dict[str, Any]:
    _require_unreal()
    source = validate_source_image()
    actions, observations, loaded = _preflight_assets()
    plan = build_asset_plan(
        actions[BACKDROP_PACKAGE] == "validate",
        actions[MATERIAL_PACKAGE] == "validate",
    )
    created: list[str] = []
    validated = list(plan["validate"])

    if actions[BACKDROP_PACKAGE] == "create":
        _, observations[BACKDROP_PACKAGE] = _import_new_backdrop()
        created.append(BACKDROP_PACKAGE)
    elif actions[BACKDROP_PACKAGE] == "recover":
        _save_backdrop(loaded[BACKDROP_PACKAGE])
        created.append(BACKDROP_PACKAGE)
    if actions[MATERIAL_PACKAGE] == "create":
        _, observations[MATERIAL_PACKAGE] = _create_new_material()
        created.append(MATERIAL_PACKAGE)
    elif actions[MATERIAL_PACKAGE] == "recover":
        _save_material(loaded[MATERIAL_PACKAGE])
        created.append(MATERIAL_PACKAGE)

    return {
        "ok": True,
        "mode": "execute-import",
        "execute_import": True,
        "source": source,
        "paths": {
            "source": str(SOURCE_IMAGE),
            "backdrop": BACKDROP_PACKAGE,
            "material": MATERIAL_PACKAGE,
        },
        "plan": plan,
        "created": created,
        "validated": validated,
        "assets": {
            "backdrop": observations[BACKDROP_PACKAGE],
            "material": observations[MATERIAL_PACKAGE],
        },
        **_dirty_packages_after_save(),
    }


def main(argv: list[str] | None = None) -> int:
    args = parse_arguments(argv)
    if not args.execute_import:
        source = validate_source_image()
        report = {
            "ok": True,
            "mode": "plan-only",
            "execute_import": False,
            "source": source,
            "paths": {
                "source": str(SOURCE_IMAGE),
                "backdrop": BACKDROP_PACKAGE,
                "material": MATERIAL_PACKAGE,
            },
            "policy": {
                "texture": TEXTURE_POLICY,
                "material": MATERIAL_POLICY,
            },
        }
    else:
        report = execute_import()
    print(json.dumps(report, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    main()
