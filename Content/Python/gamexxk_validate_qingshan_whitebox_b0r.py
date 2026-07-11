"""Read-only structural validator for the Qingshan B0R PCG whitebox."""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path
from typing import Any

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS_DIR = PROJECT_ROOT / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

from gamexxk_qingshan_whitebox_config import load_config
from qingshan_whitebox_acceptance import generate_seeded_proxy_transforms, canonical_layout_hash


SOURCE_MAP = "/Game/GameXXK/Maps/L_QingshanInn"
WHITEBOX_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R"
MANAGED_TAG = "QingshanB0RManaged"
PCG_GENERATED_COMPONENT_TAG = "PCG Generated Component"
CAMERA_IDS = (
    "CAM_QS_GATE_ARRIVAL", "CAM_QS_TOWN_CORE",
    "CAM_QS_MAIN_BRIDGE", "CAM_QS_SOUTH_DOCK",
)
ROAD_LABELS = {
    "Road_Main": "QS_B0R_Road_Main",
    "Road_Core_North": "QS_B0R_Road_Core_North",
    "Road_Core_South": "QS_B0R_Road_Core_South",
}
ROAD_TAGS = {
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
GRAPH_PATHS = {
    key: f"/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_{key.title()}"
    for key in PCG_LABELS
}
REQUIRED_LABELS = (
    "QS_B0R_World_Bounds", *ROAD_LABELS.values(), *RIVER_LABELS.values(),
    "QS_B0R_Bridge_Main", "QS_B0R_Dock_South", *TERRAIN_LABELS.values(),
    *PCG_LABELS.values(), *CAMERA_IDS,
)


def _new_result() -> dict[str, Any]:
    return {
        "marker": "GAMEXXK_QINGSHAN_B0R_VALIDATION",
        "current_map": "", "source_map_clean": False,
        "protected_actor_snapshot": {}, "managed_label_uniqueness": {},
        "road_spline_count": 0, "river_spline_count": 0,
        "bridge_road_horizontal_distance_cm": None,
        "bridge_river_horizontal_distance_cm": None,
        "building_instance_count": 0, "foliage_instance_count": 0,
        "mountain_instance_count": 0, "crossing_count": 0,
        "runtime_generation_disabled": False,
        "camera_count": 0, "camera_ids": [], "terrain_zone_count": 0,
        "player_gate_facing_angle_degrees": None, "layout_sha256": "",
        "errors": [], "success": False,
    }


def _error(result: dict, check: str, message: str, **details) -> None:
    result["errors"].append({"check": check, "message": message, **details})


def _package_path(value) -> str:
    if value is None:
        return ""
    try:
        value = value.get_outermost()
    except Exception:
        pass
    return str(value.get_path_name()).split(".", 1)[0]


def _dirty_maps() -> list[str]:
    return sorted(_package_path(value) for value in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())


def _dirty_content() -> list[str]:
    return sorted(_package_path(value) for value in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())


def _current_map() -> str:
    return _package_path(unreal.EditorLevelLibrary.get_editor_world())


def _ensure_whitebox_read_only() -> None:
    if _current_map() == WHITEBOX_MAP:
        return
    if _dirty_maps() or _dirty_content():
        raise RuntimeError("refusing whitebox map switch while packages are dirty")
    if not unreal.EditorLoadingAndSavingUtils.load_map(WHITEBOX_MAP):
        raise RuntimeError(f"failed to load only {WHITEBOX_MAP}")


def _tags(value) -> set[str]:
    try:
        raw = value.get_editor_property("component_tags")
    except Exception:
        raw = value.get_editor_property("tags")
    return {str(tag) for tag in (raw or [])}


def _label(actor) -> str:
    return str(actor.get_actor_label())


def _actors() -> list:
    return list(unreal.EditorLevelLibrary.get_all_level_actors())


def _matches(actors, label: str) -> list:
    return [actor for actor in actors if _label(actor) == label]


def _vector(value) -> list[float]:
    return [float(value.x), float(value.y), float(value.z)]


def _rotation(rotator) -> list[float]:
    return [float(rotator.roll), float(rotator.pitch), float(rotator.yaw)]


def _actor_transform(actor) -> dict:
    return {
        "location_cm": _vector(actor.get_actor_location()),
        "rotation_degrees": _rotation(actor.get_actor_rotation()),
        "scale": _vector(actor.get_actor_scale3d()),
    }


def _instance_transform(transform) -> dict:
    return {
        "location_cm": _vector(transform.translation),
        "rotation_degrees": _rotation(transform.rotation.rotator()),
        "scale": _vector(transform.scale3d),
    }


def _angle_delta_degrees(first: float, second: float) -> float:
    values = (float(first), float(second))
    if not all(math.isfinite(value) for value in values):
        return math.inf
    return abs((values[0] - values[1] + 180.0) % 360.0 - 180.0)


def _transform_matches(actual, expected, location_tolerance=1.0, rotation_tolerance=0.1, scale_tolerance=0.001) -> bool:
    try:
        actual_values = [float(value) for field in ("location_cm", "rotation_degrees", "scale") for value in actual[field]]
        expected_values = [float(value) for field in ("location_cm", "rotation_degrees", "scale") for value in expected[field]]
        if len(actual_values) != 9 or len(expected_values) != 9 or not all(math.isfinite(value) for value in actual_values + expected_values):
            return False
        location_ok = all(abs(actual["location_cm"][i] - expected["location_cm"][i]) <= location_tolerance for i in range(3))
        rotation_ok = all(_angle_delta_degrees(actual["rotation_degrees"][i], expected["rotation_degrees"][i]) <= rotation_tolerance for i in range(3))
        scale_ok = all(abs(actual["scale"][i] - expected["scale"][i]) <= scale_tolerance for i in range(3))
        return location_ok and rotation_ok and scale_ok
    except (KeyError, TypeError, ValueError, OverflowError):
        return False


def _horizontal_distance_to_polyline(point, points) -> float:
    if len(points) < 2:
        raise ValueError("polyline requires at least two points")
    px, py = float(point[0]), float(point[1])
    if not all(math.isfinite(value) for value in (px, py)):
        raise ValueError("point must be finite")
    distances = []
    for start, end in zip(points, points[1:]):
        ax, ay, bx, by = float(start[0]), float(start[1]), float(end[0]), float(end[1])
        if not all(math.isfinite(value) for value in (ax, ay, bx, by)):
            raise ValueError("polyline must be finite")
        dx, dy = bx - ax, by - ay
        denominator = dx * dx + dy * dy
        amount = 0.0 if denominator == 0.0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / denominator))
        distances.append(math.hypot(px - (ax + amount * dx), py - (ay + amount * dy)))
    return min(distances)


