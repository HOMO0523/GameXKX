"""Read-only structural validation for the Qingshan town PCG prototype."""

from __future__ import annotations

import json
import math
from typing import Any, Iterable

import unreal


SOURCE_MAP = "/Game/GameXXK/Maps/L_QingshanInn"
PROTOTYPE_MAP = "/Game/GameXXK/Maps/Prototype/L_QingshanTown_PCG_Prototype"
EXPECTED_GRAPH_PATH = "/Game/GameXXK/Environment/TownPCG/VerticalSlice/PCG_QingshanTown_VerticalSlice"
EXPECTED_BUILDING_MESH = "/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA/SM_Qingshan_Shop_A_HQ_Retop50K"
PCG_ACTOR_LABEL = "QingshanTown_PCG_Buildings"
BUILDING_HARD_CAP = 16
EXPECTED_POINT_COUNT = 12
EXPECTED_NODE_COUNT = 2
EXPECTED_EDGE_COUNT = 2
EXPECTED_INSTANCE_COUNT = 12
TREE_MARKER_COUNT = 12
TREE_MARKER_BUDGET = 48
TREE_MARKER_LABEL_PREFIX = "QingshanTown_TreeMarker_"
TREE_MARKER_MESH_CANDIDATES = (
    "/Quick_Road/Mesh/SM_Cone.SM_Cone",
    "/Engine/BasicShapes/Cone.Cone",
)
PRESERVED_LABELS = ("QingshanInn_TownExit", "PlayerStart_QingshanInn")
REQUIRED_FIXED_LABELS = (
    "QingshanTown_CityScope",
    "QingshanTown_MainRoad",
    "QingshanTown_RoadEdge_Left",
    "QingshanTown_RoadEdge_Right",
    "QingshanTown_River",
    "QingshanTown_Bridge",
    "QingshanTown_Market",
    "QingshanTown_SouthWharf",
)
REQUIRED_QUICK_ROAD_TAGS = {
    "Quick_Road_CityScope": 1,
    "Quick_Road_MainRoad": 1,
    "Quick_Road_RoadEdge": 2,
}

_SPLINE_CONTRACT = {
    "QingshanTown_CityScope": (4, True, "Quick_Road_CityScope"),
    "QingshanTown_MainRoad": (4, False, "Quick_Road_MainRoad"),
    "QingshanTown_RoadEdge_Left": (4, False, "Quick_Road_RoadEdge"),
    "QingshanTown_RoadEdge_Right": (4, False, "Quick_Road_RoadEdge"),
    "QingshanTown_River": (4, False, "TownPCG_River"),
}
_FIXED_ANCHORS = (
    "QingshanTown_Bridge",
    "QingshanTown_Market",
    "QingshanTown_SouthWharf",
)
_PROTOTYPE_ONLY_TAG = "PrototypeOnly"
_FIXED_ANCHOR_TAG = "TownPCG_FixedAnchor"
_TREE_TAGS = ("TownPCG_TreeDebug", "TownPCG_TreeMarker")
_BRIDGE_RIVER_XY_TOLERANCE_CM = 500.0
_ROAD_BRIDGE_XY_TOLERANCE_CM = 100.0


def _new_result() -> dict[str, Any]:
    return {
        "marker": "GAMEXXK_QINGSHAN_PCG_VALIDATION",
        "success": False,
        "errors": [],
        "warnings": [],
        "evidence": {},
    }


def _final_json(result: dict[str, Any]) -> str:
    return json.dumps(result, sort_keys=True, separators=(",", ":"))


def _error(result: dict[str, Any], check: str, message: str, **details: Any) -> None:
    entry: dict[str, Any] = {"check": check, "message": message}
    if details:
        entry["details"] = details
    result["errors"].append(entry)


def _warning(result: dict[str, Any], check: str, message: str, **details: Any) -> None:
    entry: dict[str, Any] = {"check": check, "message": message}
    if details:
        entry["details"] = details
    result["warnings"].append(entry)


