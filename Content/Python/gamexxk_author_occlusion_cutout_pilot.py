"""Author a project-owned masked cutout copy for the Asian Village thatched roof pilot."""

from __future__ import annotations

import json
import unreal


MAP = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
DEST_FOLDER = "/Game/GameXXK/Materials/Occlusion/AsianVillage"
MPC_FOLDER = "/Game/GameXXK/Materials/Occlusion"
MPC_PATH = f"{MPC_FOLDER}/MPC_PlayerOcclusion"
MPC_NAME = "MPC_PlayerOcclusion"
TARGET_LABELS = ("SM_thatched_roof_10", "SM_thatched_roof_12")
EXTRA_BLOCKER_MATERIALS = (
    "/Game/Asian_Village/materials/building_materials/base_materials/MI_bamboo_wall_01_Inst.MI_bamboo_wall_01_Inst",
    "/Game/Asian_Village/materials/building_materials/base_materials/MI_concrete_03_Inst.MI_concrete_03_Inst",
    "/Game/Asian_Village/materials/building_materials/base_materials/MI_glass_01_Inst.MI_glass_01_Inst",
    "/Game/Asian_Village/materials/building_materials/base_materials/MI_roof_02_Inst.MI_roof_02_Inst",
    "/Game/Asian_Village/materials/building_materials/base_materials/MI_stone_tiles_01_Inst.MI_stone_tiles_01_Inst",
    "/Game/Asian_Village/materials/building_materials/base_materials/MI_wallpaper_01_Inst.MI_wallpaper_01_Inst",
    "/Game/Asian_Village/materials/building_materials/base_materials/MI_wood_02_Inst.MI_wood_02_Inst",
    "/Game/Asian_Village/materials/building_materials/base_materials/MI_wooden_board_02_Inst.MI_wooden_board_02_Inst",
    "/Game/Asian_Village/materials/building_materials/base_materials/MI_wooden_board_WPO_01_Inst.MI_wooden_board_WPO_01_Inst",
    "/Game/Asian_Village/materials/building_materials/base_materials/MI_wooden_board_WPO_02_Inst.MI_wooden_board_WPO_02_Inst",
    "/Game/Asian_Village/materials/building_materials/base_materials/MI_wooden_board_WPO_03_Inst.MI_wooden_board_WPO_03_Inst",
    "/Game/Asian_Village/materials/building_materials/MI_building_concrete_WPO_01_Inst.MI_building_concrete_WPO_01_Inst",
    "/Game/Asian_Village/materials/building_materials/MI_building_metal_03_Inst.MI_building_metal_03_Inst",
    "/Game/Asian_Village/materials/building_materials/MI_building_wood_01_Inst.MI_building_wood_01_Inst",
)


def _set(obj, name, value):
    obj.set_editor_property(name, value)


def _node(material, cls, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(material, cls, x, y)


def _connect(source, outputs, destination, inputs):
    for output in outputs:
        for input_name in inputs:
            if unreal.MaterialEditingLibrary.connect_material_expressions(source, output, destination, input_name):
                return
    names = [str(v) for v in unreal.MaterialEditingLibrary.get_material_expression_input_names(destination)]
    raise RuntimeError(f"connection failed inputs={inputs} reflected={names}")


def _connect_property(source, outputs, prop):
    for output in outputs:
        if unreal.MaterialEditingLibrary.connect_material_property(source, output, prop):
            return
    raise RuntimeError(f"material property connection failed: {prop}")


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
            f"{material.get_path_name()}"
        )
    return node, str(output_name)