def _north_local_location(anchor, local) -> list[float]:
    origin = _vector(anchor.get_actor_location())
    yaw = math.radians(float(anchor.get_actor_rotation().yaw))
    x, y, z = (float(value) for value in local)
    return [origin[0] + x * math.cos(yaw) - y * math.sin(yaw), origin[1] + x * math.sin(yaw) + y * math.cos(yaw), origin[2] + z]


def _north_yaw(anchor, local_yaw: float) -> float:
    return float(anchor.get_actor_rotation().yaw) + float(local_yaw)


def _look_at_rotation(location, target) -> list[float]:
    dx, dy, dz = (float(target[i]) - float(location[i]) for i in range(3))
    return [0.0, math.degrees(math.atan2(dz, math.hypot(dx, dy))), math.degrees(math.atan2(dy, dx))]


def _expected_proxy_transforms(config, anchor) -> dict[str, list[dict]]:
    layout = generate_seeded_proxy_transforms(config)
    buildings = [{
        "stable_id": plot["id"],
        "location_cm": _north_local_location(anchor, [plot["location_cm"][0], plot["location_cm"][1], plot["location_cm"][2] + plot["size_cm"][2] / 2.0]),
        "rotation_degrees": [0.0, 0.0, _north_yaw(anchor, plot["yaw_degrees"])],
        "scale": [float(value) / 100.0 for value in plot["size_cm"]],
    } for plot in config["building_plots"]]
    result = {"buildings": buildings}
    for category in ("foliage", "mountains"):
        records = []
        for record in layout[category]:
            local = list(record["location_cm"])
            if category == "mountains":
                local[2] += abs(float(record["scale"][2])) * 50.0
            records.append({
                "stable_id": record["stable_id"], "location_cm": _north_local_location(anchor, local),
                "rotation_degrees": [float(record["rotation_degrees"][0]), float(record["rotation_degrees"][1]), _north_yaw(anchor, record["rotation_degrees"][2])],
                "scale": [float(value) for value in record["scale"]],
            })
        result[category] = records
    return result