def _object_path(value: Any) -> str:
    if value is None:
        return ""
    for getter_name in ("get_path_name", "get_name"):
        getter = getattr(value, getter_name, None)
        if callable(getter):
            try:
                return str(getter())
            except Exception:
                pass
    text = str(value)
    return text.split("'", 2)[1] if text.count("'") >= 2 else text


def _package_path(value: Any) -> str:
    path = _object_path(value)
    return path.split(".", 1)[0]


def _property(value: Any, name: str, default: Any = None) -> Any:
    getter = getattr(value, "get_editor_property", None)
    if callable(getter):
        try:
            return getter(name)
        except Exception:
            pass
    return getattr(value, name, default)


def _actor_label(actor: Any) -> str:
    return str(actor.get_actor_label())


def _class_path(value: Any) -> str:
    try:
        return _object_path(value.get_class())
    except Exception:
        return type(value).__name__


def _tags(value: Any) -> list[str]:
    raw = _property(value, "component_tags", None)
    if raw is None:
        raw = _property(value, "tags", ())
    return sorted(str(tag) for tag in (raw or ()))


def _dirty_map_package_names() -> list[str]:
    getter = getattr(unreal.EditorLoadingAndSavingUtils, "get_dirty_map_packages", None)
    if not callable(getter):
        raise RuntimeError("UE Python does not expose dirty-map package inspection")
    return sorted(_package_path(package) for package in getter())


def _current_map_package() -> str:
    world = unreal.EditorLevelLibrary.get_editor_world()
    return _package_path(world.get_outermost()) if world is not None else ""


def _all_actors() -> list[Any]:
    return list(unreal.EditorLevelLibrary.get_all_level_actors())


def _exact_label_matches(actors: Iterable[Any], label: str) -> list[Any]:
    return [actor for actor in actors if _actor_label(actor) == label]


def _current_level_actor(actor: Any, current_level: Any) -> bool:
    try:
        return actor.get_level() == current_level
    except Exception:
        return False


def _vector(value: Any) -> list[float]:
    return [float(value.x), float(value.y), float(value.z)]


def _delta(first: Any, second: Any) -> dict[str, Any]:
    values = [float(first.x - second.x), float(first.y - second.y), float(first.z - second.z)]
    return {
        "xyz_cm": values,
        "xy_distance_cm": math.hypot(values[0], values[1]),
        "distance_cm": math.sqrt(sum(component * component for component in values)),
    }


def _component_list(actor: Any, component_class: Any) -> list[Any]:
    try:
        return list(actor.get_components_by_class(component_class))
    except Exception:
        component = actor.get_component_by_class(component_class)
        return [component] if component is not None else []


def _validate_labels_and_tags(
    result: dict[str, Any], actors: list[Any], current_level: Any
) -> dict[str, Any]:
    evidence = result["evidence"]
    all_required = REQUIRED_FIXED_LABELS + (PCG_ACTOR_LABEL,) + PRESERVED_LABELS + tuple(
        f"{TREE_MARKER_LABEL_PREFIX}{index:02d}" for index in range(TREE_MARKER_COUNT)
    )
    counts = {label: len(_exact_label_matches(actors, label)) for label in all_required}
    evidence["required_label_counts"] = counts
    for label, count in counts.items():
        if count != 1:
            _error(result, "exact_label_unique", f"{label} must occur exactly once", label=label, count=count)
        elif not _current_level_actor(_exact_label_matches(actors, label)[0], current_level):
            _error(result, "current_level", f"{label} is not owned by the current level", label=label)

    tag_counts = {
        tag: sum(1 for actor in actors if tag in _tags(actor))
        for tag in REQUIRED_QUICK_ROAD_TAGS
    }
    evidence["quick_road_tag_counts"] = tag_counts
    for tag, expected in REQUIRED_QUICK_ROAD_TAGS.items():
        if tag_counts[tag] != expected:
            _error(result, "quick_road_tags", f"{tag} count must be {expected}", tag=tag, count=tag_counts[tag])

    ownership: dict[str, Any] = {}
    semantic_tags = {
        label: semantic for label, (_, _, semantic) in _SPLINE_CONTRACT.items()
    }
    for label in REQUIRED_FIXED_LABELS:
        matches = _exact_label_matches(actors, label)
        if len(matches) != 1:
            continue
        actor_tags = _tags(matches[0])
        expected_tags = [_PROTOTYPE_ONLY_TAG]
        if label in semantic_tags:
            expected_tags.append(semantic_tags[label])
        if label in _FIXED_ANCHORS:
            expected_tags.append(_FIXED_ANCHOR_TAG)
        missing = [tag for tag in expected_tags if tag not in actor_tags]
        ownership[label] = {"tags": actor_tags, "expected_tags": expected_tags, "missing_tags": missing}
        if missing:
            _error(result, "managed_actor_tags", f"{label} lacks ownership/semantic tags", label=label, missing=missing)
    evidence["managed_actor_ownership"] = ownership
    evidence["preserved_labels"] = {label: counts[label] for label in PRESERVED_LABELS}
    return counts


