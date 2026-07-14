"""Read-only PIE evidence for the legacy material-swap occlusion path."""

from __future__ import annotations

import json

import unreal


CUTOUT_FOLDER = "/Game/GameXXK/Materials/Occlusion/AsianVillage"
MPC_PATH = "/Game/GameXXK/Materials/Occlusion/MPC_PlayerOcclusion"
MPC_OBJECT_PATH = MPC_PATH + ".MPC_PlayerOcclusion"
SOURCE_BY_CUTOUT_NAME = {
    "MI_wooden_board_03_Inst_Cutout": (
        "/Game/Asian_Village/materials/building_materials/base_materials/"
        "MI_wooden_board_03_Inst.MI_wooden_board_03_Inst"
    ),
    "MI_building_wood_06_Inst_Cutout": (
        "/Game/Asian_Village/materials/building_materials/"
        "MI_building_wood_06_Inst.MI_building_wood_06_Inst"
    ),
}


def _path(value):
    return "" if value is None else str(value.get_path_name())


def _material_slots(component):
    slots = []
    if component is None:
        return slots
    for index in range(component.get_num_materials()):
        material = component.get_material(index)
        material_path = _path(material)
        material_name = "" if material is None else str(material.get_name())
        candidate = f"{CUTOUT_FOLDER}/{material_name}_Cutout"
        slots.append({
            "slot": index,
            "material": material_path,
            "material_name": material_name,
            "is_cutout": material_name.endswith("_Cutout"),
            "would_resolve_to": candidate,
            "candidate_exists": bool(unreal.EditorAssetLibrary.does_asset_exist(candidate)),
        })
    return slots


def _material_graph_summary(material):
    chain = []
    instance_parameters = []
    current = material
    root = None
    while current is not None:
        chain.append(_path(current))
        if isinstance(current, unreal.MaterialInstanceConstant):
            parameter_rows = {}
            for property_name in (
                "scalar_parameter_values",
                "vector_parameter_values",
                "texture_parameter_values",
            ):
                try:
                    values = current.get_editor_property(property_name)
                    rows = []
                    for value in values:
                        info = value.get_editor_property("parameter_info")
                        parameter_name = str(info.get_editor_property("name"))
                        parameter_value = value.get_editor_property("parameter_value")
                        if isinstance(parameter_value, unreal.Texture):
                            parameter_value = _path(parameter_value)
                        elif isinstance(parameter_value, unreal.LinearColor):
                            parameter_value = [
                                round(float(parameter_value.r), 5),
                                round(float(parameter_value.g), 5),
                                round(float(parameter_value.b), 5),
                                round(float(parameter_value.a), 5),
                            ]
                        else:
                            parameter_value = str(parameter_value)
                        rows.append({"name": parameter_name, "value": parameter_value})
                    parameter_rows[property_name] = rows
                except Exception as exc:
                    parameter_rows[property_name] = {"error": str(exc)}
            instance_parameters.append({
                "instance": _path(current),
                "parameters": parameter_rows,
            })
            current = current.get_editor_property("parent")
            continue
        if isinstance(current, unreal.Material):
            root = current
        break
    if root is None:
        return {"chain": chain, "root": ""}
    collection_parameters = []
    static_switches = []
    expression_error = ""
    try:
        expressions = unreal.MaterialEditingLibrary.get_material_expressions(root)
    except Exception as exc:
        expressions = []
        expression_error = str(exc)
    for expression in expressions:
        if isinstance(expression, unreal.MaterialExpressionCollectionParameter):
            collection = expression.get_editor_property("collection")
            collection_parameters.append({
                "collection": _path(collection),
                "parameter": str(expression.get_editor_property("parameter_name")),
            })
        if isinstance(expression, unreal.MaterialExpressionStaticSwitchParameter):
            try:
                static_switches.append({
                    "name": str(expression.get_editor_property("parameter_name")),
                    "default": bool(expression.get_editor_property("default_value")),
                })
            except Exception as exc:
                static_switches.append({"error": str(exc)})
    for instance_entry in instance_parameters:
        instance = unreal.load_asset(instance_entry["instance"])
        values = {}
        for switch in static_switches:
            parameter_name = switch.get("name")
            if not parameter_name:
                continue
            try:
                values[parameter_name] = bool(
                    unreal.MaterialEditingLibrary.get_material_instance_static_switch_parameter_value(
                        instance, parameter_name
                    )
                )
            except Exception as exc:
                values[parameter_name] = {"error": str(exc)}
        instance_entry["static_switches"] = values
    property_nodes = {}
    for label, property_id in (
        ("base_color", unreal.MaterialProperty.MP_BASE_COLOR),
        ("normal", unreal.MaterialProperty.MP_NORMAL),
        ("roughness", unreal.MaterialProperty.MP_ROUGHNESS),
        ("opacity_mask", unreal.MaterialProperty.MP_OPACITY_MASK),
    ):
        try:
            node = unreal.MaterialEditingLibrary.get_material_property_input_node(root, property_id)
            property_nodes[label] = "" if node is None else str(node.get_class().get_name())
        except Exception as exc:
            property_nodes[label] = {"error": str(exc)}
    return {
        "chain": chain,
        "instance_parameters": instance_parameters,
        "root": _path(root),
        "root_blend_mode": str(root.get_blend_mode()),
        "collection_parameters": collection_parameters,
        "static_switches": static_switches,
        "property_nodes": property_nodes,
        "expression_error": expression_error,
    }


