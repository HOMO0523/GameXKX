"""Host-safe loader and validator for the Qingshan B1 dress overlay."""

from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
import math
from pathlib import Path, PurePosixPath
import re


__all__ = (
    "CONFIG_PATH",
    "PROJECT_ROOT",
    "contract_sha256",
    "load_config",
    "load_overlay",
    "merge_config",
    "oriented_plot_distance",
    "validate_config",
    "validate_protected_file_hashes",
)

PROJECT_ROOT = Path(__file__).resolve().parents[2]
CONFIG_PATH = PROJECT_ROOT / "Config" / "GameXXK" / "TownPCG" / "QingshanDressB1.json"
B0R_CONFIG_PATH = PROJECT_ROOT / "Config" / "GameXXK" / "TownPCG" / "QingshanWhiteboxB0R.json"
B0R_MODULE_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_qingshan_whitebox_config.py"

_PACKAGE_PATH = re.compile(r"^/Game(?:/[A-Za-z0-9_]+)+$")
_STABLE_ID = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")
_SHA256 = re.compile(r"^[0-9a-f]{64}$")

_ARCHETYPES = {
    "gable_shop", "tall_house", "wide_house",
    "courtyard_wing", "bridge_house", "dock_shed",
}
_ASSET_ROOT = "/Game/GameXXK/Environment/TownPCG/B1"
_ROOF_PALETTES = {"orange", "teal", "indigo", "ochre"}
_PROP_TYPES = {
    "sign", "lantern", "banner", "fence", "crate",
    "stall", "well", "cart", "rock", "dock_post",
}
_ATTACHMENT_PROP_TYPES = {"sign", "lantern", "banner"}
_PLOT_CLUSTERS = {"approach", "core", "bridge", "south"}
_ADDITIONAL_PLOT_IDS = {
    "BLD_S_11", "BLD_S_12", "BLD_M_06", "BLD_S_13", "BLD_S_14",
    "BLD_S_15", "BLD_M_07", "BLD_S_16", "BLD_S_17", "BLD_S_18",
}
_CAMERA_IDS = {
    "CAM_QS_B1_GATE_ARRIVAL", "CAM_QS_B1_TOWN_CORE",
    "CAM_QS_B1_MAIN_BRIDGE", "CAM_QS_B1_SOUTH_DOCK",
}
_EXPECTED_CAPS = {
    "buildings": 26,
    "props": 72,
    "vegetation": 100,
    "animated_vegetation": 30,
    "mountains": 24,
    "crossings": 2,
}
_EXPECTED_PROTECTED_FILES = {
    "Content/GameXXK/Maps/L_QingshanInn.umap":
        "a3639b38623d00e8ad3e5a610a3e1695a47b38c1d1e6fedb8115e1e9fdf5c8a8",
    "Content/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R.umap":
        "74292340df0cea97d99e905dd193a921038326bfec2f3ce034a5e9f70bd3f107",
    "Config/GameXXK/TownPCG/QingshanWhiteboxB0R.json":
        "3f231876d0083bffe28ee555b60af1a20b0966edbde9dc4bb6f2647e920eadb1",
}
_EXPECTED_COLLISION_POLICY = {
    "sign": False,
    "lantern": False,
    "banner": False,
    "fence": True,
    "crate": True,
    "stall": True,
    "well": True,
    "cart": True,
    "rock": True,
    "dock_post": True,
    "plant_card": False,
    "mountain": False,
}
_EXPECTED_ASSET_CATALOG = {
    "building_meshes": {
        "gable_shop": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_GableShop",
        "tall_house": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_TallHouse",
        "wide_house": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_WideHouse",
        "courtyard_wing": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_CourtyardWing",
        "bridge_house": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_BridgeHouse",
        "dock_shed": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_DockShed",
    },
    "prop_meshes": {
        "sign": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_Sign",
        "lantern": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_Lantern",
        "banner": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_Banner",
        "fence": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_Fence",
        "crate": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_Crate",
        "stall": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_Stall",
        "well": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_Well",
        "cart": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_Cart",
        "rock": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_Rock",
        "dock_post": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_DockPost",
    },
    "plant_card_mesh": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_PlantCard",
    "plant_flipbook": f"{_ASSET_ROOT}/Paper2D/FB_QS_B1_Plant_Sway",
    "mountain_mesh": f"{_ASSET_ROOT}/Meshes/SM_QS_B1_Mountain",
}
_GEOMETRY_FIELDS = (
    "id", "size_class", "location_cm", "yaw_degrees",
    "size_cm", "entrance_axis", "cluster_id",
)


