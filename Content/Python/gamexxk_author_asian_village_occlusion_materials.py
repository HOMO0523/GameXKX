"""Author blend-safe project-owned Asian Village occlusion material families."""

from __future__ import annotations

import json
from pathlib import Path

import unreal

import gamexxk_occlusion_material_naming as naming
from gamexxk_occlusion_material_naming import cutout_object_path


DESTINATION = "/Game/GameXXK/Materials/Occlusion/AsianVillageFull"
if DESTINATION != naming.DESTINATION_PATH:
    raise RuntimeError("occlusion naming destination contract drifted")
MPC_PATH = "/Game/GameXXK/Materials/Occlusion/MPC_PlayerOcclusion"
INVENTORY_PATH = (
    Path(unreal.Paths.project_dir())
    / "Config/GameXXK/Occlusion/AsianVillageMaterialInventory.json"
)
SUPPORTED_BLENDS = {
    "BLEND_OPAQUE": unreal.BlendMode.BLEND_OPAQUE,
    "BLEND_MASKED": unreal.BlendMode.BLEND_MASKED,
    "BLEND_TRANSLUCENT": unreal.BlendMode.BLEND_TRANSLUCENT,
}


def _set(obj, name, value):
    obj.set_editor_property(name, value)


def _node(material, cls, x, y):
    result = unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)
    if result is None:
        raise RuntimeError(f"failed to create {cls} in {material.get_path_name()}")
    return result


def _connect(source, output, destination, input_name, exact_output=False):
    outputs = (output,) if exact_output or output == "" else (output, "")
    inputs = (input_name,) if input_name == "" else (input_name, "")
    for candidate_output in outputs:
        for candidate_input in inputs:
            if unreal.MaterialEditingLibrary.connect_material_expressions(
                source, candidate_output, destination, candidate_input
            ):
                return
    reflected = [
        str(value)
        for value in unreal.MaterialEditingLibrary.get_material_expression_input_names(
            destination
        )
    ]
    raise RuntimeError(
        f"connection failed: {source.get_name()}[{output!r}] -> "
        f"{destination.get_name()}[{input_name!r}], inputs={reflected}"
    )


def _connect_property(source, output, material_property):
    if not unreal.MaterialEditingLibrary.connect_material_property(
        source, output, material_property
    ):
        raise RuntimeError(
            f"material-property connection failed: {source.get_name()}[{output!r}] "
            f"-> {material_property}"
        )


def _collection_parameter(material, collection, parameter_name, x, y):
    expression = _node(material, unreal.MaterialExpressionCollectionParameter, x, y)
    _set(expression, "collection", collection)
    _set(expression, "parameter_name", parameter_name)
    return expression


def _circle_keep_mask(material, collection):
    screen = _node(material, unreal.MaterialExpressionScreenPosition, -1250, 650)
    center = _collection_parameter(material, collection, "OcclusionCenter", -1250, 790)
    center_rg = _node(material, unreal.MaterialExpressionComponentMask, -1040, 790)
    for channel, enabled in (("r", True), ("g", True), ("b", False), ("a", False)):
        _set(center_rg, channel, enabled)
    _connect(center, "RGB", center_rg, "Input")

    delta = _node(material, unreal.MaterialExpressionSubtract, -1000, 650)
    _connect(screen, "ViewportUV", delta, "A")
    _connect(center_rg, "", delta, "B")

    aspect = _collection_parameter(material, collection, "OcclusionAspect", -1000, 930)
    aspect_rg = _node(material, unreal.MaterialExpressionComponentMask, -790, 930)
    for channel, enabled in (("r", True), ("g", True), ("b", False), ("a", False)):
        _set(aspect_rg, channel, enabled)
    _connect(aspect, "RGB", aspect_rg, "Input")
    corrected = _node(material, unreal.MaterialExpressionMultiply, -750, 650)
    _connect(delta, "", corrected, "A")
    _connect(aspect_rg, "", corrected, "B")

    dot = _node(material, unreal.MaterialExpressionDotProduct, -520, 650)
    _connect(corrected, "", dot, "A")
    _connect(corrected, "", dot, "B")
    distance = _node(material, unreal.MaterialExpressionSquareRoot, -320, 650)
    _connect(dot, "", distance, "Input")
    radius = _collection_parameter(material, collection, "OcclusionRadius", -320, 830)
    edge = _node(material, unreal.MaterialExpressionSubtract, -100, 690)
    _connect(distance, "", edge, "A")
    _connect(radius, "", edge, "B")
    feather = _node(material, unreal.MaterialExpressionConstant, -100, 850)
    _set(feather, "r", 0.02)
    normalized = _node(material, unreal.MaterialExpressionDivide, 120, 710)
    _connect(edge, "", normalized, "A")
    _connect(feather, "", normalized, "B")
    keep_mask = _node(material, unreal.MaterialExpressionSaturate, 340, 710)
    _connect(normalized, "", keep_mask, "Input")
    return keep_mask