def _ensure_collection():
    collection = unreal.EditorAssetLibrary.load_asset(MPC_PATH)
    # MPC parameter GUIDs are referenced directly by material expression nodes.
    # Replacing these arrays on a later authoring pass would assign fresh GUIDs
    # and make already-generated Cutout materials fail to compile.
    if collection is not None:
        return collection
    if collection is None:
        collection = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            MPC_NAME,
            MPC_FOLDER,
            unreal.MaterialParameterCollection,
            unreal.MaterialParameterCollectionFactoryNew(),
        )
    if collection is None:
        raise RuntimeError(f"failed to create {MPC_PATH}")

    center = unreal.CollectionVectorParameter()
    _set(center, "parameter_name", "OcclusionCenter")
    _set(center, "default_value", unreal.LinearColor(0.5, 0.5, 0.0, 0.0))
    aspect = unreal.CollectionVectorParameter()
    _set(aspect, "parameter_name", "OcclusionAspect")
    _set(aspect, "default_value", unreal.LinearColor(1.777778, 1.0, 0.0, 0.0))
    camera_location = unreal.CollectionVectorParameter()
    _set(camera_location, "parameter_name", "OcclusionCameraLocation")
    _set(camera_location, "default_value", unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    camera_forward = unreal.CollectionVectorParameter()
    _set(camera_forward, "parameter_name", "OcclusionCameraForward")
    _set(camera_forward, "default_value", unreal.LinearColor(1.0, 0.0, 0.0, 0.0))
    radius = unreal.CollectionScalarParameter()
    _set(radius, "parameter_name", "OcclusionRadius")
    _set(radius, "default_value", 0.18)
    hero_view_depth = unreal.CollectionScalarParameter()
    _set(hero_view_depth, "parameter_name", "OcclusionHeroViewDepth")
    _set(hero_view_depth, "default_value", 0.0)
    depth_bias = unreal.CollectionScalarParameter()
    _set(depth_bias, "parameter_name", "OcclusionDepthBias")
    _set(depth_bias, "default_value", 5.0)
    hero_stencil_value = unreal.CollectionScalarParameter()
    _set(hero_stencil_value, "parameter_name", "OcclusionHeroStencilValue")
    _set(hero_stencil_value, "default_value", 13.0)
    _set(collection, "vector_parameters", [center, aspect, camera_location, camera_forward])
    _set(collection, "scalar_parameters", [radius, hero_view_depth, depth_bias, hero_stencil_value])
    collection.modify()
    if not unreal.EditorAssetLibrary.save_asset(MPC_PATH, only_if_is_dirty=False):
        raise RuntimeError(f"failed to save_asset {MPC_PATH}")
    return collection


def _current_map():
    world = unreal.EditorLevelLibrary.get_editor_world()
    return "" if world is None else str(world.get_outermost().get_path_name()).split(".", 1)[0]


def _inspect_target_materials():
    if _current_map() != MAP:
        if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP):
            raise RuntimeError(f"failed to load pilot map {MAP}")
    actors = {str(actor.get_actor_label()): actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()}
    for required_label in TARGET_LABELS:
        if required_label not in actors:
            raise RuntimeError(f"pilot actor missing: {required_label}")
    selected_labels = sorted(label for label in actors if label.startswith("SM_thatched_roof"))
    observed = {}
    material_paths = set()
    for label in selected_labels:
        actor = actors.get(label)
        slots = []
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            for slot_index in range(component.get_num_materials()):
                material = component.get_material(slot_index)
                path = "" if material is None else str(material.get_path_name())
                slots.append(path)
                if path:
                    material_paths.add(path)
        observed[label] = slots
    if not material_paths:
        raise RuntimeError(f"no materials found on {TARGET_LABELS}")
    material_paths.update(EXTRA_BLOCKER_MATERIALS)
    return observed, sorted(material_paths)


def _collection_parameter(material, collection, parameter_name, x, y):
    expression = _node(material, unreal.MaterialExpressionCollectionParameter, x, y)
    if not unreal.GameXXKMaterialAuthoringLibrary.bind_collection_parameter(
        expression, collection, parameter_name
    ):
        raise RuntimeError(f"failed to bind collection parameter: {parameter_name}")
    return expression