def _load_b0r_module():
    spec = importlib.util.spec_from_file_location(
        "_gamexxk_qingshan_whitebox_config_for_b1",
        B0R_MODULE_PATH,
    )
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to import B0R loader from {B0R_MODULE_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


_B0R_MODULE = _load_b0r_module()


def _reject_json_numeric_constant(constant):
    raise ValueError(f"non-standard JSON numeric constant {constant} is not allowed")


def _require_object(value, field):
    if not isinstance(value, dict):
        raise ValueError(f"{field} must be an object")
    return value


def _require_list(value, field):
    if not isinstance(value, list):
        raise ValueError(f"{field} must be a list")
    return value


def _require_id(value, field):
    if not isinstance(value, str) or value != value.strip() or not _STABLE_ID.fullmatch(value):
        raise ValueError(f"{field} must be a nonblank trimmed stable id")
    return value


def _require_package_path(value, field):
    if not isinstance(value, str) or value != value.strip() or not _PACKAGE_PATH.fullmatch(value):
        raise ValueError(f"{field} must be a valid Unreal package path")
    return value


def _is_finite_number(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False
    try:
        return math.isfinite(value)
    except (OverflowError, TypeError, ValueError):
        return False


def _require_number(value, field):
    if isinstance(value, bool):
        raise ValueError(f"{field} must be a number, not bool")
    if not isinstance(value, (int, float)):
        raise ValueError(f"{field} must be a number")
    if not _is_finite_number(value):
        raise ValueError(f"{field} must be finite")
    return value


def _require_integer(value, field, minimum=None):
    if isinstance(value, bool):
        raise ValueError(f"{field} must be an integer, not bool")
    if not isinstance(value, int):
        raise ValueError(f"{field} must be an integer")
    if minimum is not None and value < minimum:
        raise ValueError(f"{field} must be at least {minimum}")
    return value


def _require_vector(value, field, length=3):
    if not isinstance(value, list) or len(value) != length:
        raise ValueError(f"{field} must be a {length}-number vector")
    for component in value:
        if isinstance(component, bool):
            raise ValueError(f"{field} must contain numbers, not bool")
        if not isinstance(component, (int, float)):
            raise ValueError(f"{field} must be a {length}-number vector")
        if not _is_finite_number(component):
            raise ValueError(f"{field} must contain only finite numbers")
    return value


def _require_sha256(value, field):
    if not isinstance(value, str) or not _SHA256.fullmatch(value):
        raise ValueError(f"{field} must be a lowercase SHA-256")
    return value


def _point_segment_distance(point, start, end):
    px, py = point[:2]
    ax, ay = start[:2]
    bx, by = end[:2]
    dx, dy = bx - ax, by - ay
    length_squared = dx * dx + dy * dy
    if length_squared == 0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length_squared))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def _polyline_distance(point, points):
    return min(
        _point_segment_distance(point, start, end)
        for start, end in zip(points, points[1:])
    )


def _cross_2d(origin, first, second):
    return (
        (first[0] - origin[0]) * (second[1] - origin[1])
        - (first[1] - origin[1]) * (second[0] - origin[0])
    )


def _point_on_segment(point, start, end, epsilon=1e-7):
    if abs(_cross_2d(start, end, point)) > epsilon:
        return False
    return (
        min(start[0], end[0]) - epsilon <= point[0] <= max(start[0], end[0]) + epsilon
        and min(start[1], end[1]) - epsilon <= point[1] <= max(start[1], end[1]) + epsilon
    )


def _segments_intersect(first_start, first_end, second_start, second_end):
    cross_1 = _cross_2d(first_start, first_end, second_start)
    cross_2 = _cross_2d(first_start, first_end, second_end)
    cross_3 = _cross_2d(second_start, second_end, first_start)
    cross_4 = _cross_2d(second_start, second_end, first_end)
    epsilon = 1e-7
    if ((cross_1 > epsilon and cross_2 < -epsilon) or (cross_1 < -epsilon and cross_2 > epsilon)) \
            and ((cross_3 > epsilon and cross_4 < -epsilon) or (cross_3 < -epsilon and cross_4 > epsilon)):
        return True
    return (
        (abs(cross_1) <= epsilon and _point_on_segment(second_start, first_start, first_end))
        or (abs(cross_2) <= epsilon and _point_on_segment(second_end, first_start, first_end))
        or (abs(cross_3) <= epsilon and _point_on_segment(first_start, second_start, second_end))
        or (abs(cross_4) <= epsilon and _point_on_segment(first_end, second_start, second_end))
    )


def _segment_distance(first_start, first_end, second_start, second_end):
    if _segments_intersect(first_start, first_end, second_start, second_end):
        return 0.0
    return min(
        _point_segment_distance(first_start, second_start, second_end),
        _point_segment_distance(first_end, second_start, second_end),
        _point_segment_distance(second_start, first_start, first_end),
        _point_segment_distance(second_end, first_start, first_end),
    )


def _point_to_plot_local(point, plot):
    delta_x = point[0] - plot["location_cm"][0]
    delta_y = point[1] - plot["location_cm"][1]
    radians = math.radians(plot["yaw_degrees"])
    cosine = math.cos(radians)
    sine = math.sin(radians)
    return (
        delta_x * cosine + delta_y * sine,
        -delta_x * sine + delta_y * cosine,
    )


def _plot_obb_corners(plot):
    center_x, center_y = plot["location_cm"][:2]
    half_x = plot["size_cm"][0] / 2.0
    half_y = plot["size_cm"][1] / 2.0
    radians = math.radians(plot["yaw_degrees"])
    cosine = math.cos(radians)
    sine = math.sin(radians)
    corners = []
    for local_x, local_y in (
            (-half_x, -half_y), (half_x, -half_y),
            (half_x, half_y), (-half_x, half_y)):
        corners.append((
            center_x + local_x * cosine - local_y * sine,
            center_y + local_x * sine + local_y * cosine,
        ))
    return corners


def _obb_edges(plot):
    corners = _plot_obb_corners(plot)
    return tuple(zip(corners, corners[1:] + corners[:1]))


def _point_in_plot_obb(point, plot, epsilon=1e-7):
    local_x, local_y = _point_to_plot_local(point, plot)
    return (
        abs(local_x) <= plot["size_cm"][0] / 2.0 + epsilon
        and abs(local_y) <= plot["size_cm"][1] / 2.0 + epsilon
    )


def _point_to_plot_distance(point, plot):
    local_x, local_y = _point_to_plot_local(point, plot)
    delta_x = max(abs(local_x) - plot["size_cm"][0] / 2.0, 0.0)
    delta_y = max(abs(local_y) - plot["size_cm"][1] / 2.0, 0.0)
    return math.hypot(delta_x, delta_y)