def _validate_splines(result: dict[str, Any], actors: list[Any]) -> dict[str, Any]:
    spline_evidence: dict[str, Any] = {}
    for label, (expected_points, expected_closed, semantic_tag) in _SPLINE_CONTRACT.items():
        matches = _exact_label_matches(actors, label)
        if len(matches) != 1:
            continue
        components = _component_list(matches[0], unreal.SplineComponent)
        item: dict[str, Any] = {
            "component_count": len(components),
            "expected_point_count": expected_points,
            "expected_closed": expected_closed,
            "semantic_tag": semantic_tag,
        }
        if len(components) != 1:
            _error(result, "spline_component_count", f"{label} must own exactly one spline component", label=label, count=len(components))
        else:
            spline = components[0]
            point_count = int(spline.get_number_of_spline_points())
            closed = bool(spline.is_closed_loop())
            item.update({"point_count": point_count, "closed": closed, "component_tags": _tags(spline)})
            if point_count != expected_points:
                _error(result, "spline_point_count", f"{label} point count must be {expected_points}", label=label, count=point_count)
            if closed != expected_closed:
                _error(result, "spline_closed_state", f"{label} closed state is incorrect", label=label, closed=closed)
            for tag in (_PROTOTYPE_ONLY_TAG, semantic_tag):
                if tag not in _tags(spline):
                    _error(result, "spline_component_tags", f"{label} spline lacks {tag}", label=label, tag=tag)
        spline_evidence[label] = item
    result["evidence"]["spline_evidence"] = spline_evidence
    return spline_evidence


def _load_json_status(result: dict[str, Any]) -> dict[str, Any]:
    try:
        payload = unreal.GameXXKTownPCGAutomationLibrary.get_town_pcg_status(PCG_ACTOR_LABEL)
        status = json.loads(str(payload))
        if status.get("success") is not True:
            _error(result, "pcg_status", "read-only C++ PCG status failed", status=status)
        return status
    except Exception as error:
        _error(result, "pcg_status", f"could not inspect read-only C++ PCG status: {error}")
        return {}


def _graph_nodes(graph: Any) -> list[Any]:
    nodes = _property(graph, "nodes", None)
    if nodes is not None:
        return list(nodes)
    getter = getattr(graph, "get_nodes", None)
    return list(getter()) if callable(getter) else []


def _settings_for_node(node: Any) -> Any:
    for name in ("get_settings", "get_settings_interface"):
        getter = getattr(node, name, None)
        if callable(getter):
            try:
                return getter()
            except Exception:
                pass
    return _property(node, "settings", _property(node, "settings_interface", None))


def _reflected_edge_count(nodes: Iterable[Any]) -> int:
    count = 0
    for node in nodes:
        output_pins = list(node.get_editor_property("output_pins"))
        for pin in output_pins:
            count += len(list(pin.get_editor_property("edges")))
    return count


