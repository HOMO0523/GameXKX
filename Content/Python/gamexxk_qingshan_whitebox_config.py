"""Host-safe loader and validator for the Qingshan B0R whitebox contract."""

from __future__ import annotations

import json
import math
from pathlib import Path
import re


__all__ = ("CONFIG_PATH", "load_config", "validate_config")

PROJECT_ROOT = Path(__file__).resolve().parents[2]
CONFIG_PATH = PROJECT_ROOT / "Config" / "GameXXK" / "TownPCG" / "QingshanWhiteboxB0R.json"

_PACKAGE_PATH = re.compile(r"^/Game(?:/[A-Za-z0-9_]+)+$")
_STABLE_ID = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")


def _require_id(value, field):
    if not isinstance(value, str) or value != value.strip() or not _STABLE_ID.fullmatch(value):
        raise ValueError(f"{field} must be a nonblank trimmed stable id")
    return value


def _require_number(value, field):
    if isinstance(value, bool):
        raise ValueError(f"{field} must be a number, not bool")
    if not isinstance(value, (int, float)):
        raise ValueError(f"{field} must be a number")
    if not math.isfinite(value):
        raise ValueError(f"{field} must be finite")
    return value


def _require_vector(value, field, length=3):
    if not isinstance(value, list) or len(value) != length:
        raise ValueError(f"{field} must be a {length}-number vector")
    for component in value:
        if isinstance(component, bool):
            raise ValueError(f"{field} must contain numbers, not bool")
        if not isinstance(component, (int, float)):
            raise ValueError(f"{field} must be a {length}-number vector")
        if not math.isfinite(component):
            raise ValueError(f"{field} must contain only finite numbers")
    return value


def _require_bounds(value, field):
    bounds = _require_vector(value, field, length=4)
    if bounds[0] >= bounds[1] or bounds[2] >= bounds[3]:
        raise ValueError(f"{field} must be [min_x, max_x, min_y, max_y]")
    return bounds


def _require_package_path(value, field):
    if not isinstance(value, str) or value != value.strip() or not _PACKAGE_PATH.fullmatch(value):
        raise ValueError(f"{field} must be a valid Unreal package path")
    return value


def _require_integer(value, field, minimum=None):
    if isinstance(value, bool):
        raise ValueError(f"{field} must be an integer, not bool")
    if not isinstance(value, int):
        raise ValueError(f"{field} must be an integer")
    if minimum is not None and value < minimum:
        raise ValueError(f"{field} must be at least {minimum}")
    return value


def _require_object(value, field):
    if not isinstance(value, dict):
        raise ValueError(f"{field} must be an object")
    return value


def _require_list(value, field):
    if not isinstance(value, list):
        raise ValueError(f"{field} must be a list")
    return value


def _inside_world(vector, bounds):
    return bounds[0] <= vector[0] <= bounds[1] and bounds[2] <= vector[1] <= bounds[3]


def _require_inside_world(vector, bounds, field):
    if not _inside_world(vector, bounds):
        raise ValueError(f"{field} is outside world_bounds_cm")