def _existing_property_connection(material, material_property):
    node = unreal.MaterialEditingLibrary.get_material_property_input_node(
        material, material_property
    )
    if node is None:
        return None
    output_name = unreal.MaterialEditingLibrary.get_material_property_input_node_output_name(
        material, material_property
    )
    if output_name is None:
        raise RuntimeError(
            f"connected {material_property} has no reflected output name in "
            f"{material.get_path_name()}; refusing to overwrite it"
        )
    output_name = str(output_name)
    output_names = [
        str(value)
        for value in unreal.MaterialEditingLibrary.get_material_expression_output_names(node)
    ]
    if output_name not in output_names and not (output_name == "" and output_names):
        raise RuntimeError(
            f"connected {material_property} output {output_name!r} is not among "
            f"{output_names} for {node.get_name()}; refusing to overwrite it"
        )
    return node, output_name


def _patch_root(material, source_blend, collection):
    blend_name = str(source_blend).split(".")[-1].split(":", 1)[0]
    if source_blend == unreal.BlendMode.BLEND_OPAQUE:
        material_property = unreal.MaterialProperty.MP_OPACITY_MASK
        target_blend = unreal.BlendMode.BLEND_MASKED
        force_masked = True
    elif source_blend == unreal.BlendMode.BLEND_MASKED:
        material_property = unreal.MaterialProperty.MP_OPACITY_MASK
        target_blend = unreal.BlendMode.BLEND_MASKED
        force_masked = True
    elif source_blend == unreal.BlendMode.BLEND_TRANSLUCENT:
        material_property = unreal.MaterialProperty.MP_OPACITY
        target_blend = unreal.BlendMode.BLEND_TRANSLUCENT
        force_masked = False
    else:
        raise RuntimeError(
            f"unsupported root blend mode {blend_name}: {material.get_path_name()}"
        )

    existing = _existing_property_connection(material, material_property)
    circle = _circle_keep_mask(material, collection)
    final_node = circle
    if existing is not None:
        original_node, output_name = existing
        multiply = _node(material, unreal.MaterialExpressionMultiply, 580, 650)
        _connect(original_node, output_name, multiply, "A", exact_output=True)
        _connect(circle, "", multiply, "B")
        final_node = multiply
    _connect_property(final_node, "", material_property)
    _set(material, "blend_mode", target_blend)
    material.modify()
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    if force_masked:
        if not unreal.GameXXKMaterialAuthoringLibrary.force_masked_material_compilation(
            material
        ):
            raise RuntimeError(
                f"force_masked compilation failed for {material.get_path_name()}"
            )
    errors = [str(item) for item in unreal.MaterialEditingLibrary.recompile_material(material)]
    if errors:
        raise RuntimeError(f"recompile failed for {material.get_path_name()}: {errors}")


def _package_path(object_path):
    return str(object_path).split(".", 1)[0]