def _patch_cutout(material, collection):
    source_blend = material.get_blend_mode()
    if source_blend == unreal.BlendMode.BLEND_OPAQUE:
        material_property = unreal.MaterialProperty.MP_OPACITY_MASK
        target_blend = unreal.BlendMode.BLEND_MASKED
        preserve_existing = False
        force_masked = True
    elif source_blend == unreal.BlendMode.BLEND_MASKED:
        material_property = unreal.MaterialProperty.MP_OPACITY_MASK
        target_blend = unreal.BlendMode.BLEND_MASKED
        preserve_existing = True
        force_masked = True
    elif source_blend == unreal.BlendMode.BLEND_TRANSLUCENT:
        material_property = unreal.MaterialProperty.MP_OPACITY
        target_blend = unreal.BlendMode.BLEND_TRANSLUCENT
        preserve_existing = True
        force_masked = False
    else:
        raise RuntimeError(f"unsupported source blend: {source_blend}")
    existing = (
        _existing_property_connection(material, material_property)
        if preserve_existing
        else None
    )
    _set(material, "material_domain", unreal.MaterialDomain.MD_SURFACE)
    _set(material, "blend_mode", target_blend)
    if force_masked:
        _set(material, "opacity_mask_clip_value", 0.333)

    screen = _node(material, unreal.MaterialExpressionScreenPosition, -1150, 640)
    center = _collection_parameter(material, collection, "OcclusionCenter", -1150, 780)
    center_rg = _node(material, unreal.MaterialExpressionComponentMask, -940, 780)
    for channel, enabled in (("r", True), ("g", True), ("b", False), ("a", False)):
        _set(center_rg, channel, enabled)
    _connect(center, ("", "RGB"), center_rg, ("Input", ""))

    delta = _node(material, unreal.MaterialExpressionSubtract, -900, 650)
    _connect(screen, ("ViewportUV", ""), delta, ("A",))
    _connect(center_rg, ("",), delta, ("B",))

    aspect = _collection_parameter(material, collection, "OcclusionAspect", -900, 900)
    aspect_rg = _node(material, unreal.MaterialExpressionComponentMask, -690, 900)
    for channel, enabled in (("r", True), ("g", True), ("b", False), ("a", False)):
        _set(aspect_rg, channel, enabled)
    _connect(aspect, ("", "RGB"), aspect_rg, ("Input", ""))
    corrected = _node(material, unreal.MaterialExpressionMultiply, -650, 650)
    _connect(delta, ("",), corrected, ("A",))
    _connect(aspect_rg, ("",), corrected, ("B",))

    dot = _node(material, unreal.MaterialExpressionDotProduct, -430, 650)
    _connect(corrected, ("",), dot, ("A",))
    _connect(corrected, ("",), dot, ("B",))
    distance = _node(material, unreal.MaterialExpressionSquareRoot, -230, 650)
    _connect(dot, ("",), distance, ("Input", ""))
    radius = _collection_parameter(material, collection, "OcclusionRadius", -230, 820)
    edge = _node(material, unreal.MaterialExpressionSubtract, 0, 700)
    _connect(distance, ("",), edge, ("A",))
    _connect(radius, ("",), edge, ("B",))
    feather = _node(material, unreal.MaterialExpressionConstant, 0, 860)
    _set(feather, "r", 0.02)
    normalized = _node(material, unreal.MaterialExpressionDivide, 210, 720)
    _connect(edge, ("",), normalized, ("A",))
    _connect(feather, ("",), normalized, ("B",))
    circle_keep = _node(material, unreal.MaterialExpressionSaturate, 420, 720)
    _connect(normalized, ("",), circle_keep, ("Input", ""))
    circle_reveal = _node(material, unreal.MaterialExpressionOneMinus, 620, 720)
    _connect(circle_keep, ("",), circle_reveal, ("Input", ""))

    world_position = _node(material, unreal.MaterialExpressionWorldPosition, -1150, 1040)
    camera_location = _collection_parameter(material, collection, "OcclusionCameraLocation", -940, 1080)
    camera_location_rgb = _node(material, unreal.MaterialExpressionComponentMask, -730, 1080)
    for channel, enabled in (("r", True), ("g", True), ("b", True), ("a", False)):
        _set(camera_location_rgb, channel, enabled)
    _connect(camera_location, ("", "RGB"), camera_location_rgb, ("Input", ""))
    relative_position = _node(material, unreal.MaterialExpressionSubtract, -730, 990)
    _connect(world_position, ("", "XYZ"), relative_position, ("A",))
    _connect(camera_location_rgb, ("",), relative_position, ("B",))
    camera_forward = _collection_parameter(material, collection, "OcclusionCameraForward", -730, 1210)
    camera_forward_rgb = _node(material, unreal.MaterialExpressionComponentMask, -510, 1210)
    for channel, enabled in (("r", True), ("g", True), ("b", True), ("a", False)):
        _set(camera_forward_rgb, channel, enabled)
    _connect(camera_forward, ("", "RGB"), camera_forward_rgb, ("Input", ""))
    pixel_view_depth = _node(material, unreal.MaterialExpressionDotProduct, -480, 1010)
    _connect(relative_position, ("",), pixel_view_depth, ("A",))
    _connect(camera_forward_rgb, ("",), pixel_view_depth, ("B",))
    hero_view_depth = _collection_parameter(material, collection, "OcclusionHeroViewDepth", -500, 1120)
    depth_bias = _collection_parameter(material, collection, "OcclusionDepthBias", -300, 1160)
    foreground_threshold = _node(material, unreal.MaterialExpressionSubtract, -250, 1090)
    _connect(hero_view_depth, ("",), foreground_threshold, ("A",))
    _connect(depth_bias, ("",), foreground_threshold, ("B",))
    zero = _node(material, unreal.MaterialExpressionConstant, -50, 970)
    _set(zero, "r", 0.0)
    one = _node(material, unreal.MaterialExpressionConstant, -50, 1060)
    _set(one, "r", 1.0)
    is_foreground = _node(material, unreal.MaterialExpressionIf, 0, 1010)
    _connect(pixel_view_depth, ("",), is_foreground, ("A",))
    _connect(foreground_threshold, ("",), is_foreground, ("B",))
    _connect(zero, ("",), is_foreground, ("A > B", "A == B"))
    _connect(one, ("",), is_foreground, ("A < B",))

    # Masked surface materials cannot sample CustomStencil in UE 5.8.  Keep the
    # stable legacy fallback: the circular scope is restricted to geometry that
    # lies between the camera and hero, without sampling scene textures here.
    gated_reveal = _node(material, unreal.MaterialExpressionMultiply, 200, 860)
    _connect(circle_reveal, ("",), gated_reveal, ("A",))
    _connect(is_foreground, ("",), gated_reveal, ("B",))
    keep_mask = _node(material, unreal.MaterialExpressionOneMinus, 620, 860)
    _connect(gated_reveal, ("",), keep_mask, ("Input", ""))
    final_mask = keep_mask
    if existing is not None:
        original_node, output_name = existing
        combined_mask = _node(material, unreal.MaterialExpressionMultiply, 620, 860)
        _connect(original_node, (output_name, ""), combined_mask, ("A",))
        _connect(keep_mask, ("",), combined_mask, ("B",))
        final_mask = combined_mask
    _connect_property(final_mask, ("",), material_property)

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    if force_masked and not unreal.GameXXKMaterialAuthoringLibrary.force_masked_material_compilation(material):
        raise RuntimeError(f"failed to force masked compilation for {material.get_path_name()}")
    errors = [str(item) for item in unreal.MaterialEditingLibrary.recompile_material(material)]
    if errors:
        raise RuntimeError(f"recompile_material failed for {material.get_path_name()}: {errors}")
    material.modify()