def _obb_axes(plot):
    yaw = math.radians(float(plot["yaw_degrees"]))
    return ((math.cos(yaw), math.sin(yaw)), (-math.sin(yaw), math.cos(yaw)))


def _footprints_overlap(first, second) -> bool:
    centers = ((float(first["location_cm"][0]), float(first["location_cm"][1])), (float(second["location_cm"][0]), float(second["location_cm"][1])))
    axes_a, axes_b = _obb_axes(first), _obb_axes(second)
    extents_a = (float(first["size_cm"][0]) / 2.0, float(first["size_cm"][1]) / 2.0)
    extents_b = (float(second["size_cm"][0]) / 2.0, float(second["size_cm"][1]) / 2.0)
    delta = (centers[1][0] - centers[0][0], centers[1][1] - centers[0][1])
    for axis in (*axes_a, *axes_b):
        distance = abs(delta[0] * axis[0] + delta[1] * axis[1])
        radius_a = sum(extents_a[i] * abs(axes_a[i][0] * axis[0] + axes_a[i][1] * axis[1]) for i in range(2))
        radius_b = sum(extents_b[i] * abs(axes_b[i][0] * axis[0] + axes_b[i][1] * axis[1]) for i in range(2))
        if distance >= radius_a + radius_b:
            return False
    return True


def _validate_geometry(result, config) -> None:
    plots = config["building_plots"]
    for index, first in enumerate(plots):
        for second in plots[index + 1:]:
            if _footprints_overlap(first, second):
                _error(result, "building_footprint_overlap", "building footprints overlap", ids=[first["id"], second["id"]])
        radius = math.hypot(float(first["size_cm"][0]), float(first["size_cm"][1])) / 2.0
        for exclusion in config["exclusion_zones"]:
            if exclusion["kind"] == "anchor_circle":
                anchor = next(item for item in config["fixed_anchors"] if item["id"] == exclusion["source_id"])
                if math.hypot(first["location_cm"][0] - anchor["location_cm"][0], first["location_cm"][1] - anchor["location_cm"][1]) < radius + anchor["protected_radius_cm"] + exclusion["margin_cm"]:
                    _error(result, "protected_anchor_overlap", "building footprint overlaps a protected anchor", building=first["id"], anchor=anchor["id"])
        for field, check in (("road_splines", "road_overlap"), ("river_splines", "river_overlap")):
            for spline in config[field]:
                if _horizontal_distance_to_polyline(first["location_cm"], spline["points_cm"]) < radius + float(spline["width_cm"]) / 2.0:
                    _error(result, check, f"building footprint overlaps {field}", building=first["id"], spline=spline["id"])
    playable = config["playable_bounds_cm"]
    for record in generate_seeded_proxy_transforms(config)["mountains"]:
        x, y = record["location_cm"][:2]
        if playable[0] <= x <= playable[1] and playable[2] <= y <= playable[3]:
            _error(result, "mountain_playable_bounds", "mountain lies inside playable bounds", stable_id=record["stable_id"])