def oriented_plot_distance(first, second):
    """Return exact 2D distance between two rotated building footprints."""

    first_corners = _plot_obb_corners(first)
    second_corners = _plot_obb_corners(second)
    if any(_point_in_plot_obb(point, second) for point in first_corners) \
            or any(_point_in_plot_obb(point, first) for point in second_corners):
        return 0.0
    return min(
        _segment_distance(first_start, first_end, second_start, second_end)
        for first_start, first_end in _obb_edges(first)
        for second_start, second_end in _obb_edges(second)
    )


def _polyline_to_plot_distance(points, plot):
    edges = _obb_edges(plot)
    distance = float("inf")
    for start, end in zip(points, points[1:]):
        if _point_in_plot_obb(start, plot) or _point_in_plot_obb(end, plot):
            return 0.0
        for edge_start, edge_end in edges:
            distance = min(distance, _segment_distance(start, end, edge_start, edge_end))
            if distance == 0.0:
                return 0.0
    return distance


def _point_in_attachment_band(point, plot, band_cm=150.0):
    local_x, local_y = _point_to_plot_local(point, plot)
    absolute_x = abs(local_x)
    absolute_y = abs(local_y)
    half_x = plot["size_cm"][0] / 2.0
    half_y = plot["size_cm"][1] / 2.0
    if absolute_x > half_x + band_cm or absolute_y > half_y + band_cm:
        return False
    if absolute_x <= half_x and absolute_y <= half_y:
        edge_distance = min(half_x - absolute_x, half_y - absolute_y)
    else:
        edge_distance = math.hypot(
            max(absolute_x - half_x, 0.0),
            max(absolute_y - half_y, 0.0),
        )
    return edge_distance <= band_cm


def _landscape_bounds(landscape):
    origin = landscape["origin_local_cm"]
    resolution = landscape["resolution"]
    scale = landscape["scale_cm"]
    return [
        origin[0], origin[0] + (resolution[0] - 1) * scale[0],
        origin[1], origin[1] + (resolution[1] - 1) * scale[1],
    ]


def _point_inside_bounds(point, bounds):
    return bounds[0] <= point[0] <= bounds[1] and bounds[2] <= point[1] <= bounds[3]


def _require_point_in_spatial_bounds(point, world_bounds, landscape_bounds, field):
    if not _point_inside_bounds(point, world_bounds) or not _point_inside_bounds(point, landscape_bounds):
        raise ValueError(f"{field} is outside world/landscape bounds")


def _stable_frame_variant(seed, stable_id):
    digest = hashlib.sha256(f"{seed}:{stable_id}".encode("utf-8")).hexdigest()
    return int(digest[:8], 16) % 4


def _load_base_config(project_root=PROJECT_ROOT):
    path = Path(project_root) / "Config" / "GameXXK" / "TownPCG" / "QingshanWhiteboxB0R.json"
    return _B0R_MODULE.load_config(path)


def load_overlay(path=CONFIG_PATH):
    with Path(path).open("r", encoding="utf-8") as stream:
        data = json.load(stream, parse_constant=_reject_json_numeric_constant)
    return _require_object(data, "overlay")


def _style_plot(plot, archetype_id, roof_palette, building_materials):
    if archetype_id not in _ARCHETYPES:
        raise ValueError("building archetype_id is invalid")
    if roof_palette not in _ROOF_PALETTES:
        raise ValueError("building roof_palette is invalid")
    styled = copy.deepcopy(plot)
    styled["archetype_id"] = archetype_id
    styled["roof_palette"] = roof_palette
    styled["pcg_group_key"] = f"{archetype_id}__{roof_palette}"
    styled["material_overrides"] = {
        "Roof": building_materials["roof_palette_materials"][roof_palette],
        "WindowPaper": building_materials["shared_slot_materials"]["WindowPaper"],
    }
    return styled


def merge_config(overlay, base_config):
    """Merge B1 styles and additions onto a validated B0R geometry snapshot."""

    overlay = copy.deepcopy(_require_object(overlay, "overlay"))
    base_config = copy.deepcopy(_require_object(base_config, "base_config"))
    try:
        base_config = _B0R_MODULE.validate_config(base_config)
    except (KeyError, TypeError, ValueError) as error:
        raise ValueError(f"base_config is invalid: {error}") from error
    building_materials = _validate_material_contract(overlay)
    assignments = _require_list(overlay.get("base_plot_assignments"), "base_plot_assignments")
    additions = _require_list(
        overlay.get("additional_building_plots"),
        "additional_building_plots",
    )
    assignments = [
        _require_object(item, f"base_plot_assignments[{index}]")
        for index, item in enumerate(assignments)
    ]
    additions = [
        _require_object(item, f"additional_building_plots[{index}]")
        for index, item in enumerate(additions)
    ]
    base_plots = _require_list(base_config.get("building_plots"), "base_config.building_plots")
    base_ids = {plot["id"] for plot in base_plots}
    assignment_ids = [item.get("id") for item in assignments]
    if len(assignment_ids) != len(set(assignment_ids)) or set(assignment_ids) != base_ids:
        raise ValueError("base plot assignment IDs must exactly match B0R building plot IDs")
    assignment_by_id = {item["id"]: item for item in assignments}

    additional_ids = [item.get("id") for item in additions]
    if len(additional_ids) != len(set(additional_ids)) or set(additional_ids) != _ADDITIONAL_PLOT_IDS:
        raise ValueError("additional building plot IDs must match the approved B1 IDs")
    if base_ids.intersection(additional_ids):
        raise ValueError("additional building plot IDs must not collide with B0R IDs")

    plots = []
    for source in base_plots:
        assignment = _require_object(
            assignment_by_id[source["id"]],
            f"base_plot_assignments.{source['id']}",
        )
        if set(assignment) != {"id", "archetype_id", "roof_palette"}:
            raise ValueError("base plot assignments may contain only id, archetype_id, and roof_palette")
        plots.append(_style_plot(
            source,
            assignment.get("archetype_id"),
            assignment.get("roof_palette"),
            building_materials,
        ))
    for addition in additions:
        geometry = {field: copy.deepcopy(addition.get(field)) for field in _GEOMETRY_FIELDS}
        plots.append(_style_plot(
            geometry,
            addition.get("archetype_id"),
            addition.get("roof_palette"),
            building_materials,
        ))

    merged = overlay
    merged.pop("base_plot_assignments", None)
    merged.pop("additional_building_plots", None)
    merged["base_building_count"] = len(base_plots)
    merged["additional_building_count"] = len(additions)
    merged["building_plots"] = plots
    for field in (
        "fixed_anchors", "road_splines", "river_splines", "terrain_zones",
        "playable_bounds_cm", "world_bounds_cm",
    ):
        merged[field] = copy.deepcopy(base_config[field])

    road_by_id = {road["id"]: road for road in base_config["road_splines"]}
    quickroad = _require_object(merged.get("quickroad"), "quickroad")
    networks = _require_list(quickroad.get("networks"), "quickroad.networks")
    for index, raw_network in enumerate(networks):
        network = _require_object(raw_network, f"quickroad.networks[{index}]")
        source_id = network.get("source_spline_id")
        if source_id not in road_by_id:
            raise ValueError(f"unknown QuickRoad source_spline_id: {source_id}")
        network["points_cm"] = copy.deepcopy(road_by_id[source_id]["points_cm"])

    return validate_config(merged, base_config=base_config)