def _duplicate_cutout_family(source, collection, cache, created):
    source_path = str(source.get_path_name())
    if source_path in cache:
        return cache[source_path]
    destination = f"{DEST_FOLDER}/{source.get_name()}_Cutout"
    if unreal.EditorAssetLibrary.does_asset_exist(destination):
        if not destination.startswith(DEST_FOLDER + "/"):
            raise RuntimeError(f"refusing to replace non-project asset {destination}")
        unreal.EditorAssetLibrary.delete_asset(destination)
    duplicate = unreal.EditorAssetLibrary.duplicate_asset(source_path, destination)
    if duplicate is None:
        raise RuntimeError(f"failed to duplicate {source_path} -> {destination}")

    if isinstance(source, unreal.Material):
        _patch_cutout(duplicate, collection)
    elif isinstance(source, unreal.MaterialInstanceConstant):
        parent = source.get_editor_property("parent")
        if parent is None:
            raise RuntimeError(f"material instance has no parent: {source_path}")
        cutout_parent = _duplicate_cutout_family(parent, collection, cache, created)
        unreal.MaterialEditingLibrary.set_material_instance_parent(duplicate, cutout_parent)
        duplicate.modify()
    else:
        raise RuntimeError(f"unsupported roof material type: {source_path} ({source.get_class().get_name()})")

    if not unreal.EditorAssetLibrary.save_asset(destination, only_if_is_dirty=False):
        raise RuntimeError(f"failed to save_asset {destination}")
    cache[source_path] = duplicate
    created.append(destination)
    return duplicate