def _spline_points(component) -> list[list[float]]:
    coordinate_space = unreal.SplineCoordinateSpace.WORLD
    return [_vector(component.get_location_at_spline_point(index, coordinate_space)) for index in range(int(component.get_number_of_spline_points()))]


def _validate_splines(result, config, actors, anchor) -> None:
    road_count = river_count = 0
    for field, labels, semantic in (("road_splines", ROAD_LABELS, ROAD_TAGS), ("river_splines", RIVER_LABELS, {})):
        for spec in config[field]:
            matches = _matches(actors, labels[spec["id"]])
            if len(matches) != 1:
                _error(result, "spline_actor", "managed spline label must be unique", label=labels[spec["id"]], count=len(matches))
                continue
            components = matches[0].get_components_by_class(unreal.SplineComponent)
            if len(components) != 1:
                _error(result, "spline_component", "spline actor must own exactly one spline component", label=labels[spec["id"]], count=len(components))
                continue
            component = components[0]
            actual = _spline_points(component)
            expected = [_north_local_location(anchor, point) for point in spec["points_cm"]]
            required_tags = {MANAGED_TAG, "PrototypeOnly", f"SemanticWidthCM_{spec['width_cm']}"}
            required_tags |= ({semantic[spec["id"]], "Quick_Road_LayoutInput"} if field == "road_splines" else {"TownPCG_River"})
            if bool(component.is_closed_loop()) or len(actual) != len(expected) or any(math.dist(a, b) > 1.0 for a, b in zip(actual, expected)) or not required_tags <= _tags(component):
                _error(result, "spline_contract", "spline point data/tags/closed state differs from config", label=labels[spec["id"]])
            if field == "road_splines": road_count += 1
            else: river_count += 1
    result["road_spline_count"], result["river_spline_count"] = road_count, river_count


def _runtime_disabled(component) -> bool:
    values = []
    for field in ("generate_on_load", "generate_on_drop", "regenerate_in_editor"):
        try:
            values.append(bool(component.get_editor_property(field)))
        except Exception:
            return False
    return not any(values)


def _validate_pcg(result, config, actors, current_level, anchor) -> None:
    expected = _expected_proxy_transforms(config, anchor)
    all_ok = True
    for category, label in PCG_LABELS.items():
        matches = _matches(actors, label)
        if len(matches) != 1:
            _error(result, "pcg_actor", "PCG label must be unique", label=label, count=len(matches)); all_ok = False; continue
        actor = matches[0]
        if _package_path(actor.get_level()) != current_level or MANAGED_TAG not in _tags(actor):
            _error(result, "pcg_ownership", "PCG actor must be current-level and tag-owned", label=label); all_ok = False
        components = actor.get_components_by_class(unreal.PCGComponent)
        if len(components) != 1:
            _error(result, "pcg_component_ownership", "PCG actor must own one PCG component", label=label, count=len(components)); all_ok = False; continue
        component = components[0]
        graph_path = _package_path(component.get_graph())
        if graph_path != GRAPH_PATHS[category]:
            _error(result, "pcg_graph", "PCG component graph path differs", label=label, actual=graph_path)
        disabled = _runtime_disabled(component)
        all_ok = all_ok and disabled
        try:
            status = json.loads(unreal.GameXXKTownPCGAutomationLibrary.get_town_pcg_status(label))
            if status.get("success") is not True or status.get("generated") is not True:
                _error(result, "pcg_status", "PCG read-only status is not generated", label=label, status=status)
        except Exception as error:
            _error(result, "pcg_status", f"could not read PCG status: {error}", label=label)
        actual = []
        for ism in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            if PCG_GENERATED_COMPONENT_TAG in _tags(ism):
                actual.extend(_instance_transform(ism.get_instance_transform(index, world_space=True)) for index in range(int(ism.get_instance_count())))
        result[f"{category[:-1] if category.endswith('s') else category}_instance_count"] = len(actual)
        wanted = expected[category]
        if len(actual) != len(wanted) or any(not _transform_matches(item, wanted[index]) for index, item in enumerate(actual)):
            _error(result, "pcg_instance_transforms", "generated instance transforms differ from deterministic output", category=category, actual_count=len(actual), expected_count=len(wanted))
    result["runtime_generation_disabled"] = all_ok


