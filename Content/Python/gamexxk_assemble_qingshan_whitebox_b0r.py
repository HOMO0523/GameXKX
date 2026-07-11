"""Idempotently assemble the isolated Qingshan B0R PCG whitebox map."""

from __future__ import annotations

import json
import math
from pathlib import Path
import sys
from typing import Any, Iterable

import unreal

from gamexxk_qingshan_whitebox_config import load_config

# Task 3 is deliberately host-safe and lives beside the production harnesses.
_PROJECT_ROOT = Path(__file__).resolve().parents[2]
_SCRIPTS_DIR = _PROJECT_ROOT / "scripts"
if str(_SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPTS_DIR))
from qingshan_whitebox_acceptance import (
    canonical_layout_hash,
    generate_seeded_proxy_transforms,
)


SOURCE_MAP = "/Game/GameXXK/Maps/L_QingshanInn"
WHITEBOX_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R"
B0R_CONTENT_ROOT = "/Game/GameXXK/Environment/TownPCG/B0R/"
MANAGED_TAG = "QingshanB0RManaged"
PHASE_SETUP = "setup"
PHASE_FINALIZE = "finalize"

ROAD_LABELS = {
    "Road_Main": "QS_B0R_Road_Main",
    "Road_Core_North": "QS_B0R_Road_Core_North",
    "Road_Core_South": "QS_B0R_Road_Core_South",
}
ROAD_SEMANTIC_TAGS = {
    # These are three road centerlines, not left/right road-edge splines.
    "Road_Main": "Quick_Road_MainRoad",
    "Road_Core_North": "Quick_Road_MainRoad",
    "Road_Core_South": "Quick_Road_MainRoad",
}
RIVER_LABELS = {"River_Main": "QS_B0R_River_Main"}
TERRAIN_LABELS = {
    "Terrain_Base": "QS_B0R_Terrain_Base",
    "Terrain_CorePlateau": "QS_B0R_Terrain_CorePlateau",
    "Terrain_SouthSlope": "QS_B0R_Terrain_SouthSlope",
    "Terrain_DockLowland": "QS_B0R_Terrain_DockLowland",
}
PCG_LABELS = {
    "buildings": "QS_B0R_PCG_Buildings",
    "foliage": "QS_B0R_PCG_Foliage",
    "mountains": "QS_B0R_PCG_Mountains",
}
CAMERA_LABELS = (
    "CAM_QS_GATE_ARRIVAL",
    "CAM_QS_TOWN_CORE",
    "CAM_QS_MAIN_BRIDGE",
    "CAM_QS_SOUTH_DOCK",
)
GRAPH_PATHS = {
    "buildings": "/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_Buildings",
    "foliage": "/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_Foliage",
    "mountains": "/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_Mountains",
}

WORLD_BOUNDS_LABEL = "QS_B0R_World_Bounds"
BRIDGE_LABEL = "QS_B0R_Bridge_Main"
DOCK_LABEL = "QS_B0R_Dock_South"
PLAYER_START_CLASS = unreal.PlayerStart
CUBE_MESH = "/Engine/BasicShapes/Cube"
CONE_MESH = "/Engine/BasicShapes/Cone"
EXPECTED_COUNTS = {"buildings": 16, "foliage": 100, "mountains": 24}
_GAMEPLAY_LABEL_TOKENS = ("quest", "npc", "follower", "interaction")


def _name(value: str):
    return unreal.Name(value)


def _all_actors() -> list:
    return list(unreal.EditorLevelLibrary.get_all_level_actors())


def _actor_label(actor) -> str:
    return str(actor.get_actor_label())


def _actor_class_path(actor) -> str:
    actor_class = actor.get_class()
    return str(actor_class.get_path_name() if hasattr(actor_class, "get_path_name") else actor_class)


def _find_unique_actor_by_label(label: str, expected_class=None, required: bool = False):
    matches = [actor for actor in _all_actors() if _actor_label(actor) == label]
    if len(matches) > 1:
        raise RuntimeError(f"duplicate exact actor label {label!r}: found {len(matches)}")
    if not matches:
        if required:
            raise RuntimeError(f"required actor label is absent: {label}")
        return None
    actor = matches[0]
    if expected_class is not None and not isinstance(actor, expected_class):
        raise RuntimeError(
            f"actor {label!r} has class {_actor_class_path(actor)}, expected {expected_class}"
        )
    return actor