def _validate_graph(result: dict[str, Any], pcg_component: Any, status: dict[str, Any]) -> None:
    graph = unreal.load_asset(EXPECTED_GRAPH_PATH)
    graph_path = _package_path(graph)
    nodes = _graph_nodes(graph) if graph is not None else []
    point_count = -1
    node_classes: list[str] = []
    for node in nodes:
        settings = _settings_for_node(node)
        node_classes.append(_class_path(settings or node))
        points = _property(settings, "points_to_create", None) if settings is not None else None
        if points is not None:
            point_count = len(points)
    component_graph = pcg_component.get_graph() if pcg_component is not None else None
    component_graph_path = _package_path(component_graph)
    verified_edge_count = _reflected_edge_count(nodes)
    evidence = {
        "path": graph_path,
        "component_graph_path": component_graph_path,
        "point_count": point_count,
        "node_count": len(nodes),
        "verified_edge_count": verified_edge_count,
        "node_classes": node_classes,
        "read_only_status": status,
    }
    result["evidence"]["pcg_graph"] = evidence
    expected = {
        "path": EXPECTED_GRAPH_PATH,
        "component_graph_path": EXPECTED_GRAPH_PATH,
        "point_count": EXPECTED_POINT_COUNT,
        "node_count": EXPECTED_NODE_COUNT,
        "verified_edge_count": EXPECTED_EDGE_COUNT,
    }
    for field, wanted in expected.items():
        if evidence[field] != wanted:
            _error(result, "pcg_graph", f"graph {field} must equal {wanted!r}", field=field, actual=evidence[field])


def _static_mesh(component: Any) -> Any:
    getter = getattr(component, "get_static_mesh", None)
    return getter() if callable(getter) else _property(component, "static_mesh", None)


def _validate_pcg_actor(result: dict[str, Any], actors: list[Any], current_level: Any) -> None:
    matches = _exact_label_matches(actors, PCG_ACTOR_LABEL)
    current_level_pcg_volumes = [
        actor for actor in actors
        if isinstance(actor, unreal.PCGVolume) and _current_level_actor(actor, current_level)
    ]
    pcg_evidence: dict[str, Any] = {
        "actor_count": len(current_level_pcg_volumes),
        "expected_label_count": len(matches),
        "current_level_count": 0,
        "current_level_labels": sorted(_actor_label(actor) for actor in current_level_pcg_volumes),
    }
    result["evidence"]["pcg_actor"] = pcg_evidence
    if len(current_level_pcg_volumes) != 1:
        _error(result, "pcg_volume_count", "exactly one current-level PCGVolume is required", count=len(current_level_pcg_volumes), labels=pcg_evidence["current_level_labels"])
    if len(matches) != 1:
        _error(result, "pcg_actor", "expected PCG actor label must occur exactly once", count=len(matches))
        return
    actor = matches[0]
    pcg_evidence["class"] = _class_path(actor)
    pcg_evidence["current_level_count"] = int(_current_level_actor(actor, current_level))
    if not isinstance(actor, unreal.PCGVolume):
        _error(result, "pcg_actor_class", "PCG actor must be a PCGVolume", actual=_class_path(actor))
    if not _current_level_actor(actor, current_level):
        _error(result, "pcg_actor_level", "PCG actor is not in the current level")
    components = _component_list(actor, unreal.PCGComponent)
    pcg_evidence["pcg_component_count"] = len(components)
    if len(components) != 1:
        _error(result, "pcg_component", "PCG volume must own exactly one PCG component", count=len(components))
        pcg_component = components[0] if components else None
    else:
        pcg_component = components[0]

    trigger = str(_property(pcg_component, "generation_trigger", "")) if pcg_component is not None else ""
    generate_on_drop = bool(_property(pcg_component, "generate_on_drop_when_trigger_on_demand", False)) if pcg_component is not None else True
    regenerate = bool(_property(pcg_component, "regenerate_in_editor", False)) if pcg_component is not None else True
    runtime_disabled = "ON_DEMAND" in trigger.upper() and not generate_on_drop and not regenerate
    pcg_evidence["runtime_generation_disabled"] = runtime_disabled
    pcg_evidence["generation_trigger"] = trigger
    pcg_evidence["generate_on_drop_when_trigger_on_demand"] = generate_on_drop
    pcg_evidence["regenerate_in_editor"] = regenerate
    if not runtime_disabled:
        _error(result, "runtime_generation", "runtime/editor automatic PCG generation must be disabled", trigger=trigger)

    status = _load_json_status(result)
    _validate_graph(result, pcg_component, status)
    components = _component_list(actor, unreal.InstancedStaticMeshComponent)
    generated_component_count = len(components)
    instance_count = sum(int(component.get_instance_count()) for component in components)
    mesh_paths = sorted({_package_path(_static_mesh(component)) for component in components})
    pcg_evidence.update({
        "generated_component_count": generated_component_count,
        "building_instance_count": instance_count,
        "building_hard_cap": BUILDING_HARD_CAP,
        "generated_mesh": mesh_paths,
    })
    if int(status.get("generated_component_count", -1)) != 1 or generated_component_count != 1:
        _error(result, "generated_components", "exactly one generated instanced component is required", reflected=generated_component_count, status=status.get("generated_component_count"))
    if instance_count != EXPECTED_INSTANCE_COUNT:
        _error(result, "building_instances", f"building instance count must be {EXPECTED_INSTANCE_COUNT}", count=instance_count)
    if instance_count > BUILDING_HARD_CAP:
        _error(result, "building_hard_cap", "building instances exceed the hard cap", count=instance_count, hard_cap=BUILDING_HARD_CAP)
    if mesh_paths != [EXPECTED_BUILDING_MESH]:
        _error(result, "generated_mesh", "generated building mesh identity is incorrect", actual=mesh_paths, expected=EXPECTED_BUILDING_MESH)