def _validate_material_contract(data):
    materials = _require_object(data.get("building_materials"), "building_materials")
    shared = _require_object(materials.get("shared_slot_materials"), "building_materials.shared_slot_materials")
    if set(shared) != {"Wall", "Timber", "WindowPaper"}:
        raise ValueError("building_materials.shared_slot_materials has invalid slots")
    for slot, path in shared.items():
        _require_asset_under_root(path, f"building_materials.shared_slot_materials.{slot}")
    roof_materials = _require_object(
        materials.get("roof_palette_materials"),
        "building_materials.roof_palette_materials",
    )
    if set(roof_materials) != _ROOF_PALETTES:
        raise ValueError("roof_palette_materials must define the four approved palettes")
    for palette, path in roof_materials.items():
        _require_asset_under_root(path, f"roof_palette_materials.{palette}")
    return materials


def _require_asset_under_root(value, field):
    path = _require_package_path(value, field)
    if not path.startswith(f"{_ASSET_ROOT}/"):
        raise ValueError(f"{field} must be inside asset_root {_ASSET_ROOT}")
    return path


def _validate_asset_catalog(data):
    catalog = _require_object(data.get("asset_catalog"), "asset_catalog")
    if set(catalog) != {
            "building_meshes", "prop_meshes", "plant_card_mesh",
            "plant_flipbook", "mountain_mesh"}:
        raise ValueError("asset_catalog has invalid keys")
    building_meshes = _require_object(
        catalog.get("building_meshes"),
        "asset_catalog.building_meshes",
    )
    prop_meshes = _require_object(catalog.get("prop_meshes"), "asset_catalog.prop_meshes")
    if set(building_meshes) != _ARCHETYPES:
        raise ValueError("asset_catalog.building_meshes must define six archetypes")
    if set(prop_meshes) != _PROP_TYPES:
        raise ValueError("asset_catalog.prop_meshes must define ten prop types")
    for category, values in (
            ("building_meshes", building_meshes), ("prop_meshes", prop_meshes)):
        for asset_id, path in values.items():
            _require_asset_under_root(path, f"asset_catalog.{category}.{asset_id}")
    for field in ("plant_card_mesh", "plant_flipbook", "mountain_mesh"):
        _require_asset_under_root(catalog.get(field), f"asset_catalog.{field}")
    if catalog != _EXPECTED_ASSET_CATALOG:
        raise ValueError("asset_catalog must equal the approved B1 asset catalog")
    return catalog


def _validate_heightmap_source(value):
    if not isinstance(value, str) or not value or "\\" in value:
        raise ValueError("landscape.heightmap_source must be a project-relative POSIX path")
    path = PurePosixPath(value)
    if path.is_absolute() or ".." in path.parts or path.as_posix() != value:
        raise ValueError("landscape.heightmap_source must be a normalized project-relative POSIX path")
    approved_parent = PurePosixPath("Content/ArtSource/Qingshan/B1")
    if path.parent != approved_parent or path.suffix.lower() != ".png":
        raise ValueError("landscape.heightmap_source must stay inside Content/ArtSource/Qingshan/B1")
    if value != "Content/ArtSource/Qingshan/B1/H_QS_B1_Terrain_505.png":
        raise ValueError("landscape.heightmap_source must be the approved 505 heightmap")
    return value


def _validate_building_spatial_contract(plots, base_config, world_bounds, landscape_bounds):
    river = base_config["river_splines"][0]
    bridge = next(anchor for anchor in base_config["fixed_anchors"] if anchor["id"] == "MainBridgeAnchor")
    for plot in plots:
        for corner_index, corner in enumerate(_plot_obb_corners(plot)):
            _require_point_in_spatial_bounds(
                corner,
                world_bounds,
                landscape_bounds,
                f"building_plots.{plot['id']}.obb_corners[{corner_index}]",
            )
        if plot["id"] not in _ADDITIONAL_PLOT_IDS:
            continue
        if _point_to_plot_distance(bridge["location_cm"], plot) <= bridge["protected_radius_cm"]:
            raise ValueError(f"building {plot['id']} overlaps bridge anchor")
        for road in base_config["road_splines"]:
            clearance = road["width_cm"] / 2.0 + 250.0
            if _polyline_to_plot_distance(road["points_cm"], plot) <= clearance:
                raise ValueError(f"building {plot['id']} overlaps road corridor {road['id']}")
        if _polyline_to_plot_distance(river["points_cm"], plot) <= river["width_cm"] / 2.0 + 400.0:
            raise ValueError(f"building {plot['id']} overlaps river corridor {river['id']}")
    for index, first in enumerate(plots):
        for second in plots[index + 1:]:
            if oriented_plot_distance(first, second) <= 100.0:
                raise ValueError(f"building OBB conflict: {first['id']} and {second['id']}")