def _validate_fixed_transforms(result, config, actors, anchor) -> None:
    anchor_by_id = {item["id"]: item for item in config["fixed_anchors"]}
    for label, anchor_id, size in (
        ("QS_B0R_Bridge_Main", "MainBridgeAnchor", (900.0, 2400.0, 120.0)),
        ("QS_B0R_Dock_South", "SouthDockAnchor", (1800.0, 1200.0, 100.0)),
    ):
        spec = anchor_by_id[anchor_id]
        local = list(spec["location_cm"])
        local[2] += size[2] / 2.0
        expected = {
            "location_cm": _north_local_location(anchor, local),
            "rotation_degrees": [0.0, 0.0, _north_yaw(anchor, spec["yaw_degrees"])],
            "scale": [value / 100.0 for value in size],
        }
        matches = _matches(actors, label)
        if len(matches) != 1 or not _transform_matches(_actor_transform(matches[0]), expected):
            _error(result, "anchor_transform", "managed fixed-anchor proxy differs from config", label=label, anchor_id=anchor_id)
    for zone in config["terrain_zones"]:
        matches = _matches(actors, TERRAIN_LABELS[zone["id"]])
        expected_center = list(zone["center_cm"]); expected_center[2] -= zone["size_cm"][2] / 2.0
        expected = {"location_cm": _north_local_location(anchor, expected_center), "rotation_degrees": [0.0, 0.0, _north_yaw(anchor, zone["yaw_degrees"])], "scale": [value / 100.0 for value in zone["size_cm"]]}
        if len(matches) != 1 or not _transform_matches(_actor_transform(matches[0]), expected):
            _error(result, "terrain_transform", "terrain transform differs from config", id=zone["id"])
    camera_ids = []
    for camera_id in CAMERA_IDS:
        matches = _matches(actors, camera_id)
        if len(matches) != 1:
            _error(result, "camera", "camera label must be unique", camera_id=camera_id); continue
        actor = matches[0]; spec = config["cameras"][camera_id]
        location = _north_local_location(anchor, spec["location_cm"]); target = _north_local_location(anchor, spec["target_cm"])
        expected = {"location_cm": location, "rotation_degrees": _look_at_rotation(location, target), "scale": [1.0, 1.0, 1.0]}
        fov = float(actor.get_camera_component().get_editor_property("field_of_view"))
        if not _transform_matches(_actor_transform(actor), expected, 1.0, 0.1, 0.001) or abs(fov - spec["fov_degrees"]) > 0.01:
            _error(result, "camera_transform", "saved camera transform/FOV differs from config", camera_id=camera_id)
        camera_ids.append(camera_id)
    result["camera_ids"], result["camera_count"] = camera_ids, len(camera_ids)
    result["terrain_zone_count"] = sum(len(_matches(actors, label)) == 1 for label in TERRAIN_LABELS.values())