def _validate_mesh(result: dict[str, Any]) -> None:
    mesh = unreal.load_asset(EXPECTED_BUILDING_MESH)
    evidence: dict[str, Any] = {"path": _package_path(mesh)}
    result["evidence"]["generated_mesh"] = evidence
    if mesh is None or evidence["path"] != EXPECTED_BUILDING_MESH:
        _error(result, "building_mesh", "tracked Qingshan shop mesh could not be loaded", path=evidence["path"])
        return
    materials = _property(mesh, "static_materials", ()) or ()
    evidence["material_slot_count"] = len(materials)
    nanite = _property(mesh, "nanite_settings", None)
    evidence["nanite_enabled"] = bool(_property(nanite, "enabled", False)) if nanite is not None else None
    body_setup = getattr(mesh, "get_body_setup", lambda: None)()
    trace_flag = str(_property(body_setup, "collision_trace_flag", "")) if body_setup is not None else ""
    simple_count = -1
    try:
        simple_count = int(unreal.EditorStaticMeshLibrary.get_simple_collision_count(mesh))
    except Exception as error:
        _warning(result, "collision_inspection", f"simple collision count was unavailable: {error}")
    accepted = simple_count > 0 and "COMPLEX_AS_SIMPLE" not in trace_flag.upper()
    collision = {
        "body_setup_present": body_setup is not None,
        "collision_trace_flag": trace_flag,
        "collision_mode": "simple_primitives" if simple_count > 0 else "absent",
        "simple_collision_count": simple_count,
        "simple_collision_accepted": accepted,
    }
    result["evidence"]["collision"] = collision
    if not accepted:
        _error(result, "building_collision", "building mesh must provide simple collision and must not rely on complex-as-simple", collision=collision)