def _actor_tags(actor) -> set[str]:
    return {str(tag) for tag in actor.get_editor_property("tags")}


def _set_tags(target, tags: Iterable[str]) -> None:
    property_name = "component_tags" if isinstance(target, unreal.ActorComponent) else "tags"
    target.set_editor_property(property_name, [_name(tag) for tag in sorted(set(tags))])


def _require_managed_actor(actor, label: str) -> None:
    if MANAGED_TAG not in _actor_tags(actor):
        raise RuntimeError(
            f"refusing to edit unowned actor {label!r}; missing tag {MANAGED_TAG}"
        )
    current_level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_current_level()
    if actor.get_level() != current_level:
        raise RuntimeError(f"managed actor {label!r} belongs to another level")


def _vector_payload(vector) -> list[float]:
    values = [float(vector.x), float(vector.y), float(vector.z)]
    if not all(math.isfinite(value) for value in values):
        raise RuntimeError(f"non-finite vector: {values}")
    return values


def _transform_payload(actor) -> dict[str, Any]:
    transform = actor.get_actor_transform()
    rotation = actor.get_actor_rotation()
    rotation_values = [float(rotation.roll), float(rotation.pitch), float(rotation.yaw)]
    if not all(math.isfinite(value) for value in rotation_values):
        raise RuntimeError(f"non-finite rotation: {rotation_values}")
    return {
        "class": _actor_class_path(actor),
        "location": _vector_payload(transform.translation),
        "rotation": rotation_values,
        "scale": _vector_payload(transform.scale3d),
    }


def _preserved_actor_labels(gate_label: str) -> list[str]:
    labels = {gate_label}
    player_starts = [actor for actor in _all_actors() if isinstance(actor, PLAYER_START_CLASS)]
    if not player_starts:
        raise RuntimeError("whitebox source contains no PlayerStart")
    labels.update(_actor_label(actor) for actor in player_starts)
    for actor in _all_actors():
        label = _actor_label(actor)
        if any(token in label.lower() for token in _GAMEPLAY_LABEL_TOKENS):
            labels.add(label)
    return sorted(labels)


def _snapshot_preserved_actors(labels: Iterable[str]) -> dict[str, Any]:
    return {
        label: _transform_payload(_find_unique_actor_by_label(label, required=True))
        for label in labels
    }


def _assert_preserved(before: dict[str, Any], labels: Iterable[str]) -> dict[str, Any]:
    after = _snapshot_preserved_actors(labels)
    if after != before:
        raise RuntimeError(f"preserved gameplay actors changed: before={before}, after={after}")
    return after


def _north_local_to_world(north_transform, offset) -> Any:
    values = tuple(float(value) for value in offset)
    if len(values) != 3 or not all(math.isfinite(value) for value in values):
        raise RuntimeError(f"invalid north-local offset: {offset!r}")
    return unreal.MathLibrary.transform_location(north_transform, unreal.Vector(*values))


def _north_local_yaw(north_transform, local_yaw: float) -> float:
    yaw = float(north_transform.rotation.rotator().yaw) + float(local_yaw)
    if not math.isfinite(yaw):
        raise RuntimeError(f"non-finite north-local yaw: {local_yaw!r}")
    return yaw


def _validate_north_anchor(actor, tolerance: float = 1.0e-3):
    transform = actor.get_actor_transform()
    rotation = actor.get_actor_rotation()
    location = (
        float(transform.translation.x),
        float(transform.translation.y),
        float(transform.translation.z),
    )
    rotation_values = (
        float(rotation.pitch), float(rotation.yaw), float(rotation.roll),
    )
    scale = (
        float(transform.scale3d.x),
        float(transform.scale3d.y),
        float(transform.scale3d.z),
    )
    if not all(math.isfinite(value) for value in (*location, *rotation_values, *scale)):
        raise RuntimeError("north gate anchor transform must contain only finite values")
    if abs(rotation_values[0]) > tolerance or abs(rotation_values[2]) > tolerance:
        raise RuntimeError(
            "north gate anchor must be upright within pitch/roll tolerance; "
            f"rotation={rotation_values}, tolerance={tolerance}"
        )
    if any(abs(value - 1.0) > tolerance for value in scale):
        raise RuntimeError(
            "north gate anchor must have unit scale; "
            f"scale={scale}, tolerance={tolerance}"
        )
    return transform