def _horizontal_distance(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def validate_config(data):
    data = _require_object(data, "config")
    if _require_integer(data.get("schema_version"), "schema_version") != 1:
        raise ValueError("schema_version must be 1")
    if data.get("revision_id") != "B0R":
        raise ValueError("revision_id must be B0R")
    _require_integer(data.get("seed"), "seed", minimum=0)
    for field in ("source_map", "whitebox_map", "asset_root"):
        _require_package_path(data.get(field), field)

    coordinate_reference = _require_object(data.get("coordinate_reference"), "coordinate_reference")
    _require_id(coordinate_reference.get("anchor_id"), "coordinate_reference.anchor_id")
    actor_label = coordinate_reference.get("actor_label")
    if not isinstance(actor_label, str) or actor_label != actor_label.strip() or not actor_label:
        raise ValueError("coordinate_reference.actor_label must be a nonblank trimmed string")

    playable_bounds = _require_bounds(data.get("playable_bounds_cm"), "playable_bounds_cm")
    world_bounds = _require_bounds(data.get("world_bounds_cm"), "world_bounds_cm")
    if not (world_bounds[0] <= playable_bounds[0] < playable_bounds[1] <= world_bounds[1]
            and world_bounds[2] <= playable_bounds[2] < playable_bounds[3] <= world_bounds[3]):
        raise ValueError("playable_bounds_cm must be inside world_bounds_cm")
    if data.get("runtime_generation") is not False:
        raise ValueError("runtime_generation must be false")

    seen_ids = set()

    def register(stable_id, field):
        stable_id = _require_id(stable_id, field)
        if stable_id in seen_ids:
            raise ValueError(f"duplicate stable id: {stable_id}")
        seen_ids.add(stable_id)
        return stable_id

    anchors = _require_list(data.get("fixed_anchors"), "fixed_anchors")
    anchor_by_id = {}
    for index, raw in enumerate(anchors):
        item = _require_object(raw, f"fixed_anchors[{index}]")
        stable_id = register(item.get("id"), f"fixed_anchors[{index}].id")
        location = _require_vector(item.get("location_cm"), f"fixed_anchors[{index}].location_cm")
        _require_inside_world(location, world_bounds, f"fixed_anchors[{index}].location_cm")
        _require_number(item.get("yaw_degrees"), f"fixed_anchors[{index}].yaw_degrees")
        radius = _require_number(
            item.get("protected_radius_cm"),
            f"fixed_anchors[{index}].protected_radius_cm",
        )
        if radius <= 0:
            raise ValueError(f"fixed_anchors[{index}].protected_radius_cm must be greater than 0")
        anchor_by_id[stable_id] = item
    if "NorthGateFAnchor" not in anchor_by_id:
        raise ValueError("fixed_anchors must contain NorthGateFAnchor")
    if coordinate_reference["anchor_id"] != "NorthGateFAnchor":
        raise ValueError("coordinate_reference.anchor_id must be NorthGateFAnchor")

    def validate_splines(field):
        splines = _require_list(data.get(field), field)
        for index, raw in enumerate(splines):
            item = _require_object(raw, f"{field}[{index}]")
            register(item.get("id"), f"{field}[{index}].id")
            width = _require_number(item.get("width_cm"), f"{field}[{index}].width_cm")
            if width <= 0:
                raise ValueError(f"{field}[{index}].width_cm must be greater than 0")
            points = _require_list(item.get("points_cm"), f"{field}[{index}].points_cm")
            if len(points) < 2:
                raise ValueError(f"{field}[{index}].points_cm must contain at least two points")
            for point_index, point in enumerate(points):
                vector = _require_vector(point, f"{field}[{index}].points_cm[{point_index}]")
                _require_inside_world(vector, world_bounds, f"{field}[{index}].points_cm[{point_index}]")
        return splines

    roads = validate_splines("road_splines")
    rivers = validate_splines("river_splines")

    terrain_zones = _require_list(data.get("terrain_zones"), "terrain_zones")
    for index, raw in enumerate(terrain_zones):
        item = _require_object(raw, f"terrain_zones[{index}]")
        register(item.get("id"), f"terrain_zones[{index}].id")
        _require_vector(item.get("center_cm"), f"terrain_zones[{index}].center_cm")
        size = _require_vector(item.get("size_cm"), f"terrain_zones[{index}].size_cm")
        if any(component <= 0 for component in size):
            raise ValueError(f"terrain_zones[{index}].size_cm must be positive")
        _require_number(item.get("yaw_degrees"), f"terrain_zones[{index}].yaw_degrees")

    plots = _require_list(data.get("building_plots"), "building_plots")
    if not 13 <= len(plots) <= 19:
        raise ValueError("building_plots count must be between 13 and 19")
    for index, raw in enumerate(plots):
        item = _require_object(raw, f"building_plots[{index}]")
        register(item.get("id"), f"building_plots[{index}].id")
        if item.get("size_class") not in ("large", "medium", "small"):
            raise ValueError(f"building_plots[{index}].size_class is invalid")
        location = _require_vector(item.get("location_cm"), f"building_plots[{index}].location_cm")
        _require_inside_world(location, world_bounds, f"building_plots[{index}].location_cm")
        _require_number(item.get("yaw_degrees"), f"building_plots[{index}].yaw_degrees")
        size = _require_vector(item.get("size_cm"), f"building_plots[{index}].size_cm")
        if any(component <= 0 for component in size):
            raise ValueError(f"building_plots[{index}].size_cm must be positive")
        if item.get("entrance_axis") != "+Y":
            raise ValueError(f"building_plots[{index}].entrance_axis must be +Y")
        _require_id(item.get("cluster_id"), f"building_plots[{index}].cluster_id")

    spawn_caps = _require_object(data.get("spawn_caps"), "spawn_caps")
    for field in ("buildings", "foliage", "mountains", "crossings"):
        _require_integer(spawn_caps.get(field), f"spawn_caps.{field}", minimum=0)

    proxy = _require_object(data.get("proxy_generation"), "proxy_generation")
    foliage = _require_object(proxy.get("foliage"), "proxy_generation.foliage")
    mountains = _require_object(proxy.get("mountains"), "proxy_generation.mountains")
    _require_integer(foliage.get("count"), "proxy_generation.foliage.count", minimum=0)
    _require_vector(foliage.get("scale_range"), "proxy_generation.foliage.scale_range", length=2)
    _require_number(foliage.get("exclusion_margin_cm"), "proxy_generation.foliage.exclusion_margin_cm")
    _require_integer(mountains.get("count"), "proxy_generation.mountains.count", minimum=0)
    for field in ("scale_xy_range", "scale_z_range", "perimeter_band_cm"):
        _require_vector(mountains.get(field), f"proxy_generation.mountains.{field}", length=2)

    exclusions = _require_list(data.get("exclusion_zones"), "exclusion_zones")
    for index, raw in enumerate(exclusions):
        item = _require_object(raw, f"exclusion_zones[{index}]")
        register(item.get("id"), f"exclusion_zones[{index}].id")
        if item.get("kind") not in ("anchor_circle", "spline_corridor", "building_footprint"):
            raise ValueError(f"exclusion_zones[{index}].kind is invalid")
        _require_id(item.get("source_id"), f"exclusion_zones[{index}].source_id")
        margin = _require_number(item.get("margin_cm"), f"exclusion_zones[{index}].margin_cm")
        if margin < 0:
            raise ValueError(f"exclusion_zones[{index}].margin_cm must be at least 0")

    cameras = _require_object(data.get("cameras"), "cameras")
    for stable_id, raw in cameras.items():
        register(stable_id, f"cameras.{stable_id}")
        item = _require_object(raw, f"cameras.{stable_id}")
        for field in ("location_cm", "target_cm"):
            vector = _require_vector(item.get(field), f"cameras.{stable_id}.{field}")
            _require_inside_world(vector, world_bounds, f"cameras.{stable_id}.{field}")
        fov = _require_number(item.get("fov_degrees"), f"cameras.{stable_id}.fov_degrees")
        if not 35 <= fov <= 65:
            raise ValueError(f"cameras.{stable_id}.fov_degrees must be between 35 and 65")

    bridge = anchor_by_id.get("MainBridgeAnchor")
    if bridge is None:
        raise ValueError("fixed_anchors must contain MainBridgeAnchor")
    bridge_location = bridge["location_cm"]
    main_road = next((road for road in roads if road["id"] == "Road_Main"), None)
    if main_road is None:
        raise ValueError("road_splines must contain Road_Main")
    road_distance = min(_horizontal_distance(bridge_location, point) for point in main_road["points_cm"])
    river_distance = min(
        _horizontal_distance(bridge_location, point)
        for river in rivers
        for point in river["points_cm"]
    )
    if road_distance > 100 or river_distance > 100:
        raise ValueError("MainBridgeAnchor must be within 100cm of main-road and river control points")

    return data


def _reject_json_numeric_constant(constant):
    raise ValueError(f"non-standard JSON numeric constant {constant} is not allowed")


def load_config(path=CONFIG_PATH):
    with Path(path).open("r", encoding="utf-8") as stream:
        data = json.load(stream, parse_constant=_reject_json_numeric_constant)
    return validate_config(data)