def _validate_trees(result: dict[str, Any], actors: list[Any]) -> None:
    labels = [f"{TREE_MARKER_LABEL_PREFIX}{index:02d}" for index in range(TREE_MARKER_COUNT)]
    current_level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_current_level()
    tagged_marker_actors = [
        actor for actor in actors
        if _current_level_actor(actor, current_level)
        and any(tag in _tags(actor) for tag in _TREE_TAGS)
    ]
    unexpected_marker_labels = sorted(
        _actor_label(actor) for actor in tagged_marker_actors if _actor_label(actor) not in labels
    )
    paths: list[str] = []
    invalid: list[dict[str, Any]] = []
    exact_label_actor_count = 0
    for label in labels:
        matches = _exact_label_matches(actors, label)
        exact_label_actor_count += len(matches)
        if len(matches) != 1:
            continue
        actor = matches[0]
        components = _component_list(actor, unreal.StaticMeshComponent)
        mesh_path = _package_path(_static_mesh(components[0])) if len(components) == 1 else ""
        paths.append(mesh_path)
        missing_tags = [tag for tag in (_PROTOTYPE_ONLY_TAG,) + _TREE_TAGS if tag not in _tags(actor)]
        allowed_meshes = {_package_path(candidate) for candidate in TREE_MARKER_MESH_CANDIDATES}
        if not isinstance(actor, unreal.StaticMeshActor) or len(components) != 1 or mesh_path not in allowed_meshes or missing_tags:
            invalid.append({"label": label, "class": _class_path(actor), "component_count": len(components), "mesh": mesh_path, "missing_tags": missing_tags})
    evidence = {
        "count": len(tagged_marker_actors),
        "exact_label_actor_count": exact_label_actor_count,
        "unique_label_count": len(set(labels)),
        "expected_count": TREE_MARKER_COUNT,
        "mesh_paths": sorted(set(paths)),
        "actor_instance_budget": TREE_MARKER_BUDGET,
        "within_budget": len(tagged_marker_actors) <= TREE_MARKER_BUDGET,
        "unexpected_marker_labels": unexpected_marker_labels,
        "invalid": invalid,
    }
    result["evidence"]["tree_markers"] = evidence
    if len(tagged_marker_actors) != TREE_MARKER_COUNT or exact_label_actor_count != TREE_MARKER_COUNT:
        _error(result, "tree_marker_count", f"tree marker count must be {TREE_MARKER_COUNT}", tagged_count=len(tagged_marker_actors), exact_label_count=exact_label_actor_count)
    if len(tagged_marker_actors) > TREE_MARKER_BUDGET:
        _error(result, "tree_marker_budget", "tree markers exceed the actor/instance budget", count=len(tagged_marker_actors), budget=TREE_MARKER_BUDGET)
    if unexpected_marker_labels:
        _error(result, "tree_marker_labels", "unexpected current-level actors carry tree marker ownership tags", labels=unexpected_marker_labels)
    if invalid:
        _error(result, "tree_markers", "tree markers have wrong class, mesh, component count, or tags", invalid=invalid)


def _closest_spline_location(spline: Any, world_location: Any) -> Any:
    coordinate_space = unreal.SplineCoordinateSpace.WORLD
    return spline.find_location_closest_to_world_location(world_location, coordinate_space)