def _validate_ground_point_clearance(
        point,
        field,
        base_config,
        plots,
        *,
        building_margin_cm,
        allow_river_overlap=False,
        attachment_target_id=None):
    _validate_fixed_anchor_clearance(
        point,
        field,
        base_config,
        building_margin_cm,
    )
    for road in base_config["road_splines"]:
        if _polyline_distance(point, road["points_cm"]) <= road["width_cm"] / 2.0 + 250.0:
            raise ValueError(f"{field} overlaps road corridor {road['id']}")
    river = base_config["river_splines"][0]
    if not allow_river_overlap \
            and _polyline_distance(point, river["points_cm"]) <= river["width_cm"] / 2.0 + 400.0:
        raise ValueError(f"{field} overlaps river corridor {river['id']}")
    for plot in plots:
        if plot["id"] == attachment_target_id:
            continue
        if _point_to_plot_distance(point, plot) <= building_margin_cm:
            raise ValueError(f"{field} overlaps building OBB {plot['id']}")


def _validate_fixed_anchor_clearance(point, field, base_config, category_clearance_cm):
    for anchor in base_config["fixed_anchors"]:
        minimum_distance = anchor["protected_radius_cm"] + category_clearance_cm
        if math.dist(point[:2], anchor["location_cm"][:2]) <= minimum_distance:
            bridge_suffix = " (bridge anchor)" if anchor["id"] == "MainBridgeAnchor" else ""
            raise ValueError(
                f"{field} overlaps fixed anchor {anchor['id']}{bridge_suffix}"
            )