def _parse_operation_payload(operation: str, payload: str) -> dict[str, Any]:
    try:
        data = json.loads(str(payload))
    except (TypeError, ValueError) as error:
        raise RuntimeError(f"{operation} returned invalid JSON: {payload!r}") from error
    if not isinstance(data, dict) or data.get("success") is not True:
        raise RuntimeError(f"{operation} failed: {data}")
    return data


def _dirty_map_package_names() -> list[str]:
    getter = getattr(unreal.EditorLoadingAndSavingUtils, "get_dirty_map_packages", None)
    if getter is None:
        raise RuntimeError("UE Python does not expose get_dirty_map_packages")
    return sorted(str(package.get_path_name()).split(".", 1)[0] for package in getter())


def _dirty_content_package_names() -> list[str]:
    getter = getattr(unreal.EditorLoadingAndSavingUtils, "get_dirty_content_packages", None)
    if getter is None:
        raise RuntimeError("UE Python does not expose get_dirty_content_packages")
    return sorted(str(package.get_path_name()).split(".", 1)[0] for package in getter())


def _require_editor_clean(context: str) -> tuple[list[str], list[str]]:
    maps, content = _dirty_map_package_names(), _dirty_content_package_names()
    if maps or content:
        # _ensure_whitebox_map refuses map/content dirtiness before duplication.
        raise RuntimeError(f"{context} refuses map/content dirtiness: maps={maps}, content={content}")
    return maps, content


def _validate_finalize_dirty_state(
    current_map: str, dirty_maps, dirty_content,
) -> None:
    dirty_maps = set(dirty_maps)
    dirty_content = set(dirty_content)
    if SOURCE_MAP in dirty_maps or SOURCE_MAP in dirty_content:
        raise RuntimeError(f"source map is dirty before finalize map load: {SOURCE_MAP}")
    if current_map != WHITEBOX_MAP:
        if dirty_maps or dirty_content:
            raise RuntimeError(
                "whitebox finalize refuses all dirtiness before switching maps: "
                f"maps={sorted(dirty_maps)}, content={sorted(dirty_content)}"
            )
        return

    unexpected_maps = dirty_maps - {WHITEBOX_MAP}
    unexpected_content = {
        package for package in dirty_content
        if not package.startswith(B0R_CONTENT_ROOT)
    }
    if unexpected_maps or unexpected_content:
        raise RuntimeError(
            "whitebox finalize refuses unrelated map/content dirtiness: "
            f"maps={sorted(unexpected_maps)}, content={sorted(unexpected_content)}"
        )


def _require_finalize_clean() -> tuple[list[str], list[str]]:
    dirty_maps = _dirty_map_package_names()
    dirty_content = _dirty_content_package_names()
    _validate_finalize_dirty_state(_current_map_package(), dirty_maps, dirty_content)
    return dirty_maps, dirty_content


def _current_map_package() -> str:
    world = unreal.EditorLevelLibrary.get_editor_world()
    return str(world.get_outermost().get_path_name()).split(".", 1)[0] if world else ""