def _validate_alignment(result: dict[str, Any], actors: list[Any]) -> None:
    needed = ("QingshanTown_Bridge", "QingshanTown_River", "QingshanTown_MainRoad")
    found = {label: _exact_label_matches(actors, label) for label in needed}
    if any(len(found[label]) != 1 for label in needed):
        result["evidence"]["alignment"] = {"available": False}
        _error(result, "alignment", "bridge, river, and main road must be unique before alignment can be checked")
        return
    bridge_location = found["QingshanTown_Bridge"][0].get_actor_location()
    river = _component_list(found["QingshanTown_River"][0], unreal.SplineComponent)
    road = _component_list(found["QingshanTown_MainRoad"][0], unreal.SplineComponent)
    if len(river) != 1 or len(road) != 1:
        result["evidence"]["alignment"] = {"available": False}
        _error(result, "alignment", "river and main road require one spline each for alignment")
        return
    river_location = _closest_spline_location(river[0], bridge_location)
    road_location = _closest_spline_location(road[0], bridge_location)
    river_delta = _delta(bridge_location, river_location)
    road_delta = _delta(bridge_location, road_location)
    alignment = {
        "available": True,
        "bridge_location_cm": _vector(bridge_location),
        "river_closest_location_cm": _vector(river_location),
        "main_road_closest_location_cm": _vector(road_location),
        "bridge_to_river": {**river_delta, "tolerance_xy_cm": _BRIDGE_RIVER_XY_TOLERANCE_CM},
        "road_to_bridge": {**road_delta, "tolerance_xy_cm": _ROAD_BRIDGE_XY_TOLERANCE_CM},
    }
    result["evidence"]["alignment"] = alignment
    if river_delta["xy_distance_cm"] > _BRIDGE_RIVER_XY_TOLERANCE_CM:
        _error(result, "bridge_river_alignment", "bridge is outside the river centerline tolerance", **alignment["bridge_to_river"])
    if road_delta["xy_distance_cm"] > _ROAD_BRIDGE_XY_TOLERANCE_CM:
        _error(result, "road_bridge_alignment", "bridge is outside the main-road centerline tolerance", **alignment["road_to_bridge"])


def validate_vertical_slice() -> dict[str, Any]:
    result = _new_result()
    dirty_before: list[str] = []
    try:
        dirty_before = _dirty_map_package_names()
        result["evidence"]["dirty_map_packages_before"] = dirty_before
        if SOURCE_MAP in dirty_before or PROTOTYPE_MAP in dirty_before:
            _error(result, "dirty_before", "source and prototype maps must be clean before validation", dirty=dirty_before)
        if not unreal.EditorAssetLibrary.does_asset_exist(PROTOTYPE_MAP):
            _error(result, "prototype_map", "prototype map does not exist", path=PROTOTYPE_MAP)
        elif not unreal.EditorLoadingAndSavingUtils.load_map(PROTOTYPE_MAP):
            _error(result, "prototype_map", "prototype map could not be loaded", path=PROTOTYPE_MAP)
        if _current_map_package() != PROTOTYPE_MAP:
            _error(result, "current_map", "validator must inspect only the prototype map", current=_current_map_package())

        source_not_current = _current_map_package() != SOURCE_MAP
        result["evidence"]["source_map_not_current"] = source_not_current
        if not source_not_current:
            _error(result, "source_map", "source map must never be the current validation map")

        current_level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_current_level()
        actors = _all_actors()
        _validate_labels_and_tags(result, actors, current_level)
        _validate_splines(result, actors)
        _validate_pcg_actor(result, actors, current_level)
        _validate_mesh(result)
        _validate_trees(result, actors)
        _validate_alignment(result, actors)
    except Exception as error:
        _error(result, "validator_exception", f"unexpected validation exception: {error}", exception_type=type(error).__name__)
    finally:
        try:
            dirty_after = _dirty_map_package_names()
        except Exception as error:
            dirty_after = []
            _error(result, "dirty_after", f"could not snapshot dirty maps after validation: {error}")
        result["evidence"]["dirty_map_packages_after"] = dirty_after
        newly_dirty_protected = sorted(
            path for path in (SOURCE_MAP, PROTOTYPE_MAP)
            if path in dirty_after and path not in dirty_before
        )
        result["evidence"]["newly_dirty_protected_maps"] = newly_dirty_protected
        if SOURCE_MAP in dirty_after:
            _error(result, "source_map_dirty", "source map is dirty after validation")
        if PROTOTYPE_MAP in dirty_after:
            _error(result, "prototype_map_dirty", "prototype map is dirty after validation")
        if newly_dirty_protected:
            _error(result, "read_only", "validation dirtied a protected map", maps=newly_dirty_protected)
    result["success"] = not result["errors"]
    return result


def main() -> None:
    print(_final_json(validate_vertical_slice()))


if __name__ == "__main__":
    main()
