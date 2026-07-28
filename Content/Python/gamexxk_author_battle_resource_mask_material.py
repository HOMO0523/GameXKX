"""Author the UI material that masks PSD Track/Full resource bars by percentage.

The material is deliberately generic.  Runtime creates one MID for each HP/MP
row and supplies the two source textures plus the exact inner-channel bounds.
The Track remains visible everywhere; Full replaces it only inside the channel
from its left edge through ``FillPercent``.
"""

from __future__ import annotations

import json

import unreal


DESTINATION = "/Game/GameXXK/UI/Battle/ResourceBars"
MATERIAL_NAME = "M_BattlePsdResourceMask"
MATERIAL_PATH = f"{DESTINATION}/{MATERIAL_NAME}"


def _set(obj, property_name: str, value: object) -> None:
    try:
        obj.set_editor_property(property_name, value)
    except (AttributeError, RuntimeError) as exc:
        raise RuntimeError(f"Could not set {property_name} on {obj}: {exc}") from exc


def _node(material, node_class, x: int, y: int):
    return unreal.MaterialEditingLibrary.create_material_expression(material, node_class, x, y)


def _connect(source, source_output: str, destination, destination_input: str) -> None:
    candidates = tuple(dict.fromkeys((destination_input, "", "None")))
    for candidate in candidates:
        if unreal.MaterialEditingLibrary.connect_material_expressions(
            source, source_output, destination, candidate
        ):
            return
    available = unreal.MaterialEditingLibrary.get_material_expression_input_names(destination)
    raise RuntimeError(
        f"Could not connect {source.get_name()}[{source_output}] to "
        f"{destination.get_name()} inputs={candidates}; available={available}"
    )


def _property(source, source_output: str, property_id) -> None:
    if not unreal.MaterialEditingLibrary.connect_material_property(source, source_output, property_id):
        raise RuntimeError(f"Could not connect {source.get_name()}[{source_output}] to {property_id}")


def _scalar_parameter(material, name: str, default_value: float, x: int, y: int):
    expression = _node(material, unreal.MaterialExpressionScalarParameter, x, y)
    _set(expression, "parameter_name", name)
    _set(expression, "default_value", default_value)
    return expression


def _constant(material, value: float, x: int, y: int):
    expression = _node(material, unreal.MaterialExpressionConstant, x, y)
    _set(expression, "r", value)
    return expression


def _comparison_mask(material, a, b, *, true_when_greater: bool, x: int, y: int, one, zero):
    """Build ``a >= b`` or ``a <= b`` as a hard material mask."""
    expression = _node(material, unreal.MaterialExpressionIf, x, y)
    _connect(a, "", expression, "A")
    _connect(b, "", expression, "B")
    if true_when_greater:
        _connect(one, "", expression, "A > B")
        _connect(one, "", expression, "A == B")
        _connect(zero, "", expression, "A < B")
    else:
        _connect(zero, "", expression, "A > B")
        _connect(one, "", expression, "A == B")
        _connect(one, "", expression, "A < B")
    return expression


def _multiply(material, a, b, x: int, y: int):
    expression = _node(material, unreal.MaterialExpressionMultiply, x, y)
    _connect(a, "", expression, "A")
    _connect(b, "", expression, "B")
    return expression


def _create_or_load_material():
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        unreal.EditorAssetLibrary.make_directory(DESTINATION)
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    if material:
        if not isinstance(material, unreal.Material):
            raise RuntimeError(f"{MATERIAL_PATH} already exists but is not a Material")
        return material
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        MATERIAL_NAME,
        DESTINATION,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"Failed to create UI material {MATERIAL_PATH}")
    return material


def main() -> None:
    material = _create_or_load_material()
    _set(material, "material_domain", unreal.MaterialDomain.MD_UI)
    _set(material, "blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    _set(material, "two_sided", True)

    for expression in unreal.MaterialEditingLibrary.get_material_expressions(material):
        unreal.MaterialEditingLibrary.delete_material_expression(material, expression)

    track = _node(material, unreal.MaterialExpressionTextureSampleParameter2D, -1250, -180)
    _set(track, "parameter_name", "TrackTexture")
    full = _node(material, unreal.MaterialExpressionTextureSampleParameter2D, -1250, 40)
    _set(full, "parameter_name", "FullTexture")
    coordinate = _node(material, unreal.MaterialExpressionTextureCoordinate, -1250, 260)

    fill_percent = _scalar_parameter(material, "FillPercent", 1.0, -1050, 440)
    fill_left = _scalar_parameter(material, "FillLeft", 0.0, -1050, 520)
    fill_right = _scalar_parameter(material, "FillRight", 1.0, -1050, 600)
    fill_top = _scalar_parameter(material, "FillTop", 0.0, -1050, 680)
    fill_bottom = _scalar_parameter(material, "FillBottom", 1.0, -1050, 760)

    u_coord = _node(material, unreal.MaterialExpressionComponentMask, -1030, 260)
    _set(u_coord, "r", True)
    _connect(coordinate, "", u_coord, "Input")
    v_coord = _node(material, unreal.MaterialExpressionComponentMask, -1030, 340)
    _set(v_coord, "g", True)
    _connect(coordinate, "", v_coord, "Input")

    current_right = _node(material, unreal.MaterialExpressionLinearInterpolate, -780, 520)
    _connect(fill_left, "", current_right, "A")
    _connect(fill_right, "", current_right, "B")
    _connect(fill_percent, "", current_right, "Alpha")

    one = _constant(material, 1.0, -760, 700)
    zero = _constant(material, 0.0, -760, 780)
    after_left = _comparison_mask(material, u_coord, fill_left, true_when_greater=True, x=-530, y=250, one=one, zero=zero)
    before_current_right = _comparison_mask(material, u_coord, current_right, true_when_greater=False, x=-530, y=350, one=one, zero=zero)
    after_top = _comparison_mask(material, v_coord, fill_top, true_when_greater=True, x=-530, y=450, one=one, zero=zero)
    before_bottom = _comparison_mask(material, v_coord, fill_bottom, true_when_greater=False, x=-530, y=550, one=one, zero=zero)

    horizontal = _multiply(material, after_left, before_current_right, -290, 300)
    vertical = _multiply(material, after_top, before_bottom, -290, 500)
    mask = _multiply(material, horizontal, vertical, -80, 390)

    final_color = _node(material, unreal.MaterialExpressionLinearInterpolate, 160, -50)
    _connect(track, "RGB", final_color, "A")
    _connect(full, "RGB", final_color, "B")
    _connect(mask, "", final_color, "Alpha")

    _property(final_color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    _property(track, "A", unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)

    print(json.dumps({
        "ok": True,
        "material": material.get_path_name(),
        "parameters": ["TrackTexture", "FullTexture", "FillPercent", "FillLeft", "FillRight", "FillTop", "FillBottom"],
    }, ensure_ascii=False))


if __name__ == "__main__":
    main()