def _ensure_whitebox_map() -> bool:
    if unreal.EditorAssetLibrary.does_asset_exist(WHITEBOX_MAP):
        return False
    _require_editor_clean("whitebox duplication")
    if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE_MAP):
        raise RuntimeError(f"source map does not exist: {SOURCE_MAP}")
    duplicate = unreal.EditorAssetLibrary.duplicate_asset(SOURCE_MAP, WHITEBOX_MAP)
    if duplicate is None:
        raise RuntimeError(f"failed to duplicate {SOURCE_MAP} to {WHITEBOX_MAP}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(duplicate, True):
        raise RuntimeError(f"failed to save duplicated whitebox map {WHITEBOX_MAP}")
    duplicate_package = unreal.EditorAssetLibrary.get_package_for_object(duplicate)
    if duplicate_package is None:
        raise RuntimeError(f"could not resolve duplicated whitebox package {WHITEBOX_MAP}")
    del duplicate
    b_out_any_packages_unloaded, out_error_message = (
        unreal.EditorLoadingAndSavingUtils.unload_packages([duplicate_package])
    )
    if not b_out_any_packages_unloaded:
        raise RuntimeError(
            f"failed to unload duplicated whitebox package {WHITEBOX_MAP}: "
            f"{out_error_message}"
        )
    del duplicate_package
    unreal.SystemLibrary.collect_garbage()
    return True


def _load_whitebox_only() -> None:
    if _current_map_package() != WHITEBOX_MAP:
        if not unreal.EditorLoadingAndSavingUtils.load_map(WHITEBOX_MAP):
            raise RuntimeError(f"failed to load whitebox map {WHITEBOX_MAP}")
    if _current_map_package() != WHITEBOX_MAP:
        raise RuntimeError("automation may mutate only the B0R whitebox map")
    if SOURCE_MAP in _dirty_map_package_names():
        raise RuntimeError(f"source map is dirty: {SOURCE_MAP}")


def _load_mesh(package_path: str):
    asset = unreal.EditorAssetLibrary.load_asset(f"{package_path}.{package_path.rsplit('/', 1)[-1]}")
    if asset is None:
        raise RuntimeError(f"could not load proxy mesh {package_path}")
    return asset


def _create_or_update_managed_mesh_actor(
    label: str, mesh, location, rotation, scale, extra_tags: Iterable[str] = (),
):
    actor = _find_unique_actor_by_label(label, unreal.StaticMeshActor)
    if actor is not None:
        _require_managed_actor(actor, label)
    else:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, location, rotation
        )
        if actor is None:
            raise RuntimeError(f"failed to spawn managed mesh actor {label}")
        actor.set_actor_label(label)
    actor.set_actor_location(location, False, False)
    actor.set_actor_rotation(rotation, False)
    actor.set_actor_scale3d(unreal.Vector(*[float(value) for value in scale]))
    actor.static_mesh_component.set_static_mesh(mesh)
    tags = (MANAGED_TAG, *extra_tags)
    _set_tags(actor, tags)
    _set_tags(actor.static_mesh_component, tags)
    return actor


def _create_or_update_managed_spline_actor(
    label: str, north_transform, local_points, extra_tags: Iterable[str], closed: bool = False,
):
    existing = _find_unique_actor_by_label(label)
    if existing is not None:
        _require_managed_actor(existing, label)
    world_points = [_north_local_to_world(north_transform, point) for point in local_points]
    _parse_operation_payload(
        "create_or_update_tagged_spline",
        unreal.GameXXKTownPCGAutomationLibrary.create_or_update_tagged_spline(
            label, world_points, bool(closed),
            [_name(tag) for tag in sorted({MANAGED_TAG, *extra_tags})],
        ),
    )
    actor = _find_unique_actor_by_label(label, unreal.Actor, required=True)
    if MANAGED_TAG not in _actor_tags(actor):
        raise RuntimeError(f"reflected spline helper omitted managed tag for {label}")
    return actor


def _build_curve_plate_specs(world_points, width_cm: float, thickness_cm: float) -> list:
    width_cm = float(width_cm)
    thickness_cm = float(thickness_cm)
    if not math.isfinite(width_cm) or width_cm <= 0.0:
        raise RuntimeError(f"curve proxy width must be positive and finite: {width_cm}")
    if not math.isfinite(thickness_cm) or thickness_cm <= 0.0:
        raise RuntimeError(f"curve proxy thickness must be positive and finite: {thickness_cm}")

    def components(point):
        if all(hasattr(point, name) for name in ("x", "y", "z")):
            values = (float(point.x), float(point.y), float(point.z))
        else:
            values = tuple(float(point[index]) for index in range(3))
        if not all(math.isfinite(value) for value in values):
            raise RuntimeError(f"curve proxy point must be finite: {values}")
        return values

    points = [components(point) for point in world_points]
    if len(points) < 2:
        raise RuntimeError("curve proxy requires at least two world points")
    specs = []
    for start, end in zip(points, points[1:]):
        delta_x, delta_y = end[0] - start[0], end[1] - start[1]
        length_cm = math.hypot(delta_x, delta_y)
        if length_cm <= 0.0:
            raise RuntimeError(f"curve proxy has a zero-length horizontal segment: {start} -> {end}")
        specs.append({
            "location_cm": [
                (start[0] + end[0]) / 2.0,
                (start[1] + end[1]) / 2.0,
                (start[2] + end[2]) / 2.0 - thickness_cm / 2.0,
            ],
            "length_cm": length_cm,
            "width_cm": width_cm,
            "thickness_cm": thickness_cm,
            "yaw_degrees": math.degrees(math.atan2(delta_y, delta_x)),
        })
    return specs