def _collection_values(world):
    collection = unreal.EditorAssetLibrary.load_asset(MPC_PATH)
    if collection is None:
        collection = unreal.EditorAssetLibrary.load_asset(MPC_OBJECT_PATH)
    if collection is None:
        try:
            collection = unreal.load_asset(MPC_PATH)
        except Exception:
            collection = None
    if collection is None:
        try:
            collection = unreal.load_object(None, MPC_OBJECT_PATH)
        except Exception:
            collection = None
    if collection is None:
        return {
            "error": f"missing collection: {MPC_PATH}",
            "asset_registry_entries": [
                str(path)
                for path in unreal.EditorAssetLibrary.list_assets(
                    "/Game/GameXXK/Materials/Occlusion", recursive=False, include_folder=False
                )
            ],
        }
    try:
        instance = world.get_parameter_collection_instance(collection)
    except Exception as exc:
        return {
            "error": f"collection instance unavailable: {exc}",
            "world_collection_methods": [
                name for name in dir(world) if "collection" in name.lower()
            ],
        }
    result = {
        "instance": "" if instance is None else str(instance.get_path_name()),
    }
    for parameter in (
        "OcclusionRadius",
        "OcclusionHeroViewDepth",
        "OcclusionDepthBias",
        "OcclusionHeroStencilValue",
    ):
        try:
            result[parameter] = float(instance.get_scalar_parameter_value(parameter))
        except Exception as exc:
            result[parameter] = {"error": str(exc)}
    for parameter in (
        "OcclusionCenter",
        "OcclusionAspect",
        "OcclusionCameraLocation",
        "OcclusionCameraForward",
    ):
        try:
            value = instance.get_vector_parameter_value(parameter)
            result[parameter] = [
                round(float(value.r), 5),
                round(float(value.g), 5),
                round(float(value.b), 5),
                round(float(value.a), 5),
            ]
        except Exception as exc:
            result[parameter] = {"error": str(exc)}
    return result


def main():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world() if editor else None
    if world is None:
        raise RuntimeError("no active PIE world")
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    camera = controller.player_camera_manager if controller else None
    if pawn is None or camera is None:
        raise RuntimeError("PIE player/camera is unavailable")

    visual = None
    for component in pawn.get_components_by_class(unreal.PaperFlipbookComponent):
        if str(component.get_name()) == "Visual":
            visual = component
            break
    # PaperFlipbookComponent's SceneComponent location API is not exposed by
    # this editor's Python wrapper.  The hero root plus its visual offset is
    # the same stable target used by the existing PIE probe.
    target = pawn.get_actor_location() + unreal.Vector(0.0, 0.0, 40.0)
    hit = unreal.SystemLibrary.sphere_trace_single(
        world,
        camera.get_camera_location(),
        target,
        36.0,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        True,
        [pawn],
        unreal.DrawDebugTrace.NONE,
        True,
    )
    payload = {} if hit is None else hit.to_dict()
    component = payload.get("component") or payload.get("hit_component")
    actor = payload.get("actor") or payload.get("hit_actor")
    component_slots = _material_slots(component)
    component_graphs = [] if component is None else [
        _material_graph_summary(component.get_material(index))
        for index in range(component.get_num_materials())
    ]
    source_graphs = []
    if component is not None:
        for index in range(component.get_num_materials()):
            material = component.get_material(index)
            source_path = SOURCE_BY_CUTOUT_NAME.get("" if material is None else str(material.get_name()))
            source = None if source_path is None else unreal.load_asset(source_path)
            source_graphs.append({
                "slot": index,
                "source": source_path or "",
                "summary": {} if source is None else _material_graph_summary(source),
            })
    actor_slots = []
    if actor is not None:
        for primitive in actor.get_components_by_class(unreal.PrimitiveComponent):
            actor_slots.append({"component": str(primitive.get_name()), "slots": _material_slots(primitive)})

    report = {
        "blocked": bool(payload),
        "actor": "" if actor is None else str(actor.get_actor_label()),
        "component": "" if component is None else str(component.get_name()),
        "component_slots": component_slots,
        "component_graphs": component_graphs,
        "source_graphs": source_graphs,
        "actor_slots": actor_slots,
        "mpc_values": _collection_values(world),
    }
    print(json.dumps(report, ensure_ascii=False, sort_keys=True))
    return report


if __name__ == "__main__":
    main()