def validate_config(data, *, base_config=None):
    """Validate a fully merged B1 configuration without reading UE assets."""

    data = _require_object(data, "config")
    base_config = copy.deepcopy(base_config) if base_config is not None else _load_base_config()
    try:
        base_config = _B0R_MODULE.validate_config(base_config)
    except (KeyError, TypeError, ValueError) as error:
        raise ValueError(f"base_config is invalid: {error}") from error
    if _require_integer(data.get("schema_version"), "schema_version") != 1:
        raise ValueError("schema_version must be 1")
    if data.get("revision_id") != "B1":
        raise ValueError("revision_id must be B1")
    if _require_integer(data.get("seed"), "seed", minimum=0) != 20260711:
        raise ValueError("seed must be 20260711")
    for field in ("source_map", "dress_map", "asset_root"):
        _require_package_path(data.get(field), field)
    if data["source_map"] != "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R":
        raise ValueError("source_map must be the approved B0R map")
    if data["dress_map"] != "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1":
        raise ValueError("dress_map must be the isolated B1 map")
    if data["asset_root"] != _ASSET_ROOT:
        raise ValueError(f"asset_root must be {_ASSET_ROOT}")
    _validate_asset_catalog(data)
    if data.get("runtime_generation") is not False:
        raise ValueError("runtime_generation must be false")
    if data.get("coordinate_space") != "anchor_local":
        raise ValueError("coordinate_space must be anchor_local")
    if data.get("coordinate_reference_actor") != "QingshanInn_TownExit":
        raise ValueError("coordinate_reference_actor must be QingshanInn_TownExit")
    if _require_sha256(
        data.get("expected_base_live_layout_sha256"),
        "expected_base_live_layout_sha256",
    ) != "15d1b585739a2a180a6022974ec0c365144cbc29511c2426fc0058f1fd43dfbd":
        raise ValueError("expected_base_live_layout_sha256 must match the approved live B0R hash")
    if data.get("protected_files") != _EXPECTED_PROTECTED_FILES:
        raise ValueError("protected_files must equal the approved raw SHA-256 contract")

    landscape = _require_object(data.get("landscape"), "landscape")
    if landscape.get("label") != "QS_B1_Landscape":
        raise ValueError("landscape.label must be QS_B1_Landscape")
    resolution = landscape.get("resolution")
    if resolution != [505, 505]:
        raise ValueError("landscape.resolution must be [505, 505]")
    for axis, value in enumerate(resolution):
        _require_integer(value, f"landscape.resolution[{axis}]", minimum=2)
    if landscape.get("quads_per_section") != 63:
        raise ValueError("landscape.quads_per_section must be 63")
    if landscape.get("sections_per_component") != 2:
        raise ValueError("landscape.sections_per_component must be 2")
    if landscape.get("component_count") != [4, 4]:
        raise ValueError("landscape.component_count must be [4, 4]")
    scale = _require_vector(landscape.get("scale_cm"), "landscape.scale_cm")
    if any(component <= 0 for component in scale):
        raise ValueError("landscape.scale_cm must be strictly positive")
    center = _require_vector(landscape.get("center_local_cm"), "landscape.center_local_cm")
    origin = _require_vector(landscape.get("origin_local_cm"), "landscape.origin_local_cm")
    expected_origin = [
        center[axis] - ((resolution[axis] - 1) * scale[axis] / 2.0)
        for axis in range(2)
    ] + [center[2]]
    if origin != expected_origin:
        raise ValueError("landscape.origin_local_cm does not match the center/resolution/scale contract")
    if landscape.get("edit_layer") != "QR_B1_Roads":
        raise ValueError("landscape.edit_layer must be QR_B1_Roads")
    _validate_heightmap_source(landscape.get("heightmap_source"))
    if _require_asset_under_root(
            landscape.get("ground_material"),
            "landscape.ground_material") != f"{_ASSET_ROOT}/Materials/MI_QS_B1_Ground":
        raise ValueError("landscape.ground_material must use the exact B1 ground material")
    if landscape.get("height_encoding") != {
        "value_type": "uint16",
        "formula": "uint16=clamp(round(32768+elevation_cm*128/scale_z),0,65535)",
    }:
        raise ValueError("landscape.height_encoding is invalid")
    world_bounds = base_config["world_bounds_cm"]
    landscape_xy_bounds = _landscape_bounds(landscape)

    quickroad = _require_object(data.get("quickroad"), "quickroad")
    if _require_asset_under_root(quickroad.get("road_material"), "quickroad.road_material") != \
            f"{_ASSET_ROOT}/Materials/MI_QS_B1_Road_Earth":
        raise ValueError("quickroad.road_material must use the B1 earth road material")
    networks = _require_list(quickroad.get("networks"), "quickroad.networks")
    expected_networks = (
        ("QS_B1_Main", "Road_Main", 800),
        ("QS_B1_CoreNorth", "Road_Core_North", 450),
        ("QS_B1_CoreSouth", "Road_Core_South", 400),
    )
    if len(networks) != 3:
        raise ValueError("quickroad.networks must contain exactly three width groups")
    road_by_id = {road["id"]: road for road in base_config["road_splines"]}
    for index, (network, expected) in enumerate(zip(networks, expected_networks)):
        network = _require_object(network, f"quickroad.networks[{index}]")
        width = _require_number(network.get("width_cm"), f"quickroad.networks[{index}].width_cm")
        observed = (network.get("tag"), network.get("source_spline_id"), width)
        if observed != expected:
            raise ValueError(f"quickroad.networks[{index}] does not match its approved width group")
        if network.get("points_cm") != road_by_id[expected[1]]["points_cm"]:
            raise ValueError(f"quickroad.networks[{index}].points_cm must copy B0R geometry")
    if quickroad.get("combined_tag_expression") != "QS_B1_Main|QS_B1_CoreNorth|QS_B1_CoreSouth":
        raise ValueError("quickroad.combined_tag_expression is invalid")
    exact_quickroad_sections = {
        "ribbon": {
            "mesh_sample_distance_cm": 300,
            "width_subdivisions": 3,
            "curvature_threshold_degrees": 8,
            "max_curvature_subdivisions": 2,
        },
        "landscape_influence": {
            "falloff_cm": 250,
            "blend": 0.9,
            "vertical_offset_cm": -5,
            "smooth_iterations": 4,
            "smooth_strength": 0.6,
        },
        "intersection": {
            "sample_distance_cm": 500,
            "border_offset_cm": 100,
            "corner_radius_cm": 50,
            "branch_width_scale": 1.2,
        },
        "bake": {"split_length_cm": 5000},
    }
    for section, expected in exact_quickroad_sections.items():
        observed = _require_object(quickroad.get(section), f"quickroad.{section}")
        for field, value in observed.items():
            _require_number(value, f"quickroad.{section}.{field}")
        if observed != expected:
            raise ValueError(f"quickroad.{section} must match the approved contract")

    materials = _validate_material_contract(data)
    plots = _require_list(data.get("building_plots"), "building_plots")
    if len(plots) != 26 or data.get("base_building_count") != 16 or data.get("additional_building_count") != 10:
        raise ValueError("building counts must be 16 base + 10 additional = 26")
    seen_ids = set()

    def register(stable_id, field):
        stable_id = _require_id(stable_id, field)
        if stable_id in seen_ids:
            raise ValueError(f"duplicate stable id: {stable_id}")
        seen_ids.add(stable_id)

    base_by_id = {plot["id"]: plot for plot in base_config["building_plots"]}
    size_counts = {"large": 0, "medium": 0, "small": 0}
    observed_archetypes = set()
    observed_palettes = set()
    for index, plot in enumerate(plots):
        plot = _require_object(plot, f"building_plots[{index}]")
        register(plot.get("id"), f"building_plots[{index}].id")
        size_class = plot.get("size_class")
        if size_class not in size_counts:
            raise ValueError(f"building_plots[{index}].size_class is invalid")
        size_counts[size_class] += 1
        location = _require_vector(plot.get("location_cm"), f"building_plots[{index}].location_cm")
        _require_point_in_spatial_bounds(
            location,
            world_bounds,
            landscape_xy_bounds,
            f"building_plots[{index}].location_cm",
        )
        size = _require_vector(plot.get("size_cm"), f"building_plots[{index}].size_cm")
        if any(component <= 0 for component in size):
            raise ValueError(f"building_plots[{index}].size_cm must be strictly positive")
        _require_number(plot.get("yaw_degrees"), f"building_plots[{index}].yaw_degrees")
        if plot.get("entrance_axis") != "+Y":
            raise ValueError(f"building_plots[{index}].entrance_axis must be +Y")
        if plot.get("cluster_id") not in _PLOT_CLUSTERS:
            raise ValueError(f"building_plots[{index}].cluster_id is invalid")
        archetype = plot.get("archetype_id")
        palette = plot.get("roof_palette")
        if archetype not in _ARCHETYPES:
            raise ValueError(f"building_plots[{index}].archetype_id is invalid")
        if palette not in _ROOF_PALETTES:
            raise ValueError(f"building_plots[{index}].roof_palette is invalid")
        observed_archetypes.add(archetype)
        observed_palettes.add(palette)
        if plot.get("pcg_group_key") != f"{archetype}__{palette}":
            raise ValueError(f"building_plots[{index}].pcg_group_key is invalid")
        if plot.get("material_overrides") != {
            "Roof": materials["roof_palette_materials"][palette],
            "WindowPaper": materials["shared_slot_materials"]["WindowPaper"],
        }:
            raise ValueError(f"building_plots[{index}].material_overrides is invalid")
    if size_counts != {"large": 1, "medium": 7, "small": 18}:
        raise ValueError("building size-class counts must be 1/7/18")
    if observed_archetypes != _ARCHETYPES or observed_palettes != _ROOF_PALETTES:
        raise ValueError("building styles must cover six archetypes and four roof palettes")
    if {plot["id"] for plot in plots if plot["id"] not in base_by_id} != _ADDITIONAL_PLOT_IDS:
        raise ValueError("additional building IDs must match the approved B1 set")
    _validate_building_spatial_contract(plots, base_config, world_bounds, landscape_xy_bounds)
    for plot in plots:
        if plot["id"] in base_by_id and {
                field: plot.get(field) for field in _GEOMETRY_FIELDS} != {
                field: base_by_id[plot["id"]].get(field) for field in _GEOMETRY_FIELDS}:
            raise ValueError(f"B0R geometry drift for {plot['id']}")
    plot_by_id = {plot["id"]: plot for plot in plots}

    if data.get("caps") != _EXPECTED_CAPS:
        raise ValueError("caps must equal the approved conservative B1 caps")
    collision_policy = _require_object(data.get("collision_policy"), "collision_policy")
    if collision_policy != _EXPECTED_COLLISION_POLICY:
        raise ValueError("collision_policy must match the approved asset policy")

    props = _require_list(data.get("prop_records"), "prop_records")
    if len(props) != 72:
        raise ValueError("prop_records must contain exactly 72 records")
    observed_prop_types = set()
    for index, item in enumerate(props):
        item = _require_object(item, f"prop_records[{index}]")
        register(item.get("id"), f"prop_records[{index}].id")
        prop_type = item.get("prop_type")
        if prop_type not in _PROP_TYPES:
            raise ValueError(f"prop_records[{index}].prop_type is invalid")
        observed_prop_types.add(prop_type)
        location = _require_vector(item.get("location_cm"), f"prop_records[{index}].location_cm")
        _require_point_in_spatial_bounds(
            location,
            world_bounds,
            landscape_xy_bounds,
            f"prop_records[{index}].location_cm",
        )
        _require_number(item.get("yaw_degrees"), f"prop_records[{index}].yaw_degrees")
        scale_value = _require_number(item.get("scale"), f"prop_records[{index}].scale")
        if scale_value <= 0:
            raise ValueError(f"prop_records[{index}].scale must be positive")
        if item.get("collision_enabled") is not collision_policy[prop_type]:
            raise ValueError(f"prop_records[{index}].collision_enabled violates collision_policy")
        _validate_fixed_anchor_clearance(
            location,
            f"prop_records[{index}]",
            base_config,
            0.0,
        )
        attachment_target_id = item.get("attachment_target_id")
        if prop_type in _ATTACHMENT_PROP_TYPES:
            attachment_target_id = _require_id(
                attachment_target_id,
                f"prop_records[{index}].attachment_target_id",
            )
            if attachment_target_id not in plot_by_id:
                raise ValueError(f"prop_records[{index}].attachment_target_id must resolve to a building")
            if not _point_in_attachment_band(location, plot_by_id[attachment_target_id]):
                raise ValueError(f"prop_records[{index}] must stay in its target building attachment band")
        elif "attachment_target_id" in item:
            raise ValueError("only sign/lantern/banner props may define attachment_target_id")
        allow_river_overlap = item.get("allow_river_overlap", False)
        if "allow_river_overlap" in item and (
                allow_river_overlap is not True or prop_type != "dock_post"):
            raise ValueError("only dock_post may set allow_river_overlap=true")
        _validate_ground_point_clearance(
            location,
            f"prop_records[{index}]",
            base_config,
            plots,
            building_margin_cm=0.0,
            allow_river_overlap=allow_river_overlap,
            attachment_target_id=attachment_target_id if prop_type in _ATTACHMENT_PROP_TYPES else None,
        )
    if observed_prop_types != _PROP_TYPES:
        raise ValueError("prop_records must cover every approved prop type")

    vegetation = _require_list(data.get("vegetation_records"), "vegetation_records")
    if len(vegetation) != 100:
        raise ValueError("vegetation_records must contain exactly 100 records")
    animated = []
    static = []
    for index, item in enumerate(vegetation):
        item = _require_object(item, f"vegetation_records[{index}]")
        register(item.get("id"), f"vegetation_records[{index}].id")
        if item.get("plant_type") != "plant_card":
            raise ValueError(f"vegetation_records[{index}].plant_type is invalid")
        mode = item.get("render_mode")
        if mode not in {"animated_flipbook", "static_card"}:
            raise ValueError(f"vegetation_records[{index}].render_mode is invalid")
        location = _require_vector(item.get("location_cm"), f"vegetation_records[{index}].location_cm")
        _require_point_in_spatial_bounds(
            location,
            world_bounds,
            landscape_xy_bounds,
            f"vegetation_records[{index}].location_cm",
        )
        _require_number(item.get("yaw_degrees"), f"vegetation_records[{index}].yaw_degrees")
        if _require_number(item.get("scale"), f"vegetation_records[{index}].scale") <= 0:
            raise ValueError(f"vegetation_records[{index}].scale must be positive")
        if item.get("collision_enabled") is not False:
            raise ValueError(f"vegetation_records[{index}].collision_enabled must be false")
        _validate_ground_point_clearance(
            location,
            f"vegetation_records[{index}]",
            base_config,
            plots,
            building_margin_cm=100.0,
        )
        (animated if mode == "animated_flipbook" else static).append(item)
    if len(animated) != 30 or len(static) != 70:
        raise ValueError("vegetation records must split into 30 animated and 70 static")
    for index, item in enumerate(animated):
        if "frame_variant" in item:
            raise ValueError("animated vegetation must not contain frame_variant")
        if item.get("start_frame") != index % 4:
            raise ValueError("animated vegetation start_frame assignment is not deterministic")
    for item in static:
        if item.get("frame_variant") != _stable_frame_variant(data["seed"], item["id"]):
            raise ValueError("static vegetation frame_variant must use seed+stable-id SHA-256 derivation")
        if "start_frame" in item:
            raise ValueError("static vegetation must not contain start_frame")
    if {item["frame_variant"] for item in static} != {0, 1, 2, 3}:
        raise ValueError("static vegetation must populate all four frame variants")

    mountains = _require_list(data.get("mountain_records"), "mountain_records")
    if len(mountains) != 24:
        raise ValueError("mountain_records must contain exactly 24 records")
    for index, item in enumerate(mountains):
        item = _require_object(item, f"mountain_records[{index}]")
        register(item.get("id"), f"mountain_records[{index}].id")
        if item.get("asset_type") != "mountain":
            raise ValueError(f"mountain_records[{index}].asset_type is invalid")
        location = _require_vector(item.get("location_cm"), f"mountain_records[{index}].location_cm")
        _require_point_in_spatial_bounds(
            location,
            world_bounds,
            landscape_xy_bounds,
            f"mountain_records[{index}].location_cm",
        )
        _require_number(item.get("yaw_degrees"), f"mountain_records[{index}].yaw_degrees")
        mountain_scale = _require_vector(item.get("scale"), f"mountain_records[{index}].scale")
        if any(component <= 0 for component in mountain_scale):
            raise ValueError(f"mountain_records[{index}].scale must be strictly positive")
        if item.get("collision_enabled") is not False:
            raise ValueError(f"mountain_records[{index}].collision_enabled must be false")
        _validate_ground_point_clearance(
            location,
            f"mountain_records[{index}]",
            base_config,
            plots,
            building_margin_cm=200.0,
        )

    cameras = _require_list(data.get("cameras"), "cameras")
    if len(cameras) != 4:
        raise ValueError("cameras must contain the four approved B1 review cameras")
    observed_camera_ids = set()
    for index, item in enumerate(cameras):
        item = _require_object(item, f"cameras[{index}]")
        register(item.get("id"), f"cameras[{index}].id")
        observed_camera_ids.add(item["id"])
        camera_location = _require_vector(item.get("location_cm"), f"cameras[{index}].location_cm")
        camera_target = _require_vector(item.get("target_cm"), f"cameras[{index}].target_cm")
        _require_point_in_spatial_bounds(
            camera_location,
            world_bounds,
            landscape_xy_bounds,
            f"cameras[{index}].location_cm",
        )
        _require_point_in_spatial_bounds(
            camera_target,
            world_bounds,
            landscape_xy_bounds,
            f"cameras[{index}].target_cm",
        )
        fov = _require_number(item.get("fov_degrees"), f"cameras[{index}].fov_degrees")
        if not 35 <= fov <= 65:
            raise ValueError(f"cameras[{index}].fov_degrees must be between 35 and 65")
    if observed_camera_ids != _CAMERA_IDS:
        raise ValueError("cameras must contain the four approved B1 review cameras")

    return data


