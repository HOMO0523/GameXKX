"""Assemble the isolated Qingshan town PCG vertical-slice prototype map."""

from __future__ import annotations

import json
import math
import sys
from typing import Any, Iterable

import unreal

from gamexxk_qingshan_town_pcg_config import load_config


SOURCE_MAP = "/Game/GameXXK/Maps/L_QingshanInn"
PROTOTYPE_MAP = "/Game/GameXXK/Maps/Prototype/L_QingshanTown_PCG_Prototype"
PCG_ACTOR_LABEL = "QingshanTown_PCG_Buildings"
PHASE_SETUP = "setup"
PHASE_FINALIZE = "finalize"
PRESERVED_LABELS = ("QingshanInn_TownExit", "PlayerStart_QingshanInn")
QUICK_ROAD_TAGS = (
    "Quick_Road_CityScope",
    "Quick_Road_MainRoad",
    "Quick_Road_RoadEdge",
)
TREE_MARKER_LABEL_PREFIX = "QingshanTown_TreeMarker_"
TREE_MARKER_COUNT = 12
TREE_MARKER_MESH_CANDIDATES = (
    "/Quick_Road/Mesh/SM_Cone.SM_Cone",
    "/Engine/BasicShapes/Cone.Cone",
)
REQUIRED_MANAGED_LABELS = (
    "QingshanTown_CityScope",
    "QingshanTown_MainRoad",
    "QingshanTown_RoadEdge_Left",
    "QingshanTown_RoadEdge_Right",
    "QingshanTown_River",
    "QingshanTown_Bridge",
    "QingshanTown_Market",
    "QingshanTown_SouthWharf",
    PCG_ACTOR_LABEL,
) + tuple(f"{TREE_MARKER_LABEL_PREFIX}{index:02d}" for index in range(TREE_MARKER_COUNT))

_TEMPORARY_MESH = "/Engine/BasicShapes/Cube.Cube"
_PROTOTYPE_ONLY_TAG = "PrototypeOnly"
_FIXED_ANCHOR_TAG = "TownPCG_FixedAnchor"
_GAMEPLAY_LABEL_TOKENS = ("quest", "npc", "follower", "interaction")


def _name(value: str):
    return unreal.Name(value)


def _actor_label(actor) -> str:
    return str(actor.get_actor_label())


def _actor_class_path(actor) -> str:
    actor_class = actor.get_class()
    return str(actor_class.get_path_name() if hasattr(actor_class, "get_path_name") else actor_class)


def _all_actors() -> list:
    return list(unreal.EditorLevelLibrary.get_all_level_actors())


def _find_unique_actor_by_label(label: str, expected_class=None, required: bool = False):
    matches = [actor for actor in _all_actors() if _actor_label(actor) == label]
    if len(matches) > 1:
        raise RuntimeError(f"duplicate exact actor label {label!r}: found {len(matches)}")
    if not matches:
        if required:
            raise RuntimeError(f"required actor label is absent from prototype: {label}")
        return None
    actor = matches[0]
    if expected_class is not None and not isinstance(actor, expected_class):
        raise RuntimeError(
            f"actor {label!r} has wrong class {_actor_class_path(actor)}; "
            f"expected {expected_class}"
        )
    return actor


def _vector_payload(vector) -> list[float]:
    values = [float(vector.x), float(vector.y), float(vector.z)]
    if not all(math.isfinite(value) for value in values):
        raise RuntimeError(f"non-finite vector encountered: {values}")
    return values


def _transform_payload(actor) -> dict[str, Any]:
    transform = actor.get_actor_transform()
    rotation = actor.get_actor_rotation()
    return {
        "class": _actor_class_path(actor),
        "location": _vector_payload(transform.translation),
        "rotation": [float(rotation.roll), float(rotation.pitch), float(rotation.yaw)],
        "scale": _vector_payload(transform.scale3d),
    }


def _preserved_actor_labels() -> list[str]:
    labels = set(PRESERVED_LABELS)
    for actor in _all_actors():
        label = _actor_label(actor)
        lowered = label.lower()
        if any(token in lowered for token in _GAMEPLAY_LABEL_TOKENS):
            labels.add(label)
    return sorted(labels)