def _create_curve_plates(config, north_transform, cube_mesh) -> int:
    count = 0
    for category, splines, thickness in (
        ("Road", config["road_splines"], 18.0),
        ("River", config["river_splines"], 12.0),
    ):
        for spline in splines:
            world_points = [
                _north_local_to_world(north_transform, point)
                for point in spline["points_cm"]
            ]
            specs = _build_curve_plate_specs(world_points, spline["width_cm"], thickness)
            for index, spec in enumerate(specs):
                label = f"QS_B0R_{category}Plate_{spline['id']}_{index:03d}"
                _create_or_update_managed_mesh_actor(
                    label, cube_mesh, unreal.Vector(*spec["location_cm"]),
                    unreal.Rotator(
                        pitch=0.0,
                        yaw=spec["yaw_degrees"],
                        roll=0.0,
                    ),
                    (
                        spec["length_cm"] / 100.0,
                        spec["width_cm"] / 100.0,
                        spec["thickness_cm"] / 100.0,
                    ),
                    (f"QingshanB0R{category}Proxy",),
                )
                count += 1
    return count


def _pcg_transform(north_transform, record, *, z_offset: float = 0.0):
    local = list(record["location_cm"])
    local[2] += z_offset
    location = _north_local_to_world(north_transform, local)
    rotation = record["rotation_degrees"]
    return unreal.Transform(
        rotation=unreal.Rotator(
            pitch=float(rotation[1]),
            yaw=_north_local_yaw(north_transform, float(rotation[2])),
            roll=float(rotation[0]),
        ),
        location=location,
        scale=unreal.Vector(*[float(value) for value in record["scale"]]),
    )


def _building_transforms(config, north_transform) -> list:
    result = []
    for plot in config["building_plots"]:
        size = plot["size_cm"]
        record = {
            "location_cm": plot["location_cm"],
            "rotation_degrees": [0.0, 0.0, plot["yaw_degrees"]],
            "scale": [float(value) / 100.0 for value in size],
        }
        result.append(_pcg_transform(north_transform, record, z_offset=float(size[2]) / 2.0))
    return result


def _create_or_update_camera(label: str, spec, north_transform) -> dict[str, Any]:
    actor = _find_unique_actor_by_label(label, unreal.CameraActor)
    if actor is not None:
        _require_managed_actor(actor, label)
    location = _north_local_to_world(north_transform, spec["location_cm"])
    target = _north_local_to_world(north_transform, spec["target_cm"])
    rotation = unreal.MathLibrary.find_look_at_rotation(location, target)
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.CameraActor, location, rotation)
        if actor is None:
            raise RuntimeError(f"failed to spawn camera {label}")
        actor.set_actor_label(label)
    actor.set_actor_location(location, False, False)
    actor.set_actor_rotation(rotation, False)
    _set_tags(actor, (MANAGED_TAG, "QingshanB0RCamera"))
    component = actor.get_camera_component()
    component.set_editor_property("field_of_view", float(spec["fov_degrees"]))
    return {
        "label": label,
        "location": _vector_payload(location),
        "target": _vector_payload(target),
        "rotation": [float(rotation.roll), float(rotation.pitch), float(rotation.yaw)],
        "field_of_view": float(spec["fov_degrees"]),
    }


def _required_label_counts() -> dict[str, int]:
    required = (
        WORLD_BOUNDS_LABEL, *ROAD_LABELS.values(), *RIVER_LABELS.values(),
        BRIDGE_LABEL, DOCK_LABEL, *TERRAIN_LABELS.values(), *PCG_LABELS.values(),
        *CAMERA_LABELS,
    )
    actors = _all_actors()
    return {label: sum(_actor_label(actor) == label for actor in actors) for label in required}


def _require_unique_managed_labels() -> dict[str, int]:
    counts = _required_label_counts()
    invalid = {label: count for label, count in counts.items() if count != 1}
    if invalid:
        raise RuntimeError(f"managed-label uniqueness failed: {invalid}")
    return counts


def _managed_category_count(category_tag: str) -> int:
    return sum(
        MANAGED_TAG in _actor_tags(actor) and category_tag in _actor_tags(actor)
        for actor in _all_actors()
    )