def _duplicate_family(source, collection, cache, created, roots):
    if source is None:
        raise RuntimeError("cannot duplicate a missing source asset")
    source_path = str(source.get_path_name())
    if source_path in cache:
        return cache[source_path]
    target_object_path = cutout_object_path(source_path)
    destination = _package_path(target_object_path)
    if not destination.startswith(DESTINATION + "/"):
        raise RuntimeError(f"refusing non-project destination {destination}")
    if source_path.startswith(DESTINATION + "/"):
        raise RuntimeError(f"refusing to use generated asset as source: {source_path}")
    if unreal.EditorAssetLibrary.does_asset_exist(destination):
        if not unreal.EditorAssetLibrary.delete_asset(destination):
            raise RuntimeError(f"failed to replace project-owned destination {destination}")
    duplicate = unreal.EditorAssetLibrary.duplicate_asset(source_path, destination)
    if duplicate is None:
        raise RuntimeError(f"failed to duplicate {source_path} -> {destination}")
    cache[source_path] = duplicate
    created.append(destination)

    if isinstance(source, unreal.MaterialInstanceConstant):
        parent = source.get_editor_property("parent")
        if parent is None:
            raise RuntimeError(f"material instance has no parent: {source_path}")
        copied_parent = _duplicate_family(parent, collection, cache, created, roots)
        unreal.MaterialEditingLibrary.set_material_instance_parent(duplicate, copied_parent)
        duplicate.modify()
    elif isinstance(source, unreal.Material):
        _patch_root(duplicate, source.get_blend_mode(), collection)
        roots.append(destination)
    else:
        raise RuntimeError(
            f"unsupported material ancestry asset {source_path}: "
            f"{source.get_class().get_name()}"
        )
    if not unreal.EditorAssetLibrary.save_asset(destination, only_if_is_dirty=False):
        raise RuntimeError(f"failed to save_asset {destination}")
    return duplicate


def author():
    inventory = json.loads(INVENTORY_PATH.read_text(encoding="utf-8"))
    entries = list(inventory["materials"])
    eligible = [entry for entry in entries if not entry["excluded"]]
    excluded = [entry for entry in entries if entry["excluded"]]
    if len(eligible) != 74 or len(excluded) != 6:
        raise RuntimeError(
            f"inventory count mismatch: eligible={len(eligible)} excluded={len(excluded)}"
        )
    for entry in entries:
        if entry["blend_mode"] not in SUPPORTED_BLENDS:
            raise RuntimeError(
                f"unsupported inventory blend {entry['blend_mode']}: {entry['source_path']}"
            )
        expected = cutout_object_path(entry["source_path"])
        if entry["target_path"] != expected:
            raise RuntimeError(
                f"inventory target mismatch: {entry['target_path']} != {expected}"
            )

    collection = unreal.EditorAssetLibrary.load_asset(MPC_PATH)
    if collection is None:
        raise RuntimeError(f"required parameter collection is missing: {MPC_PATH}")
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        unreal.EditorAssetLibrary.make_directory(DESTINATION)

    created = []
    roots = []
    cache = {}
    failures = []
    mapping = {}
    for entry in eligible:
        source_path = entry["source_path"]
        try:
            source = unreal.EditorAssetLibrary.load_asset(source_path)
            copied = _duplicate_family(source, collection, cache, created, roots)
            mapping[source_path] = str(copied.get_path_name())
        except Exception as exc:
            failures.append({"source_path": source_path, "error": repr(exc)})
            unreal.log_error(
                f"AsianVillage occlusion authoring failure for {source_path}: {exc!r}"
            )
            break
    missing_targets = [
        entry["target_path"]
        for entry in eligible
        if not unreal.EditorAssetLibrary.does_asset_exist(_package_path(entry["target_path"]))
    ]
    counts = {
        name: sum(entry["blend_mode"] == name for entry in eligible)
        for name in SUPPORTED_BLENDS
    }
    report = {
        "eligible_count": len(eligible),
        "excluded_count": len(excluded),
        "created_count": len(created),
        "inventory_mapping_count": len(mapping),
        "ancestry_count": len(cache),
        "root_count": len(roots),
        "opaque_source_count": counts["BLEND_OPAQUE"],
        "masked_source_count": counts["BLEND_MASKED"],
        "translucent_source_count": counts["BLEND_TRANSLUCENT"],
        "missing_targets": missing_targets,
        "failures": failures,
    }
    print(json.dumps(report, ensure_ascii=False))
    if failures or missing_targets or len(mapping) != len(eligible):
        raise RuntimeError(f"authoring incomplete: {json.dumps(report, ensure_ascii=False)}")
    return report


if __name__ == "__main__":
    author()