def _snapshot_preserved_actors(labels: Iterable[str] | None = None) -> dict[str, Any]:
    snapshot = {}
    for label in labels or _preserved_actor_labels():
        snapshot[label] = _transform_payload(
            _find_unique_actor_by_label(label, required=label in PRESERVED_LABELS)
        )
    return snapshot


def _set_tags(target, tags: Iterable[str]) -> None:
    unique = sorted({str(tag) for tag in tags})
    property_name = "component_tags" if isinstance(target, unreal.ActorComponent) else "tags"
    target.set_editor_property(property_name, [_name(tag) for tag in unique])


def _north_local_to_world(north_transform, offset) -> Any:
    if not all(math.isfinite(float(value)) for value in offset):
        raise RuntimeError(f"non-finite local layout offset: {offset}")
    return unreal.MathLibrary.transform_location(
        north_transform,
        unreal.Vector(float(offset[0]), float(offset[1]), float(offset[2])),
    )


def _create_or_update_spline_actor(
    label: str,
    north_transform,
    local_points: Iterable[tuple[float, float, float]],
    tags: Iterable[str],
    *,
    closed: bool = False,
):
    world_points = [
        _north_local_to_world(north_transform, local_point)
        for local_point in local_points
    ]
    _parse_operation_payload(
        "create_or_update_tagged_spline",
        unreal.GameXXKTownPCGAutomationLibrary.create_or_update_tagged_spline(
            label,
            world_points,
            bool(closed),
            [_name(tag) for tag in sorted({str(tag) for tag in tags})],
        ),
    )
    actor = _find_unique_actor_by_label(label, unreal.Actor, required=True)
    spline = actor.get_component_by_class(unreal.SplineComponent)
    if spline is None:
        raise RuntimeError(f"reflected helper created {label} without a SplineComponent")
    return actor


def _load_temporary_mesh():
    mesh = unreal.EditorAssetLibrary.load_asset(_TEMPORARY_MESH)
    if mesh is None:
        raise RuntimeError(f"could not load temporary engine mesh {_TEMPORARY_MESH}")
    return mesh


def _create_or_update_anchor_actor(
    label: str,
    north_transform,
    local_location: tuple[float, float, float],
    scale: tuple[float, float, float],
    extra_tags: Iterable[str] = (),
):
    actor = _find_unique_actor_by_label(label, unreal.StaticMeshActor)
    actor_existed = actor is not None
    world_location = _north_local_to_world(north_transform, local_location)
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, world_location, unreal.Rotator(0.0, 0.0, 0.0)
        )
        if actor is None:
            raise RuntimeError(f"failed to spawn StaticMeshActor for {label}")
        actor.set_actor_label(label)
    _require_actor_in_current_level(actor, label)
    if actor_existed:
        _require_existing_actor_tags(
            actor, label, (_PROTOTYPE_ONLY_TAG, _FIXED_ANCHOR_TAG)
        )
    actor.set_actor_location(world_location, False, False)
    actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
    actor.set_actor_scale3d(unreal.Vector(*[float(value) for value in scale]))
    component = actor.static_mesh_component
    component.set_static_mesh(_load_temporary_mesh())
    _set_tags(actor, (_PROTOTYPE_ONLY_TAG, _FIXED_ANCHOR_TAG, *extra_tags))
    _set_tags(component, (_PROTOTYPE_ONLY_TAG, _FIXED_ANCHOR_TAG, *extra_tags))
    return actor


def _load_tree_marker_mesh():
    for asset_path in TREE_MARKER_MESH_CANDIDATES:
        mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
        if mesh is not None:
            return mesh, asset_path
    raise RuntimeError(
        f"could not load a temporary tree marker mesh from {TREE_MARKER_MESH_CANDIDATES}"
    )


def _require_actor_in_current_level(actor, label: str) -> None:
    current_level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_current_level()
    if actor.get_level() != current_level:
        raise RuntimeError(f"managed actor {label!r} belongs to a different level")


def _require_existing_actor_tags(actor, label: str, required_tags: Iterable[str]) -> None:
    existing_tags = {str(tag) for tag in actor.get_editor_property("tags")}
    missing = sorted(set(required_tags) - existing_tags)
    if missing:
        raise RuntimeError(
            f"existing managed actor {label!r} is missing ownership tags {missing}"
        )