def author():
    observed, material_paths = _inspect_target_materials()
    collection = _ensure_collection()
    created = []
    mapping = {}
    cache = {}
    for source_path in material_paths:
        source = unreal.EditorAssetLibrary.load_asset(source_path)
        duplicate = _duplicate_cutout_family(source, collection, cache, created)
        mapping[source_path] = str(duplicate.get_path_name()).split(".", 1)[0]

    # Reassert masked roots after the full instance graph has been reparented. UE may
    # recache a copied root's base properties while child parents are changing.
    for duplicate in cache.values():
        if isinstance(duplicate, unreal.Material):
            if (
                duplicate.get_blend_mode() == unreal.BlendMode.BLEND_MASKED
                and not unreal.GameXXKMaterialAuthoringLibrary.force_masked_material_compilation(duplicate)
            ):
                raise RuntimeError(f"final masked compilation failed for {duplicate.get_path_name()}")
            errors = [str(item) for item in unreal.MaterialEditingLibrary.recompile_material(duplicate)]
            if errors:
                raise RuntimeError(f"final root recompile failed for {duplicate.get_path_name()}: {errors}")
            if not unreal.EditorAssetLibrary.save_asset(
                str(duplicate.get_path_name()).split(".", 1)[0], only_if_is_dirty=False
            ):
                raise RuntimeError(f"failed final root save for {duplicate.get_path_name()}")

    invalid_blends = {}
    for source_path, cutout in cache.items():
        source = unreal.EditorAssetLibrary.load_asset(source_path)
        expected = (
            unreal.BlendMode.BLEND_TRANSLUCENT
            if source.get_blend_mode() == unreal.BlendMode.BLEND_TRANSLUCENT
            else unreal.BlendMode.BLEND_MASKED
        )
        if cutout.get_blend_mode() != expected:
            invalid_blends[source_path] = str(cutout.get_blend_mode())
    if invalid_blends:
        raise RuntimeError(f"cutout family did not preserve target blend: {invalid_blends}")

    report = {
        "actors": observed,
        "mapping": mapping,
        "created": created,
        "collection": MPC_PATH,
        "all_cutout_blends_preserved": True,
    }
    print(json.dumps(report, ensure_ascii=False))
    return report


if __name__ == "__main__":
    author()
