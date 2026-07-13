"""Author the post-process material for a circular unobstructed scene reveal."""

from __future__ import annotations

import json
import unreal


ASSET_PATH = "/Game/GameXXK/Materials/Player/M_PP_PlayerOcclusionReveal"
ASSET_NAME = "M_PP_PlayerOcclusionReveal"
ASSET_FOLDER = "/Game/GameXXK/Materials/Player"


def _set(obj, name, value):
    obj.set_editor_property(name, value)


def _node(material, cls, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)


def _connect(source, outputs, destination, inputs):
    for output in outputs:
        for input_name in inputs:
            if unreal.MaterialEditingLibrary.connect_material_expressions(source, output, destination, input_name):
                return
    reflected = [str(v) for v in unreal.MaterialEditingLibrary.get_material_expression_input_names(destination)]
    raise RuntimeError(f"connection failed inputs={inputs} reflected={reflected}")


def _property(source, outputs, material_property):
    for output in outputs:
        if unreal.MaterialEditingLibrary.connect_material_property(source, output, material_property):
            return
    raise RuntimeError(f"property connection failed: {material_property}")


def _load_or_create():
    material = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    if material:
        return material, False
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ASSET_NAME, ASSET_FOLDER, unreal.Material, unreal.MaterialFactoryNew()
    )
    if not material:
        raise RuntimeError(f"failed to create {ASSET_PATH}")
    return material, True


def author():
    material, created = _load_or_create()
    for expression in reversed(list(unreal.MaterialEditingLibrary.get_material_expressions(material))):
        unreal.MaterialEditingLibrary.delete_material_expression(material, expression)
    _set(material, "material_domain", unreal.MaterialDomain.MD_POST_PROCESS)

    main_scene = _node(material, unreal.MaterialExpressionSceneTexture, -900, -220)
    _set(main_scene, "scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    main_rgb = _node(material, unreal.MaterialExpressionComponentMask, -680, -260)
    _set(main_rgb, "r", True)
    _set(main_rgb, "g", True)
    _set(main_rgb, "b", True)
    _set(main_rgb, "a", False)
    _connect(main_scene, ("Color", ""), main_rgb, ("Input", ""))
    screen_uv = _node(material, unreal.MaterialExpressionScreenPosition, -1100, 180)
    capture = _node(material, unreal.MaterialExpressionTextureSampleParameter2D, -650, -40)
    _set(capture, "parameter_name", "RevealSceneTexture")
    _connect(screen_uv, ("ViewportUV", ""), capture, ("UVs", "Coordinates"))

    center = _node(material, unreal.MaterialExpressionVectorParameter, -900, 360)
    _set(center, "parameter_name", "RevealCenter")
    _set(center, "default_value", unreal.LinearColor(0.5, 0.5, 0.0, 0.0))
    center_rg = _node(material, unreal.MaterialExpressionComponentMask, -720, 420)
    _set(center_rg, "r", True)
    _set(center_rg, "g", True)
    _set(center_rg, "b", False)
    _set(center_rg, "a", False)
    _connect(center, ("", "RGB"), center_rg, ("Input", ""))
    delta = _node(material, unreal.MaterialExpressionSubtract, -650, 310)
    _connect(screen_uv, ("ViewportUV", ""), delta, ("A",))
    _connect(center_rg, ("",), delta, ("B",))
    aspect = _node(material, unreal.MaterialExpressionVectorParameter, -650, 540)
    _set(aspect, "parameter_name", "RevealAspectScale")
    _set(aspect, "default_value", unreal.LinearColor(1.777778, 1.0, 0.0, 0.0))
    aspect_rg = _node(material, unreal.MaterialExpressionComponentMask, -440, 540)
    _set(aspect_rg, "r", True)
    _set(aspect_rg, "g", True)
    _set(aspect_rg, "b", False)
    _set(aspect_rg, "a", False)
    _connect(aspect, ("", "RGB"), aspect_rg, ("Input", ""))
    corrected_delta = _node(material, unreal.MaterialExpressionMultiply, -430, 310)
    _connect(delta, ("",), corrected_delta, ("A",))
    _connect(aspect_rg, ("",), corrected_delta, ("B",))
    dot = _node(material, unreal.MaterialExpressionDotProduct, -430, 310)
    _connect(corrected_delta, ("",), dot, ("A",))
    _connect(corrected_delta, ("",), dot, ("B",))
    distance = _node(material, unreal.MaterialExpressionSquareRoot, -220, 310)
    _connect(dot, ("",), distance, ("Input", ""))

    radius = _node(material, unreal.MaterialExpressionScalarParameter, -220, 470)
    _set(radius, "parameter_name", "RevealRadius")
    _set(radius, "default_value", 0.18)
    feather = _node(material, unreal.MaterialExpressionScalarParameter, -220, 570)
    _set(feather, "parameter_name", "RevealFeather")
    _set(feather, "default_value", 0.025)
    active = _node(material, unreal.MaterialExpressionScalarParameter, 30, 600)
    _set(active, "parameter_name", "RevealActive")
    _set(active, "default_value", 0.0)
    numerator = _node(material, unreal.MaterialExpressionSubtract, 20, 330)
    _connect(radius, ("",), numerator, ("A",))
    _connect(distance, ("",), numerator, ("B",))
    normalized = _node(material, unreal.MaterialExpressionDivide, 230, 350)
    _connect(numerator, ("",), normalized, ("A",))
    _connect(feather, ("",), normalized, ("B",))
    mask = _node(material, unreal.MaterialExpressionSaturate, 440, 350)
    _connect(normalized, ("",), mask, ("Input", ""))
    enabled_mask = _node(material, unreal.MaterialExpressionMultiply, 630, 350)
    _connect(mask, ("",), enabled_mask, ("A",))
    _connect(active, ("",), enabled_mask, ("B",))

    result = _node(material, unreal.MaterialExpressionLinearInterpolate, 840, 20)
    _connect(main_rgb, ("",), result, ("A",))
    _connect(capture, ("RGB", ""), result, ("B",))
    _connect(enabled_mask, ("",), result, ("Alpha",))
    _property(result, ("", "RGB"), unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    errors = [str(item) for item in unreal.MaterialEditingLibrary.recompile_material(material)]
    if errors:
        raise RuntimeError(f"material compile failed: {errors}")
    material.modify()
    if not unreal.EditorAssetLibrary.save_asset(ASSET_PATH, only_if_is_dirty=False):
        raise RuntimeError(f"failed to save {ASSET_PATH}")
    observed = {
        "asset_path": ASSET_PATH,
        "created": created,
        "domain": str(material.get_editor_property("material_domain")),
        "expression_count": int(unreal.MaterialEditingLibrary.get_num_material_expressions(material)),
    }
    print(json.dumps(observed, ensure_ascii=False))
    return observed


if __name__ == "__main__":
    author()