def _remove_obsolete_tree_marker_spline() -> bool:
    obsolete = _find_unique_actor_by_label("QingshanTown_TreeMarkers")
    if obsolete is None:
        return False
    _require_actor_in_current_level(obsolete, "QingshanTown_TreeMarkers")
    actor_tags = {str(tag) for tag in obsolete.get_editor_property("tags")}
    spline = obsolete.get_component_by_class(unreal.SplineComponent)
    if (
        _actor_class_path(obsolete) != "/Script/Engine.Actor"
        or spline is None
        or _PROTOTYPE_ONLY_TAG not in actor_tags
        or "TownPCG_TreeDebug" not in actor_tags
    ):
        raise RuntimeError(
            "obsolete QingshanTown_TreeMarkers actor does not match the managed prototype spline contract"
        )
    if not unreal.EditorLevelLibrary.destroy_actor(obsolete):
        raise RuntimeError("failed to remove obsolete managed tree-marker spline")
    return True


def _create_or_update_tree_marker_actor(
    label: str,
    north_transform,
    local_location: tuple[float, float, float],
    yaw_degrees: float,
    mesh,
):
    actor = _find_unique_actor_by_label(label, unreal.StaticMeshActor)
    actor_existed = actor is not None
    world_location = _north_local_to_world(north_transform, local_location)
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor,
            world_location,
            unreal.Rotator(pitch=0.0, yaw=float(yaw_degrees), roll=0.0),
        )
        if actor is None:
            raise RuntimeError(f"failed to spawn tree marker StaticMeshActor {label}")
        actor.set_actor_label(label)
    _require_actor_in_current_level(actor, label)
    if actor_existed:
        _require_existing_actor_tags(
            actor,
            label,
            (_PROTOTYPE_ONLY_TAG, "TownPCG_TreeDebug", "TownPCG_TreeMarker"),
        )
    actor.set_actor_location(world_location, False, False)
    actor.set_actor_rotation(
        unreal.Rotator(pitch=0.0, yaw=float(yaw_degrees), roll=0.0), False
    )
    actor.set_actor_scale3d(unreal.Vector(4.0, 4.0, 8.0))
    actor.static_mesh_component.set_static_mesh(mesh)
    tags = (_PROTOTYPE_ONLY_TAG, "TownPCG_TreeDebug", "TownPCG_TreeMarker")
    _set_tags(actor, tags)
    _set_tags(actor.static_mesh_component, tags)
    return actor


def _create_or_update_tree_markers(north_transform, instance_limit: int):
    if int(instance_limit) < TREE_MARKER_COUNT:
        raise RuntimeError(
            f"tree_instance_limit {instance_limit} is below required marker count {TREE_MARKER_COUNT}"
        )
    _remove_obsolete_tree_marker_spline()
    mesh, mesh_path = _load_tree_marker_mesh()
    points = (
        (-10500, -1000, 200), (10500, -1000, 200),
        (-11200, -4500, 200), (11200, -4500, 200),
        (-11200, -8500, 200), (11200, -8500, 200),
        (-10000, -14000, 200), (10000, -14000, 200),
        (-8000, -11500, 200), (-4000, -12300, 200),
        (4000, -12300, 200), (8000, -11500, 200),
    )
    actors = []
    for index, point in enumerate(points):
        label = f"{TREE_MARKER_LABEL_PREFIX}{index:02d}"
        actors.append(
            _create_or_update_tree_marker_actor(
                label, north_transform, point, index * 30.0, mesh
            )
        )
    return actors, len(actors), mesh_path


def _parse_operation_payload(operation: str, payload: str) -> dict[str, Any]:
    try:
        data = json.loads(str(payload))
    except (TypeError, ValueError) as error:
        raise RuntimeError(f"{operation} returned invalid JSON: {payload!r}") from error
    if data.get("success") is not True:
        raise RuntimeError(f"{operation} failed: {data}")
    return data


def _count_generated_building_instances(actor_label: str) -> int:
    actor = _find_unique_actor_by_label(actor_label, required=True)
    components = actor.get_components_by_class(unreal.InstancedStaticMeshComponent)
    return sum(int(component.get_instance_count()) for component in components)