def _require_exact_camera_and_terrain_counts() -> None:
    camera_count = _managed_category_count("QingshanB0RCamera")
    terrain_count = _managed_category_count("QingshanB0RTerrainProxy")
    if camera_count != 4 or terrain_count != 4:
        raise RuntimeError(
            "whitebox requires exactly four camera and terrain actors; "
            f"cameras={camera_count}, terrain={terrain_count}"
        )


def _author_graphs(config, north_transform, layout) -> dict[str, Any]:
    buildings = _building_transforms(config, north_transform)
    foliage = [_pcg_transform(north_transform, record) for record in layout["foliage"]]
    mountains = [
        _pcg_transform(
            north_transform,
            record,
            z_offset=abs(float(record["scale"][2])) * 50.0,
        )
        for record in layout["mountains"]
    ]
    transforms = {"buildings": buildings, "foliage": foliage, "mountains": mountains}
    meshes = {"buildings": CUBE_MESH, "foliage": CONE_MESH, "mountains": CUBE_MESH}
    world = config["world_bounds_cm"]
    center_local = ((world[0] + world[1]) / 2, (world[2] + world[3]) / 2, 2500.0)
    center = _north_local_to_world(north_transform, center_local)
    extent = unreal.Vector((world[1] - world[0]) / 2, (world[3] - world[2]) / 2, 5000.0)
    result = {}
    library = unreal.GameXXKTownPCGAutomationLibrary
    for key in ("buildings", "foliage", "mountains"):
        if len(transforms[key]) != EXPECTED_COUNTS[key]:
            raise RuntimeError(f"{key} transform count must be {EXPECTED_COUNTS[key]}")
        existing = _find_unique_actor_by_label(PCG_LABELS[key])
        if existing is not None:
            _require_managed_actor(existing, PCG_LABELS[key])
        graph = _parse_operation_payload(
            "create_or_update_town_pcg_graph",
            library.create_or_update_town_pcg_graph(GRAPH_PATHS[key], meshes[key], transforms[key]),
        )
        attach = _parse_operation_payload(
            "attach_town_pcg_graph",
            library.attach_town_pcg_graph(PCG_LABELS[key], GRAPH_PATHS[key], center, extent),
        )
        volume = _find_unique_actor_by_label(PCG_LABELS[key], required=True)
        _set_tags(volume, (MANAGED_TAG, f"QingshanB0RPCG{key.title()}"))
        clear = _parse_operation_payload("clear_town_pcg", library.clear_town_pcg(PCG_LABELS[key]))
        result[key] = {"graph": graph, "attach": attach, "clear": clear}
    return result