def validate_protected_file_hashes(protected_files, project_root=PROJECT_ROOT):
    """Validate exact raw bytes for protected project files and return observed hashes."""

    protected_files = _require_object(protected_files, "protected_files")
    root = Path(project_root).resolve()
    observed = {}
    for relative_path, expected_hash in protected_files.items():
        if not isinstance(relative_path, str) or not relative_path or "\\" in relative_path:
            raise ValueError("protected file paths must be nonblank project-relative POSIX paths")
        expected_hash = _require_sha256(expected_hash, f"protected_files.{relative_path}")
        target = (root / Path(relative_path)).resolve()
        try:
            target.relative_to(root)
        except ValueError as error:
            raise ValueError(f"protected file escapes project root: {relative_path}") from error
        if not target.is_file():
            raise ValueError(f"protected file is missing: {relative_path}")
        actual_hash = hashlib.sha256(target.read_bytes()).hexdigest()
        if actual_hash != expected_hash:
            raise ValueError(
                f"raw SHA-256 mismatch for {relative_path}: "
                f"expected {expected_hash}, observed {actual_hash}"
            )
        observed[relative_path] = actual_hash
    return observed


def contract_sha256(config):
    """Hash only the B1 authoring contract, never the anchor-dependent B0R live hash."""

    payload = {
        "schema_version": config["schema_version"],
        "revision_id": config["revision_id"],
        "seed": config["seed"],
        "coordinate_space": config["coordinate_space"],
        "coordinate_reference_actor": config["coordinate_reference_actor"],
        "source_map": config["source_map"],
        "dress_map": config["dress_map"],
        "asset_root": config["asset_root"],
        "runtime_generation": config["runtime_generation"],
        "protected_files": config["protected_files"],
        "landscape": config["landscape"],
        "quickroad": config["quickroad"],
        "asset_catalog": config["asset_catalog"],
        "building_materials": config["building_materials"],
        "collision_policy": config["collision_policy"],
        "building_plots": config["building_plots"],
        "prop_records": config["prop_records"],
        "vegetation_records": config["vegetation_records"],
        "mountain_records": config["mountain_records"],
        "cameras": config["cameras"],
        "caps": config["caps"],
    }
    encoded = json.dumps(
        payload,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def load_config(
        path=CONFIG_PATH,
        *,
        base_config=None,
        project_root=PROJECT_ROOT,
        verify_protected_files=True):
    overlay = load_overlay(path)
    if base_config is None:
        base_config = _load_base_config(project_root)
    merged = merge_config(overlay, base_config)
    if verify_protected_files:
        validate_protected_file_hashes(merged["protected_files"], project_root)
    return merged