def _dirty_map_package_names() -> list[str]:
    getter = getattr(unreal.EditorLoadingAndSavingUtils, "get_dirty_map_packages", None)
    if getter is None:
        raise RuntimeError("UE Python does not expose get_dirty_map_packages for source-map proof")
    return sorted(str(package.get_path_name()) for package in getter())


def _ensure_prototype_map() -> bool:
    if unreal.EditorAssetLibrary.does_asset_exist(PROTOTYPE_MAP):
        return False
    if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE_MAP):
        raise RuntimeError(f"source map does not exist: {SOURCE_MAP}")
    duplicate = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP, PROTOTYPE_MAP)
    if duplicate is None:
        raise RuntimeError(f"failed to duplicate {SOURCE_MAP} to {PROTOTYPE_MAP}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(duplicate, True):
        raise RuntimeError(f"failed to save duplicated prototype map {PROTOTYPE_MAP}")
    duplicate_package = unreal.EditorAssetLibrary.get_package_for_object(duplicate)
    if duplicate_package is None:
        raise RuntimeError(f"could not resolve duplicated map package {PROTOTYPE_MAP}")
    del duplicate
    if not unreal.EditorLoadingAndSavingUtils.unload_packages([duplicate_package]):
        raise RuntimeError(f"failed to unload duplicated map package {PROTOTYPE_MAP}")
    del duplicate_package
    unreal.SystemLibrary.collect_garbage()
    return True


def _require_graph_counts(payload: dict[str, Any]) -> None:
    expected = {"point_count": 12, "node_count": 2, "verified_edge_count": 2}
    for field, value in expected.items():
        if int(payload.get(field, -1)) != value:
            raise RuntimeError(f"graph {field} must be {value}, got {payload.get(field)!r}")


def _managed_label_counts() -> dict[str, int]:
    actors = _all_actors()
    return {
        label: sum(1 for actor in actors if _actor_label(actor) == label)
        for label in REQUIRED_MANAGED_LABELS
    }


def setup_vertical_slice() -> dict[str, Any]:
    config = load_config()
    if config["source_map"] != SOURCE_MAP or config["prototype_map"] != PROTOTYPE_MAP:
        raise RuntimeError("Task 2 map paths do not match Task 4's stable contract")
    if len(config["building_points"]) != 12:
        raise RuntimeError("vertical slice requires exactly 12 configured building points")

    dirty_before = _dirty_map_package_names()
    if SOURCE_MAP in dirty_before:
        raise RuntimeError(f"source map was already dirty before assembly: {SOURCE_MAP}")
    prototype_created = _ensure_prototype_map()
    if not unreal.EditorLoadingAndSavingUtils.load_map(PROTOTYPE_MAP):
        raise RuntimeError(f"failed to load prototype map {PROTOTYPE_MAP}")
    dirty_after_load = _dirty_map_package_names()
    if SOURCE_MAP in dirty_after_load:
        raise RuntimeError("source map became dirty during prototype creation/load")

    preserved_labels = _preserved_actor_labels()
    preserved_before = _snapshot_preserved_actors(preserved_labels)
    north_gate = _find_unique_actor_by_label(config["north_gate_label"], required=True)
    north_transform = north_gate.get_actor_transform()

    _create_or_update_spline_actor(
        "QingshanTown_CityScope", north_transform,
        ((-12000, 0, 0), (12000, 0, 0), (12000, -18000, 0), (-12000, -18000, 0)),
        ("Quick_Road_CityScope", _PROTOTYPE_ONLY_TAG, "Envelope_24000x18000cm"), closed=True,
    )
    _create_or_update_spline_actor(
        "QingshanTown_MainRoad", north_transform,
        ((0, 0, 0), (0, -5000, 0), (0, -10500, 0), (0, -16500, 0)),
        ("Quick_Road_MainRoad", _PROTOTYPE_ONLY_TAG, f"SemanticWidthCM_{config['main_road_width_cm']}"),
    )
    edge_offset = float(config["main_road_width_cm"]) * 0.5
    for label, x_offset in (
        ("QingshanTown_RoadEdge_Left", -edge_offset),
        ("QingshanTown_RoadEdge_Right", edge_offset),
    ):
        _create_or_update_spline_actor(
            label, north_transform,
            ((x_offset, 0, 0), (x_offset, -5000, 0), (x_offset, -10500, 0), (x_offset, -16500, 0)),
            ("Quick_Road_RoadEdge", _PROTOTYPE_ONLY_TAG),
        )
    _create_or_update_spline_actor(
        "QingshanTown_River", north_transform,
        ((-12000, -12000, 0), (-4000, -11800, 0), (4000, -12200, 0), (12000, -12000, 0)),
        ("TownPCG_River", _PROTOTYPE_ONLY_TAG, f"SemanticWidthCM_{config['river_width_cm']}"),
    )
    _create_or_update_anchor_actor(
        "QingshanTown_Bridge", north_transform, (0, -12000, 50),
        (config["bridge_width_cm"] / 100.0, 24.0, 1.0), (f"SemanticWidthCM_{config['bridge_width_cm']}",),
    )
    _create_or_update_anchor_actor(
        "QingshanTown_Market", north_transform, (-5000, -6500, 100), (28.0, 18.0, 2.0)
    )
    _create_or_update_anchor_actor(
        "QingshanTown_SouthWharf", north_transform, (4000, -15000, 100), (30.0, 18.0, 2.0)
    )
    _, tree_marker_count, tree_marker_mesh = _create_or_update_tree_markers(
        north_transform, config["tree_instance_limit"]
    )

    building_transforms = []
    for point in config["building_points"]:
        world_location = _north_local_to_world(north_transform, point["location_cm"])
        north_yaw = float(north_gate.get_actor_rotation().yaw)
        building_transforms.append(
            unreal.Transform(
                rotation=unreal.Rotator(
                    pitch=0.0,
                    yaw=north_yaw + float(point["yaw_degrees"]),
                    roll=0.0,
                ),
                location=world_location,
                scale=unreal.Vector(1.0, 1.0, 1.0),
            )
        )

    library = unreal.GameXXKTownPCGAutomationLibrary
    graph = _parse_operation_payload(
        "create_or_update_town_pcg_graph",
        library.create_or_update_town_pcg_graph(
            config["graph_path"], config["building_mesh"], building_transforms
        ),
    )
    _require_graph_counts(graph)
    layout_center = _north_local_to_world(north_transform, (0, -9000, 2500))
    attach = _parse_operation_payload(
        "attach_town_pcg_graph",
        library.attach_town_pcg_graph(
            PCG_ACTOR_LABEL,
            config["graph_path"],
            layout_center,
            unreal.Vector(12500.0, 9500.0, 3000.0),
        ),
    )
    clear = _parse_operation_payload(
        "clear_town_pcg", library.clear_town_pcg(PCG_ACTOR_LABEL)
    )
    preserved_after = _snapshot_preserved_actors(preserved_labels)
    if preserved_after != preserved_before:
        raise RuntimeError(
            f"preserved gameplay actors changed during assembly: before={preserved_before}, after={preserved_after}"
        )
    label_counts = _managed_label_counts()
    duplicates = {label: count for label, count in label_counts.items() if count != 1}
    if duplicates:
        raise RuntimeError(f"managed label uniqueness failed: {duplicates}")
    if SOURCE_MAP in _dirty_map_package_names():
        raise RuntimeError("source map is dirty immediately before prototype save")
    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError(f"failed to save prototype map {PROTOTYPE_MAP}")
    dirty_after_layout_save = _dirty_map_package_names()
    if SOURCE_MAP in dirty_after_layout_save:
        raise RuntimeError("source map became dirty or was saved by assembly")
    generate = _parse_operation_payload(
        "generate_town_pcg", library.generate_town_pcg(PCG_ACTOR_LABEL)
    )

    return {
        "success": True,
        "phase": PHASE_SETUP,
        "complete": False,
        "source_map": SOURCE_MAP,
        "prototype_map": PROTOTYPE_MAP,
        "prototype_created": prototype_created,
        "graph_path": config["graph_path"],
        "pcg_actor_label": PCG_ACTOR_LABEL,
        "building_point_count": len(building_transforms),
        "generated_component_count": int(generate.get("generated_component_count", 0)),
        "tree_marker_count": tree_marker_count,
        "tree_marker_mesh": tree_marker_mesh,
        "managed_label_counts": label_counts,
        "preserved_before": preserved_before,
        "preserved_after": preserved_after,
        "graph": graph,
        "attach": attach,
        "clear": clear,
        "generate": generate,
        "dirty_map_packages_before": dirty_before,
        "dirty_map_packages_after_layout_save": dirty_after_layout_save,
        "source_map_dirty_or_saved": False,
    }


def _current_map_package() -> str:
    world = unreal.EditorLevelLibrary.get_editor_world()
    return str(world.get_outermost().get_path_name()) if world is not None else ""


def finalize_vertical_slice() -> dict[str, Any]:
    if _current_map_package() != PROTOTYPE_MAP:
        if not unreal.EditorLoadingAndSavingUtils.load_map(PROTOTYPE_MAP):
            raise RuntimeError(f"failed to load prototype map {PROTOTYPE_MAP}")
    if SOURCE_MAP in _dirty_map_package_names():
        raise RuntimeError("source map is dirty during PCG finalize")

    preserved_labels = _preserved_actor_labels()
    preserved_before = _snapshot_preserved_actors(preserved_labels)
    status = _parse_operation_payload(
        "get_town_pcg_status",
        unreal.GameXXKTownPCGAutomationLibrary.get_town_pcg_status(PCG_ACTOR_LABEL),
    )
    common = {
        "phase": PHASE_FINALIZE,
        "prototype_map": PROTOTYPE_MAP,
        "graph_path": load_config()["graph_path"],
        "pcg_actor_label": PCG_ACTOR_LABEL,
        "status": status,
        "source_map_dirty_or_saved": False,
    }
    if status.get("generating") is True:
        return {"success": True, "complete": False, "pending": True, **common}
    if status.get("generated") is not True:
        return {
            "success": False,
            "complete": False,
            "pending": False,
            "error": f"PCG generation stopped without completing: {status}",
            **common,
        }

    generated_instance_count = _count_generated_building_instances(PCG_ACTOR_LABEL)
    if generated_instance_count != 12:
        raise RuntimeError(
            f"PCG generation must produce 12 public ISM instances, got {generated_instance_count}"
        )
    preserved_after = _snapshot_preserved_actors(preserved_labels)
    if preserved_after != preserved_before:
        raise RuntimeError("preserved gameplay actors changed during PCG finalize")
    label_counts = _managed_label_counts()
    duplicates = {label: count for label, count in label_counts.items() if count != 1}
    if duplicates:
        raise RuntimeError(f"managed label uniqueness failed during finalize: {duplicates}")
    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError(f"failed to save finalized prototype map {PROTOTYPE_MAP}")
    dirty_after_save = _dirty_map_package_names()
    if SOURCE_MAP in dirty_after_save:
        raise RuntimeError("source map became dirty or was saved by finalize")
    return {
        "success": True,
        "complete": True,
        "pending": False,
        "generated_component_count": int(status.get("generated_component_count", 0)),
        "generated_instance_count": generated_instance_count,
        "managed_label_counts": label_counts,
        "preserved_before": preserved_before,
        "preserved_after": preserved_after,
        "dirty_map_packages_after": dirty_after_save,
        **common,
    }


def assemble_vertical_slice(phase: str = PHASE_SETUP) -> dict[str, Any]:
    if phase == PHASE_SETUP:
        return setup_vertical_slice()
    if phase == PHASE_FINALIZE:
        return finalize_vertical_slice()
    raise ValueError(f"unsupported phase {phase!r}; expected setup or finalize")


def _phase_from_argv(argv: list[str]) -> str:
    if "--phase" not in argv:
        return PHASE_SETUP
    index = argv.index("--phase")
    if index + 1 >= len(argv):
        raise ValueError("--phase requires setup or finalize")
    return argv[index + 1]


def main() -> None:
    try:
        print(json.dumps(assemble_vertical_slice(_phase_from_argv(sys.argv[1:])), sort_keys=True))
    except Exception as error:
        unreal.log_error(f"GameXXK Qingshan town PCG assembly failed: {error}")
        print(json.dumps({"success": False, "error": str(error)}, sort_keys=True))
        raise


if __name__ == "__main__":
    main()