def setup_whitebox() -> dict[str, Any]:
    config = load_config()
    if config["source_map"] != SOURCE_MAP or config["whitebox_map"] != WHITEBOX_MAP:
        raise RuntimeError("B0R config map paths do not match assembler constants")
    if len(config["road_splines"]) != 3 or len(config["river_splines"]) != 1:
        raise RuntimeError("B0R whitebox requires exactly three road splines and one river spline")
    if len(config["terrain_zones"]) != 4 or len(config["cameras"]) != 4:
        raise RuntimeError("B0R whitebox requires exactly four terrain zones and four cameras")
    dirty_maps_before, dirty_content_before = _require_editor_clean("whitebox setup")
    created = _ensure_whitebox_map()
    _load_whitebox_only()

    gate_label = config["coordinate_reference"]["actor_label"]
    preserved_labels = _preserved_actor_labels(gate_label)
    preserved_before = _snapshot_preserved_actors(preserved_labels)
    north_gate = _find_unique_actor_by_label(gate_label, required=True)
    north_transform = _validate_north_anchor(north_gate)
    cube_mesh = _load_mesh(CUBE_MESH)

    bounds = config["world_bounds_cm"]
    _create_or_update_managed_spline_actor(
        WORLD_BOUNDS_LABEL, north_transform,
        ((bounds[0], bounds[2], 0), (bounds[1], bounds[2], 0),
         (bounds[1], bounds[3], 0), (bounds[0], bounds[3], 0)),
        ("PrototypeOnly", "Quick_Road_CityScope"), closed=True,
    )
    for spline in config["road_splines"]:
        semantic_tag = ROAD_SEMANTIC_TAGS[spline["id"]]
        _create_or_update_managed_spline_actor(
            ROAD_LABELS[spline["id"]], north_transform, spline["points_cm"],
            (
                "PrototypeOnly", semantic_tag, "Quick_Road_LayoutInput",
                f"SemanticWidthCM_{spline['width_cm']}",
            ),
        )
    for spline in config["river_splines"]:
        _create_or_update_managed_spline_actor(
            RIVER_LABELS[spline["id"]], north_transform, spline["points_cm"],
            ("PrototypeOnly", "TownPCG_River", f"SemanticWidthCM_{spline['width_cm']}"),
        )

    anchor_by_id = {item["id"]: item for item in config["fixed_anchors"]}
    for label, anchor_id, size in (
        (BRIDGE_LABEL, "MainBridgeAnchor", (900.0, 2400.0, 120.0)),
        (DOCK_LABEL, "SouthDockAnchor", (1800.0, 1200.0, 100.0)),
    ):
        anchor = anchor_by_id[anchor_id]
        height = size[2]
        location = list(anchor["location_cm"])
        location[2] += height / 2.0
        _create_or_update_managed_mesh_actor(
            label, cube_mesh, _north_local_to_world(north_transform, location),
            unreal.Rotator(
                pitch=0.0,
                yaw=_north_local_yaw(north_transform, float(anchor["yaw_degrees"])),
                roll=0.0,
            ),
            tuple(value / 100.0 for value in size), ("QingshanB0RCrossing",),
        )
    for zone in config["terrain_zones"]:
        size = zone["size_cm"]
        center = list(zone["center_cm"])
        center[2] -= float(size[2]) / 2.0
        _create_or_update_managed_mesh_actor(
            TERRAIN_LABELS[zone["id"]], cube_mesh,
            _north_local_to_world(north_transform, center),
            unreal.Rotator(
                pitch=0.0,
                yaw=_north_local_yaw(north_transform, float(zone["yaw_degrees"])),
                roll=0.0,
            ),
            tuple(float(value) / 100.0 for value in size),
            ("QingshanB0RTerrainProxy", f"TopElevationCM_{zone['center_cm'][2]}"),
        )
    curve_plate_count = _create_curve_plates(config, north_transform, cube_mesh)

    layout = generate_seeded_proxy_transforms(config)
    layout_evidence = canonical_layout_hash(layout)
    graphs = _author_graphs(config, north_transform, layout)
    cameras = {
        label: _create_or_update_camera(label, config["cameras"][label], north_transform)
        for label in CAMERA_LABELS
    }
    preserved_after = _assert_preserved(preserved_before, preserved_labels)
    label_counts = _require_unique_managed_labels()
    _require_exact_camera_and_terrain_counts()
    if SOURCE_MAP in _dirty_map_package_names():
        raise RuntimeError("source map is dirty immediately before whitebox save")
    if _current_map_package() != WHITEBOX_MAP or not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError(f"failed to save only whitebox map {WHITEBOX_MAP}")
    dirty_maps_after = _dirty_map_package_names()
    if SOURCE_MAP in dirty_maps_after:
        raise RuntimeError("source map became dirty during whitebox assembly")
    generation = {
        key: _parse_operation_payload(
            "generate_town_pcg",
            unreal.GameXXKTownPCGAutomationLibrary.generate_town_pcg(PCG_LABELS[key]),
        ) for key in PCG_LABELS
    }
    return {
        "success": True, "complete": False, "pending": True, "phase": PHASE_SETUP,
        "source_map": SOURCE_MAP, "whitebox_map": WHITEBOX_MAP,
        "whitebox_created": created, "quickroad_status": "proxy_spline",
        "graph_paths": GRAPH_PATHS, "counts": EXPECTED_COUNTS,
        "cameras": cameras, "curve_plate_count": curve_plate_count,
        "layout_sha256": layout_evidence["sha256"], "graphs": graphs,
        "generation": generation, "managed_label_counts": label_counts,
        "preserved_before": preserved_before, "preserved_after": preserved_after,
        "dirty_map_packages_before": dirty_maps_before,
        "dirty_content_packages_before": dirty_content_before,
        "dirty_map_packages_after": dirty_maps_after,
    }


def _generated_instance_count(label: str) -> int:
    actor = _find_unique_actor_by_label(label, required=True)
    return sum(
        int(component.get_instance_count())
        for component in actor.get_components_by_class(unreal.InstancedStaticMeshComponent)
    )


