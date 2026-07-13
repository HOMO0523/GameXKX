"""Safely assemble the isolated Qingshan B1 dress map in bounded editor phases."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
import re
import sys
from typing import Any, Iterable

try:  # Keep pure geometry/group helpers importable by host tests.
    import unreal
except ModuleNotFoundError:  # pragma: no cover - exercised only outside Unreal.
    unreal = None

from gamexxk_qingshan_dress_b1_config import (
    load_config,
    validate_protected_file_hashes,
)


PROJECT_ROOT = Path(__file__).resolve().parents[2]
B0R_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R"
B1_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1"
B1_ASSET_ROOT = "/Game/GameXXK/Environment/TownPCG/B1"
B1_GRAPH_ROOT = f"{B1_ASSET_ROOT}/Graphs"
B1_MANAGED_TAG = "QingshanB1Managed"
B0R_MANAGED_TAG = "QingshanB0RManaged"
GATE_LABEL = "QingshanInn_TownExit"

PHASE_SETUP = "setup"
PHASE_INFRASTRUCTURE = "infrastructure"
PHASE_FINALIZE_BEGIN = "finalize_begin"
PHASE_FINALIZE_POLL = "finalize_poll"
PHASES = (
    PHASE_SETUP,
    PHASE_INFRASTRUCTURE,
    PHASE_FINALIZE_BEGIN,
    PHASE_FINALIZE_POLL,
)
ASSEMBLY_MARKER = "GAMEXXK_QINGSHAN_B1_ASSEMBLY"

EXPECTED_PROTECTED_FILE_COUNT = 3
EXPECTED_PCG_GROUP_COUNTS = {
    "building": 19,
    "prop": 10,
    "static_plant": 4,
    "mountain": 1,
}
EXPECTED_PCG_GROUP_TOTAL = 34
EXPECTED_ANIMATED_PLANT_COUNT = 30
EXPECTED_POPULATION_COUNTS = {
    "buildings": 26,
    "props": 72,
    "static_plants": 70,
    "mountains": 24,
    "animated_plants": 30,
}
MINIMUM_ROAD_CLEARANCE_CM = 250.0
MAX_BUILDING_BOTTOM_ERROR_CM = 25.0
ANIMATED_PLANT_CULL_CM = 12000.0
BRIDGE_WATER_CLEARANCE_CM = 120.0
BRIDGE_APPROACH_LIFT_CM = 90.0
RIVER_SEGMENT_HALF_THICKNESS_CM = 10.0

CAMERA_LABELS = (
    "CAM_QS_B1_GATE_ARRIVAL",
    "CAM_QS_B1_TOWN_CORE",
    "CAM_QS_B1_MAIN_BRIDGE",
    "CAM_QS_B1_SOUTH_DOCK",
)

# Exactly the 18 fixed and 13 curve-plate actors created by the B0R assembler.
B0R_CLONE_CLEANUP_LABELS = frozenset({
    "QS_B0R_World_Bounds",
    "QS_B0R_Road_Main",
    "QS_B0R_Road_Core_North",
    "QS_B0R_Road_Core_South",
    "QS_B0R_River_Main",
    "QS_B0R_Bridge_Main",
    "QS_B0R_Dock_South",
    "QS_B0R_Terrain_Base",
    "QS_B0R_Terrain_CorePlateau",
    "QS_B0R_Terrain_SouthSlope",
    "QS_B0R_Terrain_DockLowland",
    "QS_B0R_PCG_Buildings",
    "QS_B0R_PCG_Foliage",
    "QS_B0R_PCG_Mountains",
    "CAM_QS_GATE_ARRIVAL",
    "CAM_QS_TOWN_CORE",
    "CAM_QS_MAIN_BRIDGE",
    "CAM_QS_SOUTH_DOCK",
    *{f"QS_B0R_RoadPlate_Road_Main_{index:03d}" for index in range(5)},
    *{f"QS_B0R_RoadPlate_Road_Core_North_{index:03d}" for index in range(2)},
    *{f"QS_B0R_RoadPlate_Road_Core_South_{index:03d}" for index in range(2)},
    *{f"QS_B0R_RiverPlate_River_Main_{index:03d}" for index in range(4)},
})

_SHA256 = re.compile(r"^[0-9a-f]{64}$")
_PROTECTED_LABEL_TOKENS = ("quest", "npc", "follower", "interaction")


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("B1 assembly mutations require Unreal Editor Python")


def _stable_seed(base_seed: int, key: str) -> int:
    digest = hashlib.sha256(f"{int(base_seed)}:{key}".encode("utf-8")).hexdigest()
    return int(digest[:8], 16) & 0x7FFFFFFF


def _sanitize(value: str) -> str:
    sanitized = re.sub(r"[^A-Za-z0-9_]+", "_", str(value)).strip("_")
    if not sanitized:
        raise ValueError(f"cannot sanitize empty identifier from {value!r}")
    return sanitized


def _building_variant_path(config: dict[str, Any], archetype: str, palette: str) -> str:
    source = config["asset_catalog"]["building_meshes"][archetype]
    source_name = source.rsplit("/", 1)[-1]
    return f"{B1_ASSET_ROOT}/Meshes/Variants/{source_name}_{palette.title()}"


def build_pcg_group_specs(config: dict[str, Any]) -> list[dict[str, Any]]:
    """Return the deterministic 34-group PCG authoring manifest without UE access."""

    seed = int(config["seed"])
    groups: list[dict[str, Any]] = []
    shared = config["building_materials"]["shared_slot_materials"]
    roofs = config["building_materials"]["roof_palette_materials"]

    building_groups: dict[tuple[str, str], list[dict[str, Any]]] = {}
    for record in config["building_plots"]:
        key = (record["archetype_id"], record["roof_palette"])
        building_groups.setdefault(key, []).append(record)
    for (archetype, palette), records in sorted(building_groups.items()):
        key = f"building__{archetype}__{palette}"
        suffix = f"Building_{_sanitize(archetype)}_{_sanitize(palette)}"
        groups.append({
            "key": key,
            "category": "building",
            "records": sorted(records, key=lambda item: item["id"]),
            "mesh_path": _building_variant_path(config, archetype, palette),
            "slot_materials": {
                "Wall": shared["Wall"],
                "Timber": shared["Timber"],
                "WindowPaper": shared["WindowPaper"],
                "Roof": roofs[palette],
            },
            "graph_path": f"{B1_GRAPH_ROOT}/PCG_QS_B1_{suffix}",
            "volume_label": f"QS_B1_PCG_{suffix}",
            "base_seed": _stable_seed(seed, f"{key}:points"),
            "component_seed": _stable_seed(seed, f"{key}:component"),
            "collision_enabled": True,
            "cull_end_cm": 30000.0,
        })

    prop_groups: dict[str, list[dict[str, Any]]] = {}
    for record in config["prop_records"]:
        prop_groups.setdefault(record["prop_type"], []).append(record)
    for prop_type, records in sorted(prop_groups.items()):
        key = f"prop__{prop_type}"
        suffix = f"Prop_{_sanitize(prop_type)}"
        groups.append({
            "key": key,
            "category": "prop",
            "records": sorted(records, key=lambda item: item["id"]),
            "mesh_path": config["asset_catalog"]["prop_meshes"][prop_type],
            "slot_materials": None,
            "graph_path": f"{B1_GRAPH_ROOT}/PCG_QS_B1_{suffix}",
            "volume_label": f"QS_B1_PCG_{suffix}",
            "base_seed": _stable_seed(seed, f"{key}:points"),
            "component_seed": _stable_seed(seed, f"{key}:component"),
            "collision_enabled": bool(config["collision_policy"][prop_type]),
            "cull_end_cm": 18000.0,
        })

    static_plants = [
        record for record in config["vegetation_records"]
        if record["render_mode"] == "static_card"
    ]
    for frame_variant in range(4):
        records = [
            record for record in static_plants
            if int(record["frame_variant"]) == frame_variant
        ]
        key = f"static_plant__frame_{frame_variant}"
        suffix = f"Plant_Frame_{frame_variant}"
        groups.append({
            "key": key,
            "category": "static_plant",
            "records": sorted(records, key=lambda item: item["id"]),
            "mesh_path": config["asset_catalog"]["plant_card_mesh"],
            "slot_materials": {
                "Foliage": f"{B1_ASSET_ROOT}/Materials/MI_QS_B1_Foliage_F{frame_variant}",
            },
            "graph_path": f"{B1_GRAPH_ROOT}/PCG_QS_B1_{suffix}",
            "volume_label": f"QS_B1_PCG_{suffix}",
            "base_seed": _stable_seed(seed, f"{key}:points"),
            "component_seed": _stable_seed(seed, f"{key}:component"),
            "collision_enabled": False,
            "cull_end_cm": 12000.0,
        })

    key = "mountain"
    groups.append({
        "key": key,
        "category": "mountain",
        "records": sorted(config["mountain_records"], key=lambda item: item["id"]),
        "mesh_path": config["asset_catalog"]["mountain_mesh"],
        "slot_materials": None,
        "graph_path": f"{B1_GRAPH_ROOT}/PCG_QS_B1_Mountain",
        "volume_label": "QS_B1_PCG_Mountain",
        "base_seed": _stable_seed(seed, f"{key}:points"),
        "component_seed": _stable_seed(seed, f"{key}:component"),
        "collision_enabled": False,
        "cull_end_cm": 60000.0,
    })

    observed: dict[str, int] = {}
    for group in groups:
        observed[group["category"]] = observed.get(group["category"], 0) + 1
        if not group["records"]:
            raise RuntimeError(f"PCG group must be nonempty: {group['key']}")
    if observed != EXPECTED_PCG_GROUP_COUNTS or len(groups) != EXPECTED_PCG_GROUP_TOTAL:
        raise RuntimeError(
            f"B1 PCG group contract drift: expected={EXPECTED_PCG_GROUP_COUNTS}, "
            f"observed={observed}, total={len(groups)}"
        )
    if len({group["graph_path"] for group in groups}) != len(groups):
        raise RuntimeError("B1 PCG graph paths must be unique")
    return groups


def _interpolate(first, second, alpha: float) -> tuple[float, float, float]:
    return tuple(
        float(first[index]) + (float(second[index]) - float(first[index])) * alpha
        for index in range(3)
    )


def _segment_circle_roots(first, second, center, radius: float) -> list[float]:
    ax, ay = float(first[0]) - float(center[0]), float(first[1]) - float(center[1])
    dx = float(second[0]) - float(first[0])
    dy = float(second[1]) - float(first[1])
    a = dx * dx + dy * dy
    if a <= 1.0e-12:
        return []
    b = 2.0 * (ax * dx + ay * dy)
    c = ax * ax + ay * ay - radius * radius
    discriminant = b * b - 4.0 * a * c
    if discriminant < 0.0:
        return []
    root = math.sqrt(max(discriminant, 0.0))
    values = [(-b - root) / (2.0 * a), (-b + root) / (2.0 * a)]
    return sorted({max(0.0, min(1.0, value)) for value in values if 0.0 < value < 1.0})


def _split_polyline_outside_circle(points, center, radius: float) -> list[list[tuple[float, float, float]]]:
    """Clip a polyline to the outside of a protected XY circle."""

    points = [tuple(map(float, point)) for point in points]
    center = tuple(map(float, center))
    radius = float(radius)
    if len(points) < 2 or len(center) < 2 or not math.isfinite(radius) or radius <= 0.0:
        raise ValueError("bridge-gap clipping requires a polyline and positive finite radius")
    result: list[list[tuple[float, float, float]]] = []
    current: list[tuple[float, float, float]] = []
    epsilon = 1.0e-6

    def outside(point) -> bool:
        return math.hypot(point[0] - center[0], point[1] - center[1]) >= radius - epsilon

    for first, second in zip(points, points[1:]):
        breakpoints = [0.0, *_segment_circle_roots(first, second, center, radius), 1.0]
        for start_t, end_t in zip(breakpoints, breakpoints[1:]):
            start = _interpolate(first, second, start_t)
            end = _interpolate(first, second, end_t)
            midpoint = _interpolate(first, second, (start_t + end_t) * 0.5)
            if outside(midpoint):
                if not current:
                    current.append(start)
                elif math.dist(current[-1], start) > epsilon:
                    current.append(start)
                if math.dist(current[-1], end) > epsilon:
                    current.append(end)
            elif current:
                if len(current) >= 2:
                    result.append(current)
                current = []
    if current and len(current) >= 2:
        result.append(current)
    if not result or any(not all(outside(point) for point in segment) for segment in result):
        raise RuntimeError("bridge-gap clipping did not produce complete outside segments")
    return result


def local_to_world(anchor_transform, local_xyz):
    return anchor_transform.transform_location(unreal.Vector(*map(float, local_xyz)))


def _name(value: str):
    return unreal.Name(str(value))


def _all_actors() -> list:
    return list(unreal.EditorLevelLibrary.get_all_level_actors())


def _actor_label(actor) -> str:
    return str(actor.get_actor_label())


def _actor_tags(actor) -> set[str]:
    return {str(tag) for tag in actor.get_editor_property("tags")}


def _set_tags(target, tags: Iterable[str]) -> None:
    property_name = "component_tags" if isinstance(target, unreal.ActorComponent) else "tags"
    target.set_editor_property(property_name, [_name(tag) for tag in sorted(set(tags))])


def _find_unique_actor(label: str, expected_class=None, required: bool = False):
    matches = [actor for actor in _all_actors() if _actor_label(actor) == label]
    if len(matches) > 1:
        raise RuntimeError(f"duplicate exact actor label {label!r}: {len(matches)}")
    if not matches:
        if required:
            raise RuntimeError(f"required actor label is absent: {label}")
        return None
    actor = matches[0]
    if expected_class is not None and not isinstance(actor, expected_class):
        raise RuntimeError(f"actor {label!r} is not {expected_class}")
    return actor


def _vector_payload(vector) -> list[float]:
    result = [float(vector.x), float(vector.y), float(vector.z)]
    if not all(math.isfinite(value) for value in result):
        raise RuntimeError(f"nonfinite vector: {result}")
    return result


def _transform_payload(actor) -> dict[str, Any]:
    transform = actor.get_actor_transform()
    rotation = actor.get_actor_rotation()
    return {
        "location": _vector_payload(transform.translation),
        "rotation": [float(rotation.roll), float(rotation.pitch), float(rotation.yaw)],
        "scale": _vector_payload(transform.scale3d),
        "class": str(actor.get_class().get_path_name()),
    }


def _snapshot_protected_actors() -> dict[str, Any]:
    labels = {"QingshanInn_TownExit"}
    for actor in _all_actors():
        label = _actor_label(actor)
        if isinstance(actor, unreal.PlayerStart) or any(
                token in label.lower() for token in ("quest", "npc", "follower", "interaction")):
            labels.add(label)
    return {
        label: _transform_payload(_find_unique_actor(label, required=True))
        for label in sorted(labels)
    }


def _assert_protected_actors(before: dict[str, Any]) -> dict[str, Any]:
    after = _snapshot_protected_actors()
    if after != before:
        raise RuntimeError(f"protected anchor/NPC actors changed: before={before}, after={after}")
    return after


def _validate_north_gate_anchor(actor, tolerance: float = 1.0e-3):
    transform = actor.get_actor_transform()
    rotation = actor.get_actor_rotation()
    values = [
        transform.translation.x, transform.translation.y, transform.translation.z,
        rotation.pitch, rotation.yaw, rotation.roll,
        transform.scale3d.x, transform.scale3d.y, transform.scale3d.z,
    ]
    if not all(math.isfinite(float(value)) for value in values):
        raise RuntimeError("north gate F anchor transform must be finite")
    if abs(float(rotation.pitch)) > tolerance or abs(float(rotation.roll)) > tolerance:
        raise RuntimeError("north gate F anchor must remain upright")
    if any(abs(float(value) - 1.0) > tolerance for value in (
            transform.scale3d.x, transform.scale3d.y, transform.scale3d.z)):
        raise RuntimeError("north gate F anchor must retain unit scale")
    return transform


def _current_map_package() -> str:
    world = unreal.EditorLevelLibrary.get_editor_world()
    return str(world.get_outermost().get_path_name()).split(".", 1)[0] if world else ""


def _current_level_package() -> str:
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    level = subsystem.get_current_level() if subsystem is not None else None
    return str(level.get_outermost().get_path_name()).split(".", 1)[0] if level else ""


def _require_b1_current_map() -> None:
    world_package = _current_map_package()
    level_package = _current_level_package()
    if world_package != B1_MAP or level_package != B1_MAP:
        raise RuntimeError(
            f"B1 mutation requires exact world/current level {B1_MAP}; "
            f"got {world_package}/{level_package}"
        )


def _dirty_map_packages() -> list[str]:
    return sorted(
        str(package.get_path_name()).split(".", 1)[0]
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    )


def _dirty_content_packages() -> list[str]:
    return sorted(
        str(package.get_path_name()).split(".", 1)[0]
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    )


def _validate_dirty_scope(*, require_clean: bool = False) -> tuple[list[str], list[str]]:
    maps = _dirty_map_packages()
    content = _dirty_content_packages()
    if require_clean and (maps or content):
        raise RuntimeError(f"B1 map duplication requires a clean editor: maps={maps}, content={content}")
    invalid_maps = [path for path in maps if path != B1_MAP]
    invalid_content = [
        path for path in content
        if not path.startswith(f"{B1_ASSET_ROOT}/")
    ]
    if invalid_maps or invalid_content:
        raise RuntimeError(
            f"B1 automation refuses unrelated dirty packages: "
            f"maps={invalid_maps}, content={invalid_content}"
        )
    return maps, content


def _ensure_b1_map() -> bool:
    if unreal.EditorAssetLibrary.does_asset_exist(B1_MAP):
        return False
    _validate_dirty_scope(require_clean=True)
    if not unreal.EditorAssetLibrary.does_asset_exist(B0R_MAP):
        raise RuntimeError(f"B0R clone source does not exist: {B0R_MAP}")
    duplicate = unreal.EditorAssetLibrary.duplicate_asset(B0R_MAP, B1_MAP)
    if duplicate is None:
        raise RuntimeError(f"failed to duplicate_asset(B0R_MAP, B1_MAP): {B0R_MAP} -> {B1_MAP}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(duplicate, True):
        raise RuntimeError(f"failed to save only duplicated B1 map {B1_MAP}")
    package = unreal.EditorAssetLibrary.get_package_for_object(duplicate)
    if package is None:
        raise RuntimeError("could not resolve duplicated B1 map package")
    del duplicate
    unloaded, error_message = unreal.EditorLoadingAndSavingUtils.unload_packages([package])
    if not unloaded:
        raise RuntimeError(f"failed to unload duplicated B1 map: {error_message}")
    del package
    unreal.SystemLibrary.collect_garbage()
    return True


def _load_b1_map() -> None:
    if _current_map_package() != B1_MAP:
        _validate_dirty_scope()
        if not unreal.EditorLoadingAndSavingUtils.load_map(B1_MAP):
            raise RuntimeError(f"failed to load B1 map {B1_MAP}")
    _require_b1_current_map()


def _destroy_exact_actors(actors: Iterable[Any]) -> int:
    count = 0
    for actor in actors:
        if not unreal.EditorLevelLibrary.destroy_actor(actor):
            raise RuntimeError(f"failed to destroy actor {_actor_label(actor)!r}")
        count += 1
    return count


def _cleanup_created_b0r_clone(created: bool, protected_labels: set[str]) -> int:
    _require_b1_current_map()
    required_tag = "QingshanB0RManaged"
    if required_tag != B0R_MANAGED_TAG:
        raise RuntimeError("B0R clone cleanup ownership tag contract drift")
    cloned = [actor for actor in _all_actors() if required_tag in _actor_tags(actor)]
    if not created:
        if cloned:
            raise RuntimeError(
                "rerun found B0R-managed clone residue; delete/recreate B1 rather than broad cleanup"
            )
        return 0
    labels = [_actor_label(actor) for actor in cloned]
    unexpected = sorted(set(labels) - B0R_CLONE_CLEANUP_LABELS)
    if unexpected:
        raise RuntimeError(f"unexpected cloned B0R managed actor outside allowlist: {unexpected}")
    if protected_labels.intersection(labels):
        raise RuntimeError("B0R clone cleanup allowlist intersects protected actors")
    if len(labels) != len(set(labels)):
        raise RuntimeError("duplicate B0R managed labels make clone cleanup ambiguous")
    return _destroy_exact_actors(cloned)


def _cleanup_b1_rerun_actors(protected_labels: set[str]) -> int:
    _require_b1_current_map()
    owned = [actor for actor in _all_actors() if B1_MANAGED_TAG in _actor_tags(actor)]
    overlap = sorted(protected_labels.intersection(_actor_label(actor) for actor in owned))
    if overlap:
        raise RuntimeError(f"refusing to delete protected actor carrying QingshanB1Managed: {overlap}")
    return _destroy_exact_actors(owned)


def _parse_operation_payload(operation: str, payload: Any) -> dict[str, Any]:
    try:
        data = json.loads(str(payload), parse_constant=lambda value: (_ for _ in ()).throw(
            ValueError(f"nonfinite constant {value}")))
    except (TypeError, ValueError) as error:
        raise RuntimeError(f"{operation} returned invalid JSON: {payload!r}") from error
    if not isinstance(data, dict) or data.get("success") is not True:
        raise RuntimeError(f"{operation} failed: {data}")
    return data


def _package_path(value: str) -> str:
    return str(value).split(".", 1)[0]


def _load_asset(package_path: str):
    asset = unreal.EditorAssetLibrary.load_asset(str(package_path))
    if asset is None:
        raise RuntimeError(f"required B1 asset is missing: {package_path}")
    return asset


def _save_b1_only(config: dict[str, Any]) -> dict[str, Any]:
    _require_b1_current_map()
    maps, content = _validate_dirty_scope()
    if any(path != B1_MAP for path in maps):
        raise RuntimeError(f"save scope contains a non-B1 map: {maps}")
    for package_path in content:
        if not package_path.startswith(f"{B1_ASSET_ROOT}/"):
            raise RuntimeError(f"save scope escaped B1_ASSET_ROOT: {package_path}")
        if not unreal.EditorAssetLibrary.save_asset(package_path, True):
            raise RuntimeError(f"failed to save B1 asset package {package_path}")
    if _current_map_package() != B1_MAP or not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError(f"failed to save only B1_MAP {B1_MAP}")
    after_hashes = validate_protected_file_hashes(config["protected_files"], PROJECT_ROOT)
    if len(after_hashes) != EXPECTED_PROTECTED_FILE_COUNT:
        raise RuntimeError("protected raw hash count drift after B1 save")
    if B0R_MAP in _dirty_map_packages():
        raise RuntimeError(f"B0R source remained dirty after B1-only save: {B0R_MAP}")
    return {"saved_map": B1_MAP, "saved_assets": content}


def setup_b1(config: dict[str, Any]) -> dict[str, Any]:
    if config["source_map"] != B0R_MAP or config["dress_map"] != B1_MAP:
        raise RuntimeError("B1 config map paths do not match exact assembler constants")
    _validate_dirty_scope()
    created = _ensure_b1_map()
    _load_b1_map()
    _require_b1_current_map()
    protected_before = _snapshot_protected_actors()
    gate = _find_unique_actor("QingshanInn_TownExit", required=True)
    _validate_north_gate_anchor(gate)
    protected_labels = set(protected_before)
    removed_b0r = _cleanup_created_b0r_clone(created, protected_labels)
    removed_b1 = _cleanup_b1_rerun_actors(protected_labels)
    protected_after = _assert_protected_actors(protected_before)
    save = _save_b1_only(config)
    return {
        "success": True,
        "phase": PHASE_SETUP,
        "state": "complete",
        "complete": True,
        "pending": False,
        "b1_created": created,
        "removed_b0r_clone_actor_count": removed_b0r,
        "removed_b1_actor_count": removed_b1,
        "protected_before": protected_before,
        "protected_after": protected_after,
        "save": save,
        "error": "",
    }


def _anchor_local_yaw(anchor_transform, local_yaw: float) -> float:
    value = float(anchor_transform.rotation.rotator().yaw) + float(local_yaw)
    if not math.isfinite(value):
        raise RuntimeError(f"nonfinite anchor-local yaw: {local_yaw!r}")
    return value


def _create_or_update_b1_spline(label: str, anchor_transform, local_points, tags) -> Any:
    _require_b1_current_map()
    existing = _find_unique_actor(label)
    if existing is not None and B1_MANAGED_TAG not in _actor_tags(existing):
        raise RuntimeError(f"refusing to replace unowned B1 spline actor {label!r}")
    world_points = [local_to_world(anchor_transform, point) for point in local_points]
    payload = _parse_operation_payload(
        "create_or_update_tagged_spline",
        unreal.GameXXKTownPCGAutomationLibrary.create_or_update_tagged_spline(
            label,
            world_points,
            False,
            [_name(tag) for tag in sorted({"PrototypeOnly", B1_MANAGED_TAG, *tags})],
        ),
    )
    actor = _find_unique_actor(label, required=True)
    if B1_MANAGED_TAG not in _actor_tags(actor):
        raise RuntimeError(f"B1 spline helper omitted ownership tag for {label}")
    return {"actor": actor, "operation": payload, "point_count": len(world_points)}


def _validate_sha256(value: Any, field: str) -> str:
    if not isinstance(value, str) or not _SHA256.fullmatch(value):
        raise RuntimeError(f"{field} must be a lowercase SHA-256")
    return value


def _validate_quickroad_audit(
        audit: dict[str, Any], config: dict[str, Any], expected_center_world, *,
        require_active: bool = True) -> dict[str, Any]:
    if audit.get("network_count") != 1 or audit.get("owned_network_count") != 1:
        raise RuntimeError(f"QuickRoad audit requires one consolidated network: {audit}")
    if audit.get("unowned_network_count") != 0:
        raise RuntimeError("QuickRoad audit found unowned networks")
    if int(audit.get("road_triangle_count", 0)) <= 0:
        raise RuntimeError("QuickRoad audit road_triangle_count must be nonzero")
    if int(audit.get("intersection_patch_count", 0)) <= 0:
        raise RuntimeError("QuickRoad audit intersection_patch_count must be nonzero")
    if audit.get("source_width_group_count") != 3:
        raise RuntimeError("QuickRoad audit source_width_group_count must be three")
    expected = {
        network["tag"]: int(network["width_cm"])
        for network in config["quickroad"]["networks"]
    }
    source_records = audit.get("source_records")
    if not isinstance(source_records, list) or len(source_records) != 3:
        raise RuntimeError("QuickRoad audit source_records must contain three entries")
    observed = {}
    for record in source_records:
        tag = record.get("road_tag")
        widths = record.get("road_widths_cm")
        materials = record.get("network_material_paths")
        if tag not in expected or widths != [expected[tag]] or int(record.get("spline_count", 0)) <= 0:
            raise RuntimeError(f"QuickRoad source width/tag audit mismatch: {record}")
        if not materials or {
                _package_path(path) for path in materials} != {config["quickroad"]["road_material"]}:
            raise RuntimeError(f"QuickRoad source material audit mismatch: {record}")
        observed[tag] = widths[0]
    if observed != expected:
        raise RuntimeError(f"QuickRoad source_records mismatch: {observed} != {expected}")
    if audit.get("edit_layer") != config["landscape"]["edit_layer"]:
        raise RuntimeError("QuickRoad audit edit layer name drift")
    if audit.get("edit_layer_visible") is not True:
        raise RuntimeError("QuickRoad audit edit layer must be visible")
    if require_active and audit.get("edit_layer_active") is not True:
        raise RuntimeError("QuickRoad audit edit layer must be active during assembly")
    if audit.get("edit_layer_locked") is not False:
        raise RuntimeError("QuickRoad audit edit layer must remain unlocked")
    if int(audit.get("non_neutral_sample_count", 0)) <= 0:
        raise RuntimeError("QuickRoad audit non_neutral_sample_count must be nonzero")
    delta_digest = _validate_sha256(
        audit.get("edit_layer_delta_sha256"), "edit_layer_delta_sha256")
    edge_actor_labels = audit.get("edge_actor_labels")
    if edge_actor_labels != ["QS_B1_QR_Edge_000"]:
        raise RuntimeError(f"QuickRoad stable edge_actor_labels mismatch: {edge_actor_labels}")
    edge_spline_count = int(audit.get("edge_spline_count", 0))
    if edge_spline_count < 2:
        raise RuntimeError("QuickRoad edge_spline_count < 2; bridge gap needs multiple boundaries")
    edge_digest = _validate_sha256(audit.get("edge_geometry_sha256"), "edge_geometry_sha256")
    center = audit.get("reconstructed_grid_center_world")
    expected_center = _vector_payload(expected_center_world)
    if not isinstance(center, list) or len(center) != 3 or any(
            abs(float(center[index]) - expected_center[index]) > 1.0 for index in range(3)):
        raise RuntimeError(f"QuickRoad Landscape grid center mismatch: {center} != {expected_center}")
    return {
        "quickroad_status": "verified_procmesh",
        "source_records": source_records,
        "network_count": 1,
        "road_triangle_count": int(audit["road_triangle_count"]),
        "intersection_patch_count": int(audit["intersection_patch_count"]),
        "edit_layer_delta_sha256": delta_digest,
        "edge_actor_labels": list(edge_actor_labels),
        "edge_spline_count": edge_spline_count,
        "edge_geometry_sha256": edge_digest,
    }


def _call_quickroad(operation: str, callable_object, *args) -> dict[str, Any]:
    return _parse_operation_payload(operation, callable_object(*args))


def build_infrastructure(config: dict[str, Any]) -> dict[str, Any]:
    _require_b1_current_map()
    _validate_dirty_scope()
    protected_before = _snapshot_protected_actors()
    gate = _find_unique_actor("QingshanInn_TownExit", required=True)
    anchor_transform = _validate_north_gate_anchor(gate)
    quickroad = config["quickroad"]
    expected_width_groups = (
        ("QS_B1_Main", 800),
        ("QS_B1_CoreNorth", 450),
        ("QS_B1_CoreSouth", 400),
    )
    observed_width_groups = tuple(
        (network["tag"], int(network["width_cm"]))
        for network in quickroad["networks"]
    )
    if observed_width_groups != expected_width_groups:
        raise RuntimeError(
            f"QuickRoad width group contract drift: {observed_width_groups}"
        )
    bridge = next(
        item for item in config["fixed_anchors"] if item["id"] == "MainBridgeAnchor"
    )
    facade = unreal.Quick_RoadAutomationLibrary
    operations = {
        "reset": _call_quickroad(
            "reset_road_infrastructure", facade.reset_road_infrastructure),
    }

    input_splines = []
    for network in quickroad["networks"]:
        segments = [network["points_cm"]]
        if network["source_spline_id"] == "Road_Main":
            segments = _split_polyline_outside_circle(
                network["points_cm"], bridge["location_cm"], bridge["protected_radius_cm"])
            if len(segments) != 2:
                raise RuntimeError("MainBridgeAnchor protected_radius_cm must split Road_Main in two")
        for segment_index, local_points in enumerate(segments):
            label = f"QS_B1_QR_Input_{network['tag']}_{segment_index:02d}"
            input_splines.append(_create_or_update_b1_spline(
                label,
                anchor_transform,
                local_points,
                (
                    "Quick_Road_LayoutSpline",
                    "Quick_Road_MainRoad",
                    "Quick_Road_LayoutInput",
                    network["tag"],
                ),
            ))

    landscape = config["landscape"]
    desired_center_world = local_to_world(anchor_transform, landscape["center_local_cm"])
    heightmap_path = str((PROJECT_ROOT / landscape["heightmap_source"]).resolve())
    operations["landscape"] = _call_quickroad(
        "ensure_landscape_infrastructure",
        facade.ensure_landscape_infrastructure,
        landscape["label"],
        int(landscape["quads_per_section"]),
        int(landscape["sections_per_component"]),
        unreal.IntPoint(
            int(landscape["component_count"][0]),
            int(landscape["component_count"][1]),
        ),
        desired_center_world,
        unreal.Rotator(
            pitch=0.0,
            yaw=_anchor_local_yaw(anchor_transform, 0.0),
            roll=0.0,
        ),
        unreal.Vector(*map(float, landscape["scale_cm"])),
        _name(landscape["edit_layer"]),
        landscape["ground_material"],
        heightmap_path,
    )

    ribbon = quickroad["ribbon"]
    influence = quickroad["landscape_influence"]
    intersection = quickroad["intersection"]
    operations["generate"] = {}
    for network in quickroad["networks"]:
        tag = network["tag"]
        operations["generate"][tag] = _call_quickroad(
            "generate_road_network",
            facade.generate_road_network,
            _name(tag),
            float(network["width_cm"]),
            float(influence["falloff_cm"]),
            float(influence["blend"]),
            float(ribbon["mesh_sample_distance_cm"]),
            int(ribbon["width_subdivisions"]),
            True,
            float(ribbon["curvature_threshold_degrees"]),
            int(ribbon["max_curvature_subdivisions"]),
            float(intersection["sample_distance_cm"]),
            float(intersection["border_offset_cm"]),
            quickroad["road_material"],
        )
    operations["intersections"] = _call_quickroad(
        "rebuild_road_intersections",
        facade.rebuild_road_intersections,
        _name("QS_B1_Main|QS_B1_CoreNorth|QS_B1_CoreSouth"),
        float(intersection["sample_distance_cm"]),
        float(intersection["border_offset_cm"]),
        float(intersection["corner_radius_cm"]),
        True,
        float(intersection["branch_width_scale"]),
        float(ribbon["mesh_sample_distance_cm"]),
        int(ribbon["width_subdivisions"]),
        True,
        float(ribbon["curvature_threshold_degrees"]),
        int(ribbon["max_curvature_subdivisions"]),
        quickroad["road_material"],
    )
    operations["influence"] = _call_quickroad(
        "clear_and_apply_all_road_influence",
        facade.clear_and_apply_all_road_influence,
        landscape["label"],
        _name(landscape["edit_layer"]),
        float(influence["falloff_cm"]),
        float(influence["blend"]),
        float(influence["vertical_offset_cm"]),
        int(influence["smooth_iterations"]),
        float(influence["smooth_strength"]),
    )
    operations["edges"] = _call_quickroad(
        "extract_road_edges",
        facade.extract_road_edges,
        100.0,
        1.0,
        True,
        False,
        200.0,
    )
    raw_audit = _call_quickroad(
        "audit_infrastructure",
        facade.audit_infrastructure,
        landscape["label"],
        _name(landscape["edit_layer"]),
        _name(quickroad["combined_tag_expression"]),
    )
    audit = _validate_quickroad_audit(raw_audit, config, desired_center_world)
    protected_after = _assert_protected_actors(protected_before)
    return {
        "success": True,
        "phase": PHASE_INFRASTRUCTURE,
        "state": "complete",
        "complete": True,
        "pending": False,
        "input_spline_count": len(input_splines),
        "operations": operations,
        "quickroad": audit,
        "quickroad_status": audit["quickroad_status"],
        "protected_before": protected_before,
        "protected_after": protected_after,
        "error": "",
    }


def _ground_snap_world(anchor_transform, local_xyz, *, z_offset_cm: float = 0.0):
    nominal = local_to_world(anchor_transform, local_xyz)
    impact = _trace_ground_at_world_location(nominal)
    values = _vector_payload(impact)
    return unreal.Vector(values[0], values[1], values[2] + float(z_offset_cm))


def _require_trace_hit_result(value, world_location):
    """UE Python maps bool + one out HitResult to HitResult on success, None on miss."""

    if value is None:
        raise RuntimeError(
            f"Landscape line_trace_single missed at world point "
            f"{_vector_payload(world_location)}"
        )
    return value


def _impact_point_from_hit_result(hit_result):
    try:
        payload = hit_result.to_dict()
    except Exception as error:
        raise RuntimeError("HitResult did not expose the UE 5.8 to_dict contract") from error
    if not isinstance(payload, dict) or "impact_point" not in payload:
        raise RuntimeError(f"HitResult to_dict omitted impact_point: {payload!r}")
    impact = payload["impact_point"]
    _vector_payload(impact)
    return impact


def _trace_ground_at_world_location(world_location, *, actors_to_ignore=None):
    start = unreal.Vector(
        float(world_location.x), float(world_location.y), float(world_location.z) + 50000.0)
    end = unreal.Vector(
        float(world_location.x), float(world_location.y), float(world_location.z) - 50000.0)
    world = unreal.EditorLevelLibrary.get_editor_world()
    hit_result = _require_trace_hit_result(
        unreal.SystemLibrary.line_trace_single(
            world,
            start,
            end,
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
            True,
            list(actors_to_ignore or []),
            unreal.DrawDebugTrace.NONE,
            True,
        ),
        world_location,
    )
    return _impact_point_from_hit_result(hit_result)


def _attachment_height_above_target_base(
        record: dict[str, Any], building_plots_by_id: dict[str, dict[str, Any]]) -> float:
    target_id = str(record.get("attachment_target_id", ""))
    target = building_plots_by_id.get(target_id)
    if target is None:
        raise RuntimeError(
            f"attachment target {target_id!r} is missing for {record.get('id')!r}"
        )
    return float(record["location_cm"][2]) - float(target["location_cm"][2])


def _record_transform(
        anchor_transform, group: dict[str, Any], record: dict[str, Any],
        building_plots_by_id: dict[str, dict[str, Any]] | None = None):
    category = group["category"]
    local = record["location_cm"]
    if category == "prop" and record.get("attachment_target_id"):
        if building_plots_by_id is None:
            raise RuntimeError("attachment props require the building plot lookup")
        target = building_plots_by_id[str(record["attachment_target_id"])]
        attachment_height = _attachment_height_above_target_base(
            record, building_plots_by_id)
        target_base = _ground_snap_world(anchor_transform, target["location_cm"])
        attachment_world = local_to_world(anchor_transform, local)
        location = unreal.Vector(
            float(attachment_world.x),
            float(attachment_world.y),
            float(target_base.z) + attachment_height,
        )
    else:
        location = _ground_snap_world(anchor_transform, local)
    if category == "building":
        scale_values = [float(value) / 100.0 for value in record["size_cm"]]
        local_yaw = float(record["yaw_degrees"])
    elif category == "mountain":
        scale_values = [float(value) for value in record["scale"]]
        local_yaw = float(record["yaw_degrees"])
    else:
        scale = float(record["scale"])
        scale_values = [scale, scale, scale]
        local_yaw = float(record["yaw_degrees"])
    return unreal.Transform(
        rotation=unreal.Rotator(
            pitch=0.0,
            yaw=_anchor_local_yaw(anchor_transform, local_yaw),
            roll=0.0,
        ),
        location=location,
        scale=unreal.Vector(*scale_values),
    )


def _mesh_material_override_paths(mesh, slot_materials) -> list[str]:
    static_materials = list(mesh.get_editor_property("static_materials"))
    if not static_materials:
        raise RuntimeError(f"PCG mesh has no material slots: {mesh}")
    slot_names = [str(item.get_editor_property("material_slot_name")) for item in static_materials]
    if len(slot_names) != len(set(slot_names)):
        raise RuntimeError(f"PCG mesh material slots are duplicated: {slot_names}")
    if slot_materials is not None:
        if set(slot_names) != set(slot_materials):
            raise RuntimeError(
                f"PCG material override slots mismatch: mesh={slot_names}, "
                f"requested={sorted(slot_materials)}"
            )
        return [str(slot_materials[name]) for name in slot_names]
    result = []
    for item in static_materials:
        material = item.get_editor_property("material_interface")
        if material is None:
            raise RuntimeError(f"PCG mesh slot has no authored material: {item}")
        result.append(_package_path(material.get_path_name()))
    return result


def _road_edge_spline_components(consumed_edge_labels: list[str], expected_count: int) -> list[Any]:
    splines = []
    for label in consumed_edge_labels:
        actor = _find_unique_actor(label, required=True)
        components = list(actor.get_components_by_class(unreal.SplineComponent))
        if not components:
            raise RuntimeError(f"consumed QuickRoad edge actor has no spline components: {label}")
        splines.extend(components)
    if len(splines) != int(expected_count):
        raise RuntimeError(
            f"live QuickRoad edge component count drift: {len(splines)} != {expected_count}"
        )
    return splines


def _filter_transforms_against_live_edges(
        record_transforms: list[tuple[dict[str, Any], Any]],
        edge_splines: list[Any],
        minimum_road_clearance_cm: float) -> tuple[list[Any], list[str]]:
    result = []
    rejected_stable_ids = []
    clearance = float(minimum_road_clearance_cm)
    for record, transform in record_transforms:
        location = transform.translation
        nearest_distance = math.inf
        for spline in edge_splines:
            closest = spline.find_location_closest_to_world_location(
                location, unreal.SplineCoordinateSpace.WORLD)
            nearest_distance = min(
                nearest_distance,
                math.hypot(float(location.x) - float(closest.x), float(location.y) - float(closest.y)),
            )
        if nearest_distance >= clearance:
            result.append(transform)
        else:
            rejected_stable_ids.append(str(record["id"]))
    return result, rejected_stable_ids


def _pcg_volume_center_extent(config: dict[str, Any], anchor_transform):
    bounds = config["world_bounds_cm"]
    center_local = (
        (float(bounds[0]) + float(bounds[1])) * 0.5,
        (float(bounds[2]) + float(bounds[3])) * 0.5,
        2500.0,
    )
    center = local_to_world(anchor_transform, center_local)
    extent = unreal.Vector(
        (float(bounds[1]) - float(bounds[0])) * 0.75,
        (float(bounds[3]) - float(bounds[2])) * 0.75,
        8000.0,
    )
    return center, extent


def _author_pcg_groups(
        config: dict[str, Any], anchor_transform, quickroad_audit: dict[str, Any]) -> dict[str, Any]:
    _require_b1_current_map()
    groups = build_pcg_group_specs(config)
    consumed_edge_labels = list(quickroad_audit["edge_actor_labels"])
    consumed_edge_digest = _validate_sha256(
        quickroad_audit["edge_geometry_sha256"], "consumed_edge_digest")
    edge_splines = _road_edge_spline_components(
        consumed_edge_labels, quickroad_audit["edge_spline_count"])
    minimum_road_clearance_cm = MINIMUM_ROAD_CLEARANCE_CM
    center, extent = _pcg_volume_center_extent(config, anchor_transform)
    library = unreal.GameXXKTownPCGAutomationLibrary
    authored = {}
    building_plots_by_id = {
        str(record["id"]): record for record in config["building_plots"]
    }

    for group in groups:
        record_transforms = [
            (
                record,
                _record_transform(
                    anchor_transform, group, record, building_plots_by_id),
            )
            for record in group["records"]
        ]
        transforms = [transform for _, transform in record_transforms]
        filtered_transforms, rejected_stable_ids = _filter_transforms_against_live_edges(
            record_transforms, edge_splines, minimum_road_clearance_cm)
        if len(filtered_transforms) != len(transforms):
            raise RuntimeError(
                f"live QuickRoad exclusion removed configured points for {group['key']}: "
                f"{len(transforms)} -> {len(filtered_transforms)}; "
                f"rejected_stable_ids={rejected_stable_ids}"
            )
        mesh = _load_asset(group["mesh_path"])
        material_override_paths = _mesh_material_override_paths(mesh, group["slot_materials"])
        existing_volume = _find_unique_actor(group["volume_label"])
        if existing_volume is not None:
            if B1_MANAGED_TAG not in _actor_tags(existing_volume):
                raise RuntimeError(f"refusing to reuse unowned PCG volume {group['volume_label']}")
            _parse_operation_payload(
                "clear_town_pcg",
                library.clear_town_pcg(group["volume_label"]),
            )
        base_seed = int(group["base_seed"])
        component_seed = int(group["component_seed"])
        graph = _parse_operation_payload(
            "create_or_update_town_pcg_graph_advanced",
            library.create_or_update_town_pcg_graph_advanced(
                group["graph_path"],
                group["mesh_path"],
                filtered_transforms,
                base_seed,
                material_override_paths,
                consumed_edge_labels,
                consumed_edge_digest,
                minimum_road_clearance_cm,
            ),
        )
        _validate_advanced_graph_contract(
            graph,
            group,
            len(filtered_transforms),
            material_override_paths,
            consumed_edge_labels,
            consumed_edge_digest,
            minimum_road_clearance_cm,
        )
        attach = _parse_operation_payload(
            "attach_town_pcg_graph_advanced",
            library.attach_town_pcg_graph_advanced(
                group["volume_label"],
                group["graph_path"],
                center,
                extent,
                component_seed,
            ),
        )
        volume = _find_unique_actor(group["volume_label"], required=True)
        _set_tags(volume, (
            B1_MANAGED_TAG,
            "QingshanB1PCG",
            f"QingshanB1PCG{group['category'].title().replace('_', '')}",
            f"QingshanB1PCGGroup_{group['key']}",
        ))
        authored[group["key"]] = {
            "category": group["category"],
            "volume_label": group["volume_label"],
            "graph_path": group["graph_path"],
            "mesh_path": group["mesh_path"],
            "point_count": len(filtered_transforms),
            "base_seed": base_seed,
            "component_seed": component_seed,
            "material_override_paths": material_override_paths,
            "consumed_edge_labels": consumed_edge_labels,
            "consumed_edge_digest": consumed_edge_digest,
            "minimum_road_clearance_cm": minimum_road_clearance_cm,
            "collision_enabled": bool(group["collision_enabled"]),
            "cull_end_cm": float(group["cull_end_cm"]),
            "graph": graph,
            "attach": attach,
        }
    return {
        "groups": authored,
        "group_count": len(authored),
        "consumed_edge_labels": consumed_edge_labels,
        "consumed_edge_digest": consumed_edge_digest,
    }


def _validate_advanced_graph_contract(
        graph_result: dict[str, Any],
        group: dict[str, Any],
        point_count: int,
        material_override_paths: list[str],
        consumed_edge_labels: list[str],
        consumed_edge_digest: str,
        minimum_road_clearance_cm: float) -> dict[str, Any]:
    raw_contract = graph_result.get("contract")
    try:
        contract = json.loads(str(raw_contract))
    except (TypeError, ValueError) as error:
        raise RuntimeError(f"advanced graph omitted valid contract JSON: {graph_result}") from error
    expected = {
        "schema_version": 1,
        "static_mesh_path": group["mesh_path"],
        "point_count": int(point_count),
        "base_seed": int(group["base_seed"]),
        "road_edge_geometry_digest": consumed_edge_digest,
        "minimum_road_clearance_cm": float(minimum_road_clearance_cm),
        "consumed_road_edge_actor_labels": sorted(consumed_edge_labels),
        "material_override_paths": material_override_paths,
    }
    for field, value in expected.items():
        if contract.get(field) != value:
            raise RuntimeError(
                f"advanced graph contract mismatch for {group['key']}.{field}: "
                f"{contract.get(field)!r} != {value!r}"
            )
    _validate_sha256(contract.get("point_seed_sha256"), "point_seed_sha256")
    return contract


def _generated_instance_count(label: str) -> int:
    actor = _find_unique_actor(label, required=True)
    return sum(
        int(component.get_instance_count())
        for component in actor.get_components_by_class(unreal.InstancedStaticMeshComponent)
    )


def _require_instance_transform(value, volume_label: str, instance_index: int):
    """UE Python maps bool + one out Transform to Transform on success, None on failure."""

    if value is None:
        raise RuntimeError(
            f"get_instance_transform failed for {volume_label} instance {instance_index}"
        )
    return value


def _require_component_static_mesh(component, volume_label: str):
    try:
        static_mesh = component.get_editor_property("static_mesh")
    except Exception as error:
        raise RuntimeError(
            f"generated component for {volume_label} did not expose static_mesh"
        ) from error
    if static_mesh is None:
        raise RuntimeError(f"generated component for {volume_label} has no StaticMesh")
    return static_mesh


def _apply_generated_component_policy(group: dict[str, Any]) -> dict[str, Any]:
    actor = _find_unique_actor(group["volume_label"], required=True)
    components = list(actor.get_components_by_class(unreal.InstancedStaticMeshComponent))
    if not components:
        raise RuntimeError(f"generated PCG volume has no ISM/HISM components: {group['volume_label']}")
    collision_enabled = bool(group["collision_enabled"])
    cull_end_cm = float(group["cull_end_cm"])
    if not math.isfinite(cull_end_cm) or cull_end_cm <= 0.0:
        raise RuntimeError(f"cull_end_cm must be finite and positive: {group}")
    collision_mode = (
        unreal.CollisionEnabled.QUERY_AND_PHYSICS
        if collision_enabled else unreal.CollisionEnabled.NO_COLLISION
    )
    instance_count = 0
    maximum_building_bottom_error = 0.0
    for component in components:
        component.set_collision_enabled(collision_mode)
        component.set_cull_distances(int(cull_end_cm * 0.7), int(cull_end_cm))
        component_instance_count = int(component.get_instance_count())
        if group["category"] == "building":
            static_mesh = _require_component_static_mesh(
                component, group["volume_label"])
            bounds = static_mesh.get_bounds()
            local_bottom_z = float(bounds.origin.z) - float(bounds.box_extent.z)
            for index in range(component_instance_count):
                instance_transform = _require_instance_transform(
                    component.get_instance_transform(index, world_space=True),
                    group["volume_label"],
                    index,
                )
                bottom_world = unreal.MathLibrary.transform_location(
                    instance_transform, unreal.Vector(0.0, 0.0, local_bottom_z))
                ground = _trace_ground_at_world_location(
                    bottom_world, actors_to_ignore=[actor])
                building_bottom_error_cm = abs(float(bottom_world.z) - float(ground.z))
                maximum_building_bottom_error = max(
                    maximum_building_bottom_error, building_bottom_error_cm)
        instance_count += component_instance_count
    if maximum_building_bottom_error > MAX_BUILDING_BOTTOM_ERROR_CM:
        raise RuntimeError(
            f"building_bottom_error_cm exceeds {MAX_BUILDING_BOTTOM_ERROR_CM}: "
            f"{maximum_building_bottom_error}"
        )
    return {
        "component_count": len(components),
        "instance_count": instance_count,
        "collision_enabled": collision_enabled,
        "cull_end_cm": cull_end_cm,
        "building_bottom_error_cm": maximum_building_bottom_error,
    }


def _set_primitive_policy(component, *, collision_enabled: bool, cull_end_cm: float) -> None:
    collision = (
        unreal.CollisionEnabled.QUERY_AND_PHYSICS
        if collision_enabled else unreal.CollisionEnabled.NO_COLLISION
    )
    profile_name = "BlockAll" if collision_enabled else "NoCollision"
    component.modify()
    component.set_collision_profile_name(unreal.Name(profile_name))
    component.set_collision_enabled(collision)
    if component.get_collision_enabled() != collision:
        raise RuntimeError(
            f"semantic collision readback mismatch: {profile_name}, {collision}"
        )
    component.set_cull_distance(float(cull_end_cm))


def _create_semantic_mesh_actor(
        label: str,
        mesh_path: str,
        location,
        rotation,
        scale,
        category_tag: str,
        *,
        material_path: str | None = None,
        collision_enabled: bool,
        cull_end_cm: float):
    _require_b1_current_map()
    actor = _find_unique_actor(label, unreal.StaticMeshActor)
    if actor is not None and B1_MANAGED_TAG not in _actor_tags(actor):
        raise RuntimeError(f"refusing to update unowned semantic mesh actor {label!r}")
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, location, rotation)
        if actor is None:
            raise RuntimeError(f"failed to spawn semantic mesh actor {label}")
        actor.set_actor_label(label)
    actor.set_actor_location(location, False, False)
    actor.set_actor_rotation(rotation, False)
    actor.set_actor_scale3d(unreal.Vector(*map(float, scale)))
    component = actor.static_mesh_component
    component.set_static_mesh(_load_asset(mesh_path))
    if material_path:
        component.set_material(0, _load_asset(material_path))
    _set_primitive_policy(
        component,
        collision_enabled=collision_enabled,
        cull_end_cm=cull_end_cm,
    )
    _set_tags(actor, (B1_MANAGED_TAG, category_tag))
    _set_tags(component, (B1_MANAGED_TAG, category_tag))
    return actor


def _rotated_local_offset(anchor: dict[str, Any], forward: float, right: float, z: float = 0.0):
    radians = math.radians(float(anchor["yaw_degrees"]))
    cos_yaw, sin_yaw = math.cos(radians), math.sin(radians)
    return (
        float(anchor["location_cm"][0]) + cos_yaw * forward - sin_yaw * right,
        float(anchor["location_cm"][1]) + sin_yaw * forward + cos_yaw * right,
        float(anchor["location_cm"][2]) + z,
    )


def _world_with_z(world_location, z: float):
    return unreal.Vector(float(world_location.x), float(world_location.y), float(z))


def _point_segment_distance_xy(point, first, second) -> float:
    px, py = float(point[0]), float(point[1])
    ax, ay = float(first[0]), float(first[1])
    bx, by = float(second[0]), float(second[1])
    dx, dy = bx - ax, by - ay
    length_squared = dx * dx + dy * dy
    if length_squared <= 1.0e-12:
        return math.hypot(px - ax, py - ay)
    alpha = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length_squared))
    return math.hypot(px - (ax + alpha * dx), py - (ay + alpha * dy))


def _river_surface_top_world_z_at_local_point(
        config: dict[str, Any], anchor_transform, local_point) -> float:
    river = config["river_splines"][0]
    candidates = []
    for first, second in zip(river["points_cm"], river["points_cm"][1:]):
        distance = _point_segment_distance_xy(local_point, first, second)
        if distance <= float(river["width_cm"]) * 0.5 + 1.0e-6:
            midpoint = tuple(
                (float(first[index]) + float(second[index])) * 0.5
                for index in range(3)
            )
            world_midpoint = local_to_world(anchor_transform, midpoint)
            candidates.append(float(world_midpoint.z) + RIVER_SEGMENT_HALF_THICKNESS_CM)
    if not candidates:
        raise RuntimeError("MainBridgeAnchor does not overlap the authored river surface")
    return max(candidates)


def _compute_bridge_deck_center_z(
        *, approach_ground_z_values, crossing_water_top_z: float,
        deck_half_extent_z: float) -> float:
    approach_values = [float(value) for value in approach_ground_z_values]
    water_top = float(crossing_water_top_z)
    half_extent = float(deck_half_extent_z)
    if (
        not approach_values
        or not all(math.isfinite(value) for value in approach_values)
        or not math.isfinite(water_top)
        or not math.isfinite(half_extent)
        or half_extent <= 0.0
    ):
        raise ValueError("bridge deck height inputs must be finite and nonempty")
    return max(
        max(approach_values) + BRIDGE_APPROACH_LIFT_CM,
        water_top + BRIDGE_WATER_CLEARANCE_CM + half_extent,
    )


def _create_bridge_assembly(config: dict[str, Any], anchor_transform) -> list[str]:
    bridge = next(item for item in config["fixed_anchors"] if item["id"] == "MainBridgeAnchor")
    timber = config["building_materials"]["shared_slot_materials"]["Timber"]
    rock_mesh = config["asset_catalog"]["prop_meshes"]["rock"]
    cube_mesh = "/Engine/BasicShapes/Cube"
    approach_world = [
        _ground_snap_world(anchor_transform, _rotated_local_offset(bridge, direction * 1450.0, 0.0))
        for direction in (-1.0, 1.0)
    ]
    cube_bounds = _load_asset(cube_mesh).get_bounds()
    deck_half_extent_z = float(cube_bounds.box_extent.z) * 1.2
    crossing_water_top_z = _river_surface_top_world_z_at_local_point(
        config, anchor_transform, bridge["location_cm"])
    deck_z = _compute_bridge_deck_center_z(
        approach_ground_z_values=[float(point.z) for point in approach_world],
        crossing_water_top_z=crossing_water_top_z,
        deck_half_extent_z=deck_half_extent_z,
    )
    yaw = _anchor_local_yaw(anchor_transform, float(bridge["yaw_degrees"]))
    rotation = unreal.Rotator(pitch=0.0, yaw=yaw, roll=0.0)
    specs = [
        ("QS_B1_Bridge_Deck", 0.0, 0.0, 0.0, (24.0, 9.0, 1.2), "QingshanB1BridgeDeck"),
        ("QS_B1_Bridge_Rail_L", 0.0, -410.0, 150.0, (24.0, 0.8, 1.8), "QingshanB1BridgeRail"),
        ("QS_B1_Bridge_Rail_R", 0.0, 410.0, 150.0, (24.0, 0.8, 1.8), "QingshanB1BridgeRail"),
    ]
    for forward in (-1000.0, 1000.0):
        for right in (-380.0, 380.0):
            suffix = f"{'W' if forward < 0 else 'E'}_{'L' if right < 0 else 'R'}"
            specs.append((
                f"QS_B1_Bridge_Post_{suffix}", forward, right, 100.0,
                (1.0, 1.0, 4.2), "QingshanB1BridgePost",
            ))
    labels = []
    for label, forward, right, z_offset, scale, category in specs:
        local = _rotated_local_offset(bridge, forward, right)
        base_world = local_to_world(anchor_transform, local)
        _create_semantic_mesh_actor(
            label,
            cube_mesh,
            _world_with_z(base_world, deck_z + z_offset),
            rotation,
            scale,
            category,
            material_path=timber,
            collision_enabled=True,
            cull_end_cm=30000.0,
        )
        labels.append(label)
    for direction in (-1.0, 1.0):
        for right in (-300.0, 300.0):
            label = f"QS_B1_Bridge_ApproachStone_{'W' if direction < 0 else 'E'}_{'L' if right < 0 else 'R'}"
            local = _rotated_local_offset(bridge, direction * 1450.0, right)
            location = _ground_snap_world(anchor_transform, local)
            _create_semantic_mesh_actor(
                label,
                rock_mesh,
                location,
                rotation,
                (0.9, 0.7, 0.45),
                "QingshanB1BridgeApproachStone",
                collision_enabled=True,
                cull_end_cm=22000.0,
            )
            labels.append(label)
    return labels


def _create_river_and_dock(config: dict[str, Any], anchor_transform) -> dict[str, list[str]]:
    river = config["river_splines"][0]
    _create_or_update_b1_spline(
        "QS_B1_River_Centerline",
        anchor_transform,
        river["points_cm"],
        ("TownPCG_River", "QingshanB1River"),
    )
    water_material = f"{B1_ASSET_ROOT}/Materials/MI_QS_B1_Water_Teal"
    river_labels = []
    for index, (first, second) in enumerate(zip(river["points_cm"], river["points_cm"][1:])):
        world_first = local_to_world(anchor_transform, first)
        world_second = local_to_world(anchor_transform, second)
        delta_x = float(world_second.x) - float(world_first.x)
        delta_y = float(world_second.y) - float(world_first.y)
        length = math.hypot(delta_x, delta_y)
        label = f"QS_B1_River_Segment_{index:03d}"
        _create_semantic_mesh_actor(
            label,
            "/Engine/BasicShapes/Cube",
            unreal.Vector(
                (float(world_first.x) + float(world_second.x)) * 0.5,
                (float(world_first.y) + float(world_second.y)) * 0.5,
                (float(world_first.z) + float(world_second.z)) * 0.5,
            ),
            unreal.Rotator(
                pitch=0.0,
                yaw=math.degrees(math.atan2(delta_y, delta_x)),
                roll=0.0,
            ),
            (length / 100.0, float(river["width_cm"]) / 100.0, 0.2),
            "QingshanB1River",
            material_path=water_material,
            collision_enabled=False,
            cull_end_cm=50000.0,
        )
        river_labels.append(label)

    dock = next(item for item in config["fixed_anchors"] if item["id"] == "SouthDockAnchor")
    dock_rotation = unreal.Rotator(
        pitch=0.0,
        yaw=_anchor_local_yaw(anchor_transform, float(dock["yaw_degrees"])),
        roll=0.0,
    )
    timber = config["building_materials"]["shared_slot_materials"]["Timber"]
    dock_world = _ground_snap_world(anchor_transform, dock["location_cm"], z_offset_cm=60.0)
    dock_labels = []
    for label, right, scale in (
        ("QS_B1_Dock_Deck", 0.0, (18.0, 12.0, 1.0)),
        ("QS_B1_Dock_Rail_L", -560.0, (18.0, 0.7, 1.5)),
        ("QS_B1_Dock_Rail_R", 560.0, (18.0, 0.7, 1.5)),
    ):
        local = _rotated_local_offset(dock, 0.0, right)
        location = local_to_world(anchor_transform, local)
        location = _world_with_z(location, float(dock_world.z) + (120.0 if right else 0.0))
        _create_semantic_mesh_actor(
            label,
            "/Engine/BasicShapes/Cube",
            location,
            dock_rotation,
            scale,
            "QingshanB1Dock",
            material_path=timber,
            collision_enabled=True,
            cull_end_cm=30000.0,
        )
        dock_labels.append(label)
    return {"river": river_labels, "dock": dock_labels}


def _camera_component(actor):
    try:
        return actor.get_camera_component()
    except Exception:
        component = actor.get_editor_property("camera_component")
        if component is None:
            raise RuntimeError("camera actor has no camera component")
        return component


def _create_review_cameras(config: dict[str, Any], anchor_transform) -> dict[str, Any]:
    result = {}
    records = {record["id"]: record for record in config["cameras"]}
    if set(records) != set(CAMERA_LABELS):
        raise RuntimeError("B1 camera records do not match exact review labels")
    for label in CAMERA_LABELS:
        record = records[label]
        actor = _find_unique_actor(label, unreal.CameraActor)
        if actor is not None and B1_MANAGED_TAG not in _actor_tags(actor):
            raise RuntimeError(f"refusing to update unowned review camera {label}")
        location = local_to_world(anchor_transform, record["location_cm"])
        target = local_to_world(anchor_transform, record["target_cm"])
        rotation = unreal.MathLibrary.find_look_at_rotation(location, target)
        if actor is None:
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
                unreal.CameraActor, location, rotation)
            if actor is None:
                raise RuntimeError(f"failed to spawn review camera {label}")
            actor.set_actor_label(label)
        actor.set_actor_location(location, False, False)
        actor.set_actor_rotation(rotation, False)
        component = _camera_component(actor)
        component.set_editor_property("field_of_view", float(record["fov_degrees"]))
        _set_tags(actor, (B1_MANAGED_TAG, "QingshanB1Camera"))
        result[label] = {
            "location": _vector_payload(location),
            "target": _vector_payload(target),
            "field_of_view": float(record["fov_degrees"]),
        }
    return result


def _flipbook_component(actor):
    try:
        return actor.get_render_component()
    except Exception:
        component = actor.get_editor_property("render_component")
        if component is None:
            raise RuntimeError("PaperFlipbookActor has no render component")
        return component


def _create_animated_plants(config: dict[str, Any], anchor_transform) -> dict[str, Any]:
    records = [
        record for record in config["vegetation_records"]
        if record["render_mode"] == "animated_flipbook"
    ]
    if len(records) != EXPECTED_ANIMATED_PLANT_COUNT:
        raise RuntimeError("animated plant config count must be exactly 30")
    flipbook_path = config["asset_catalog"]["plant_flipbook"]
    if not flipbook_path.endswith("FB_QS_B1_Plant_Sway"):
        raise RuntimeError("animated plants must use FB_QS_B1_Plant_Sway")
    flipbook = _load_asset(flipbook_path)
    labels = []
    pending_playback = []
    for record in sorted(records, key=lambda item: item["id"]):
        label = f"QS_B1_{record['id']}"
        actor = _find_unique_actor(label, unreal.PaperFlipbookActor)
        if actor is not None and B1_MANAGED_TAG not in _actor_tags(actor):
            raise RuntimeError(f"refusing to update unowned PaperFlipbookActor {label}")
        location = _ground_snap_world(anchor_transform, record["location_cm"])
        rotation = unreal.Rotator(
            pitch=0.0,
            yaw=_anchor_local_yaw(anchor_transform, float(record["yaw_degrees"])),
            roll=0.0,
        )
        if actor is None:
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
                unreal.PaperFlipbookActor, location, rotation)
            if actor is None:
                raise RuntimeError(f"failed to spawn PaperFlipbookActor {label}")
            actor.set_actor_label(label)
        actor.set_actor_location(location, False, False)
        actor.set_actor_rotation(rotation, False)
        scale = float(record["scale"])
        actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        component = _flipbook_component(actor)
        component.stop()
        component.set_flipbook(flipbook)
        component.set_looping(True)
        component.set_play_rate(1.0)
        start_frame = int(record["start_frame"])
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_cull_distance(ANIMATED_PLANT_CULL_CM)
        _set_tags(actor, (B1_MANAGED_TAG, "QingshanB1AnimatedPlant"))
        _set_tags(component, (B1_MANAGED_TAG, "QingshanB1AnimatedPlant"))
        pending_playback.append((component, start_frame))
        labels.append(label)
    for component, start_frame in pending_playback:
        component.set_playback_position_in_frames(start_frame, False)
    for component, _start_frame in pending_playback:
        component.play()
    return {"count": len(labels), "labels": labels, "flipbook": flipbook_path}


def _create_unique_semantic_actors(config: dict[str, Any], anchor_transform) -> dict[str, Any]:
    return {
        "bridge_labels": _create_bridge_assembly(config, anchor_transform),
        "river_dock": _create_river_and_dock(config, anchor_transform),
        "animated_plants": _create_animated_plants(config, anchor_transform),
        "cameras": _create_review_cameras(config, anchor_transform),
    }


def _audit_quickroad_now(config: dict[str, Any], anchor_transform) -> dict[str, Any]:
    _require_b1_current_map()
    landscape = config["landscape"]
    raw = _call_quickroad(
        "audit_infrastructure",
        unreal.Quick_RoadAutomationLibrary.audit_infrastructure,
        landscape["label"],
        _name(landscape["edit_layer"]),
        _name(config["quickroad"]["combined_tag_expression"]),
    )
    center = local_to_world(anchor_transform, landscape["center_local_cm"])
    return _validate_quickroad_audit(raw, config, center)


def _validate_live_bridge_gap(
        config: dict[str, Any], anchor_transform, quickroad_audit: dict[str, Any]) -> dict[str, Any]:
    bridge = next(item for item in config["fixed_anchors"] if item["id"] == "MainBridgeAnchor")
    center = local_to_world(anchor_transform, bridge["location_cm"])
    radius = float(bridge["protected_radius_cm"])
    labels = (
        "QS_B1_QR_Input_QS_B1_Main_00",
        "QS_B1_QR_Input_QS_B1_Main_01",
    )
    distances = []
    for label in labels:
        actor = _find_unique_actor(label, required=True)
        splines = list(actor.get_components_by_class(unreal.SplineComponent))
        if len(splines) != 1:
            raise RuntimeError(f"bridge-gap input {label} must own exactly one spline")
        closest = splines[0].find_location_closest_to_world_location(
            center, unreal.SplineCoordinateSpace.WORLD)
        distance = math.hypot(float(closest.x) - float(center.x), float(closest.y) - float(center.y))
        if distance < radius - 2.0:
            raise RuntimeError(f"QuickRoad input spans MainBridgeAnchor gap: {label}, {distance}")
        distances.append(distance)
    edge_actor_labels = list(quickroad_audit["edge_actor_labels"])
    edge_spline_count = int(quickroad_audit["edge_spline_count"])
    generated_edges = _road_edge_spline_components(edge_actor_labels, edge_spline_count)
    generated_edge_closest_distances_cm = []
    for spline in generated_edges:
        closest = spline.find_location_closest_to_world_location(
            center, unreal.SplineCoordinateSpace.WORLD)
        distance = math.hypot(
            float(closest.x) - float(center.x),
            float(closest.y) - float(center.y),
        )
        if distance < radius - 2.0:
            raise RuntimeError(
                f"generated QuickRoad edge enters MainBridgeAnchor gap: {distance} < {radius}"
            )
        generated_edge_closest_distances_cm.append(distance)
    return {
        "labels": list(labels),
        "closest_distances_cm": distances,
        "radius_cm": radius,
        "edge_actor_labels": edge_actor_labels,
        "edge_spline_count": edge_spline_count,
        "generated_edge_closest_distances_cm": generated_edge_closest_distances_cm,
    }


def _require_unique_b1_managed_labels() -> dict[str, int]:
    counts: dict[str, int] = {}
    for actor in _all_actors():
        if B1_MANAGED_TAG in _actor_tags(actor):
            label = _actor_label(actor)
            counts[label] = counts.get(label, 0) + 1
    duplicates = {label: count for label, count in counts.items() if count != 1}
    if duplicates:
        raise RuntimeError(f"B1 managed label uniqueness failed: {duplicates}")
    return counts


def begin_finalize(config: dict[str, Any]) -> dict[str, Any]:
    _require_b1_current_map()
    _validate_dirty_scope()
    protected_before = _snapshot_protected_actors()
    gate = _find_unique_actor("QingshanInn_TownExit", required=True)
    anchor_transform = _validate_north_gate_anchor(gate)
    quickroad = _audit_quickroad_now(config, anchor_transform)
    bridge_gap = _validate_live_bridge_gap(config, anchor_transform, quickroad)
    pcg = _author_pcg_groups(config, anchor_transform, quickroad)
    unique_actors = _create_unique_semantic_actors(config, anchor_transform)
    generation = {}
    for key, group in sorted(pcg["groups"].items()):
        generation[key] = _parse_operation_payload(
            "generate_town_pcg",
            unreal.GameXXKTownPCGAutomationLibrary.generate_town_pcg(
                group["volume_label"]),
        )
    if len(generation) != EXPECTED_PCG_GROUP_TOTAL:
        raise RuntimeError("finalize_begin must schedule exactly 34 PCG groups")
    protected_after = _assert_protected_actors(protected_before)
    return {
        "success": True,
        "phase": PHASE_FINALIZE_BEGIN,
        "state": "pending",
        "complete": False,
        "pending": True,
        "graph_count": len(generation),
        "generation": generation,
        "pcg": pcg,
        "unique_actors": unique_actors,
        "bridge_gap": bridge_gap,
        "quickroad": quickroad,
        "quickroad_status": quickroad["quickroad_status"],
        "protected_before": protected_before,
        "protected_after": protected_after,
        "error": "",
    }


def _animated_plant_live_count() -> int:
    return sum(
        B1_MANAGED_TAG in _actor_tags(actor)
        and "QingshanB1AnimatedPlant" in _actor_tags(actor)
        and isinstance(actor, unreal.PaperFlipbookActor)
        for actor in _all_actors()
    )


def poll_finalize(config: dict[str, Any]) -> dict[str, Any]:
    _require_b1_current_map()
    _validate_dirty_scope()
    protected_before = _snapshot_protected_actors()
    gate = _find_unique_actor("QingshanInn_TownExit", required=True)
    anchor_transform = _validate_north_gate_anchor(gate)
    quickroad = _audit_quickroad_now(config, anchor_transform)
    bridge_gap = _validate_live_bridge_gap(config, anchor_transform, quickroad)
    groups = build_pcg_group_specs(config)
    statuses = {}
    for group in groups:
        statuses[group["key"]] = _parse_operation_payload(
            "get_town_pcg_status",
            unreal.GameXXKTownPCGAutomationLibrary.get_town_pcg_status(
                group["volume_label"]),
        )
    common = {
        "phase": PHASE_FINALIZE_POLL,
        "graph_count": len(groups),
        "statuses": statuses,
        "bridge_gap": bridge_gap,
        "quickroad": quickroad,
        "quickroad_status": quickroad["quickroad_status"],
    }
    if any(status.get("generating") is True for status in statuses.values()):
        protected_after = _assert_protected_actors(protected_before)
        return {
            "success": True,
            "state": "pending",
            "complete": False,
            "pending": True,
            "protected_before": protected_before,
            "protected_after": protected_after,
            "error": "",
            **common,
        }
    failed = {
        key: status for key, status in statuses.items()
        if status.get("generated") is not True
    }
    if failed:
        protected_after = _assert_protected_actors(protected_before)
        return {
            "success": False,
            "state": "failed",
            "complete": False,
            "pending": False,
            "protected_before": protected_before,
            "protected_after": protected_after,
            "error": f"PCG generation stopped before completion: {failed}",
            **common,
        }

    group_results = {}
    actual_counts = {
        "buildings": 0,
        "props": 0,
        "static_plants": 0,
        "mountains": 0,
        "animated_plants": _animated_plant_live_count(),
    }
    category_key = {
        "building": "buildings",
        "prop": "props",
        "static_plant": "static_plants",
        "mountain": "mountains",
    }
    maximum_building_bottom_error = 0.0
    for group in groups:
        result = _apply_generated_component_policy(group)
        independently_measured_instances = _generated_instance_count(group["volume_label"])
        if independently_measured_instances != result["instance_count"]:
            raise RuntimeError(
                f"generated instance recount mismatch for {group['key']}: "
                f"{independently_measured_instances} != {result['instance_count']}"
            )
        expected_count = len(group["records"])
        if result["instance_count"] != expected_count:
            raise RuntimeError(
                f"PCG instance count mismatch for {group['key']}: "
                f"{result['instance_count']} != {expected_count}"
            )
        actual_counts[category_key[group["category"]]] += result["instance_count"]
        maximum_building_bottom_error = max(
            maximum_building_bottom_error,
            float(result["building_bottom_error_cm"]),
        )
        group_results[group["key"]] = result
    if actual_counts != EXPECTED_POPULATION_COUNTS:
        raise RuntimeError(
            f"B1 live population counts mismatch: {actual_counts} != {EXPECTED_POPULATION_COUNTS}"
        )
    if maximum_building_bottom_error > MAX_BUILDING_BOTTOM_ERROR_CM:
        raise RuntimeError(
            f"building_bottom_error_cm exceeds {MAX_BUILDING_BOTTOM_ERROR_CM}: "
            f"{maximum_building_bottom_error}"
        )
    managed_label_counts = _require_unique_b1_managed_labels()
    protected_after = _assert_protected_actors(protected_before)
    save = _save_b1_only(config)
    return {
        "success": True,
        "state": "complete",
        "complete": True,
        "pending": False,
        "actual_counts": actual_counts,
        "group_results": group_results,
        "building_bottom_error_cm": maximum_building_bottom_error,
        "managed_label_counts": managed_label_counts,
        "protected_before": protected_before,
        "protected_after": protected_after,
        "save": save,
        "error": "",
        **common,
    }


def _run_guarded_phase(phase_callable) -> dict[str, Any]:
    config = load_config()
    before_hashes = validate_protected_file_hashes(config["protected_files"], PROJECT_ROOT)
    if len(before_hashes) != EXPECTED_PROTECTED_FILE_COUNT:
        raise RuntimeError("protected raw hash contract must contain exactly three files")
    try:
        result = phase_callable(config)
    except Exception as phase_error:
        after_hashes = validate_protected_file_hashes(
            config["protected_files"], PROJECT_ROOT)
        if before_hashes != after_hashes:
            raise RuntimeError(
                f"protected raw bytes changed during failed B1 phase: "
                f"before={before_hashes}, after={after_hashes}"
            ) from phase_error
        raise
    after_hashes = validate_protected_file_hashes(config["protected_files"], PROJECT_ROOT)
    if before_hashes != after_hashes:
        raise RuntimeError(
            f"protected raw bytes changed during B1 phase: "
            f"before={before_hashes}, after={after_hashes}"
        )
    result["protected_file_hashes_before"] = before_hashes
    result["protected_file_hashes_after"] = after_hashes
    return result


def assemble_b1(phase: str = PHASE_SETUP) -> dict[str, Any]:
    _require_unreal()
    if phase == "setup":
        return _run_guarded_phase(setup_b1)
    if phase == "infrastructure":
        return _run_guarded_phase(build_infrastructure)
    if phase == "finalize_begin":
        return _run_guarded_phase(begin_finalize)
    if phase == "finalize_poll":
        return _run_guarded_phase(poll_finalize)
    raise ValueError(f"unsupported phase {phase!r}; expected one of {PHASES}")


def _phase_from_argv(argv: list[str]) -> str:
    if "--phase" not in argv:
        raise ValueError("--phase is required")
    index = argv.index("--phase")
    if index + 1 >= len(argv):
        raise ValueError("--phase requires a value")
    phase = argv[index + 1]
    if phase not in PHASES:
        raise ValueError(f"unsupported phase {phase!r}; expected one of {PHASES}")
    return phase


def _compact_json(payload: Any) -> str:
    try:
        return json.dumps(
            payload,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
            allow_nan=False,
        )
    except Exception as error:
        return json.dumps({
            "success": False,
            "state": "failed",
            "complete": False,
            "pending": False,
            "error": f"result serialization failed: {type(error).__name__}",
        }, sort_keys=True, separators=(",", ":"))


def main() -> None:
    phase = ""
    try:
        phase = _phase_from_argv(sys.argv[1:])
        result = assemble_b1(phase)
    except Exception as error:
        if unreal is not None:
            unreal.log_error(f"GameXXK Qingshan B1 assembly failed: {error}")
        result = {
            "success": False,
            "phase": phase,
            "state": "failed",
            "complete": False,
            "pending": False,
            "error": str(error),
        }
    print(f"{ASSEMBLY_MARKER} {_compact_json(result)}")


if __name__ == "__main__":
    main()