def validate_whitebox() -> dict[str, Any]:
    result = _new_result()
    try:
        config = load_config()
        _ensure_whitebox_read_only()
        result["current_map"] = _current_map()
        result["source_map_clean"] = SOURCE_MAP not in _dirty_maps()
        actors = _actors(); current_level = result["current_map"]
        counts = {label: len(_matches(actors, label)) for label in REQUIRED_LABELS}
        result["managed_label_uniqueness"] = counts
        for label, count in counts.items():
            if count != 1:
                _error(result, "managed_label_uniqueness", "managed label must occur exactly once", label=label, count=count)
        for actor in actors:
            tags = _tags(actor)
            if _label(actor) in REQUIRED_LABELS and MANAGED_TAG not in tags:
                _error(result, "managed_ownership", "required managed object must be tag-owned", label=_label(actor))
            if MANAGED_TAG in tags and _package_path(actor.get_level()) != current_level:
                _error(result, "managed_ownership", "managed object must be in the current level", label=_label(actor))
        gate_label = config["coordinate_reference"]["actor_label"]
        gate_matches = _matches(actors, gate_label)
        player_matches = _matches(actors, "PlayerStart_QingshanInn")
        if len(gate_matches) != 1 or len(player_matches) != 1:
            _error(result, "protected_actor_snapshot", "gate and duplicated PlayerStart must each be unique")
            return result
        gate, player = gate_matches[0], player_matches[0]
        result["protected_actor_snapshot"] = {gate_label: _actor_transform(gate), "PlayerStart_QingshanInn": _actor_transform(player)}
        facing = _angle_delta_degrees(player.get_actor_rotation().yaw, gate.get_actor_rotation().yaw)
        result["player_gate_facing_angle_degrees"] = facing
        if facing < 35.0:
            _error(result, "player_gate_facing", "duplicated PlayerStart facing must differ from gate by at least 35 degrees", angle=facing)
        _validate_splines(result, config, actors, gate)
        bridge = next(item for item in config["fixed_anchors"] if item["id"] == "MainBridgeAnchor")["location_cm"]
        road = next(item for item in config["road_splines"] if item["id"] == "Road_Main")["points_cm"]
        river = config["river_splines"][0]["points_cm"]
        result["bridge_road_horizontal_distance_cm"] = _horizontal_distance_to_polyline(bridge, road)
        result["bridge_river_horizontal_distance_cm"] = _horizontal_distance_to_polyline(bridge, river)
        if result["bridge_road_horizontal_distance_cm"] > 100 or result["bridge_river_horizontal_distance_cm"] > 100:
            _error(result, "bridge_alignment", "bridge must be within 100cm horizontally of road and river")
        _validate_geometry(result, config)
        _validate_pcg(result, config, actors, current_level, gate)
        _validate_fixed_transforms(result, config, actors, gate)
        result["crossing_count"] = sum(MANAGED_TAG in _tags(actor) and "QingshanB0RCrossing" in _tags(actor) for actor in actors)
        if not 16 <= result["building_instance_count"] <= 19: _error(result, "building_count", "building instances must be 16..19")
        if result["foliage_instance_count"] > 100: _error(result, "foliage_count", "foliage instances exceed 100")
        if not 24 <= result["mountain_instance_count"] <= 30: _error(result, "mountain_count", "mountain instances must be 24..30")
        if result["crossing_count"] > 2: _error(result, "crossing_count", "crossings exceed 2")
        if result["road_spline_count"] != 3 or result["river_spline_count"] != 1: _error(result, "spline_counts", "requires exactly 3 roads and 1 river")
        if result["camera_count"] != 4 or set(result["camera_ids"]) != set(CAMERA_IDS): _error(result, "camera_count", "requires exact four camera IDs")
        if result["terrain_zone_count"] != 4: _error(result, "terrain_count", "requires exactly four terrain zones")
        result["layout_sha256"] = canonical_layout_hash(generate_seeded_proxy_transforms(config))["sha256"]
        if SOURCE_MAP in _dirty_maps(): _error(result, "source_map_clean", "source map is dirty")
    except Exception as error:
        _error(result, "exception", str(error), exception_type=type(error).__name__)
    result["success"] = not result["errors"]
    return result


def main() -> None:
    try:
        result = validate_whitebox()
        output = json.dumps(result, sort_keys=True, separators=(",", ":"), allow_nan=False)
    except Exception as error:
        fallback = {"marker": "GAMEXXK_QINGSHAN_B0R_VALIDATION", "success": False, "errors": [{"check": "serialization", "message": str(error)}]}
        output = json.dumps(fallback, sort_keys=True, separators=(",", ":"), allow_nan=False)
    print(output)


if __name__ == "__main__":
    main()