def finalize_whitebox() -> dict[str, Any]:
    config = load_config()
    _require_finalize_clean()
    _load_whitebox_only()
    preserved_labels = _preserved_actor_labels(config["coordinate_reference"]["actor_label"])
    preserved_before = _snapshot_preserved_actors(preserved_labels)
    statuses = {
        key: _parse_operation_payload(
            "get_town_pcg_status",
            unreal.GameXXKTownPCGAutomationLibrary.get_town_pcg_status(PCG_LABELS[key]),
        ) for key in PCG_LABELS
    }
    common = {
        "phase": PHASE_FINALIZE, "whitebox_map": WHITEBOX_MAP,
        "graph_paths": GRAPH_PATHS, "counts": EXPECTED_COUNTS,
        "statuses": statuses, "quickroad_status": "proxy_spline",
    }
    if any(status.get("generating") is True for status in statuses.values()):
        return {"success": True, "complete": False, "pending": True, **common}
    terminal = {key: status for key, status in statuses.items() if status.get("generated") is not True}
    if terminal:
        return {
            "success": False, "complete": False, "pending": False,
            "error": f"PCG generation stopped without completing: {terminal}", **common,
        }
    actual_counts = {key: _generated_instance_count(PCG_LABELS[key]) for key in PCG_LABELS}
    if actual_counts != EXPECTED_COUNTS:
        raise RuntimeError(f"generated proxy counts must be {EXPECTED_COUNTS}, got {actual_counts}")
    cameras = {
        label: _transform_payload(_find_unique_actor_by_label(label, unreal.CameraActor, True))
        for label in CAMERA_LABELS
    }
    if len(cameras) != 4 or len(TERRAIN_LABELS) != 4:
        raise RuntimeError("whitebox requires exactly four configured cameras and terrain labels")
    label_counts = _require_unique_managed_labels()
    _require_exact_camera_and_terrain_counts()
    preserved_after = _assert_preserved(preserved_before, preserved_labels)
    layout_sha = canonical_layout_hash(generate_seeded_proxy_transforms(config))["sha256"]
    if SOURCE_MAP in _dirty_map_package_names():
        raise RuntimeError("source map is dirty during finalize")
    if _current_map_package() != WHITEBOX_MAP or not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError(f"failed to save only whitebox map {WHITEBOX_MAP}")
    dirty_maps = _dirty_map_package_names()
    if SOURCE_MAP in dirty_maps:
        raise RuntimeError("source map became dirty during finalize")
    return {
        "success": True, "complete": True, "pending": False,
        "actual_counts": actual_counts, "cameras": cameras,
        "managed_label_counts": label_counts, "layout_sha256": layout_sha,
        "preserved_before": preserved_before, "preserved_after": preserved_after,
        "dirty_map_packages_after": dirty_maps, **common,
    }


def assemble_whitebox(phase: str = PHASE_SETUP) -> dict[str, Any]:
    if phase == PHASE_SETUP:
        return setup_whitebox()
    if phase == PHASE_FINALIZE:
        return finalize_whitebox()
    raise ValueError(f"unsupported phase {phase!r}; expected setup or finalize")


def _phase_from_argv(argv: list[str]) -> str:
    if "--phase" not in argv:
        return PHASE_SETUP
    index = argv.index("--phase")
    if index + 1 >= len(argv):
        raise ValueError("--phase requires setup or finalize")
    phase = argv[index + 1]
    if phase not in (PHASE_SETUP, PHASE_FINALIZE):
        raise ValueError(f"unsupported phase {phase!r}; expected setup or finalize")
    return phase


def _compact_json_result(payload) -> str:
    options = {"sort_keys": True, "separators": (",", ":"), "allow_nan": False}
    try:
        return json.dumps(payload, **options)
    except Exception as error:
        try:
            return json.dumps(
                {
                    "success": False,
                    "complete": False,
                    "error": f"result serialization failed: {type(error).__name__}",
                },
                **options,
            )
        except Exception:
            return '{"complete":false,"error":"result serialization failed","success":false}'


def main() -> None:
    try:
        result = assemble_whitebox(_phase_from_argv(sys.argv[1:]))
    except Exception as error:
        unreal.log_error(f"GameXXK Qingshan B0R whitebox assembly failed: {error}")
        result = {"success": False, "complete": False, "error": str(error)}
    print(_compact_json_result(result))


if __name__ == "__main__":
    main()
