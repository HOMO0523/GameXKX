"""Host-safe loader and validator for the Qingshan B1 dress overlay."""

from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
import math
from pathlib import Path
import re


__all__ = (
    "CONFIG_PATH",
    "PROJECT_ROOT",
    "contract_sha256",
    "load_config",
    "load_overlay",
    "merge_config",
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
_ROOF_PALETTES = {"orange", "teal", "indigo", "ochre"}
_PROP_TYPES = {
    "sign", "lantern", "banner", "fence", "crate",
    "stall", "well", "cart", "rock", "dock_post",
}
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
    "sign": True,
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
    assignments = _require_list(overlay.get("base_plot_assignments"), "base_plot_assignments")
    additions = _require_list(
        overlay.get("additional_building_plots"),
        "additional_building_plots",
    )
    base_plots = _require_list(base_config.get("building_plots"), "base_config.building_plots")
    base_ids = {plot["id"] for plot in base_plots}
    assignment_ids = [item.get("id") for item in assignments if isinstance(item, dict)]
    if len(assignment_ids) != len(set(assignment_ids)) or set(assignment_ids) != base_ids:
        raise ValueError("base plot assignment IDs must exactly match B0R building plot IDs")
    assignment_by_id = {item["id"]: item for item in assignments}

    additional_ids = [item.get("id") for item in additions if isinstance(item, dict)]
    if len(additional_ids) != len(set(additional_ids)) or set(additional_ids) != _ADDITIONAL_PLOT_IDS:
        raise ValueError("additional building plot IDs must match the approved B1 IDs")
    if base_ids.intersection(additional_ids):
        raise ValueError("additional building plot IDs must not collide with B0R IDs")

    building_materials = _require_object(overlay.get("building_materials"), "building_materials")
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
        addition = _require_object(addition, "additional_building_plots item")
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
    for network in merged.get("quickroad", {}).get("networks", []):
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
        _require_package_path(path, f"building_materials.shared_slot_materials.{slot}")
    roof_materials = _require_object(
        materials.get("roof_palette_materials"),
        "building_materials.roof_palette_materials",
    )
    if set(roof_materials) != _ROOF_PALETTES:
        raise ValueError("roof_palette_materials must define the four approved palettes")
    for palette, path in roof_materials.items():
        _require_package_path(path, f"roof_palette_materials.{palette}")
    return materials


def _validate_additional_plot_clearance(plots, base_config):
    river = base_config["river_splines"][0]
    for plot in (item for item in plots if item["id"] in _ADDITIONAL_PLOT_IDS):
        radius = math.hypot(plot["size_cm"][0] / 2.0, plot["size_cm"][1] / 2.0)
        for other in plots:
            if other["id"] == plot["id"]:
                continue
            other_radius = math.hypot(other["size_cm"][0] / 2.0, other["size_cm"][1] / 2.0)
            if math.dist(plot["location_cm"][:2], other["location_cm"][:2]) <= radius + other_radius + 100.0:
                raise ValueError(f"additional building plot {plot['id']} overlaps {other['id']}")
        for road in base_config["road_splines"]:
            clearance = radius + road["width_cm"] / 2.0 + 250.0
            if _polyline_distance(plot["location_cm"], road["points_cm"]) <= clearance:
                raise ValueError(f"additional building plot {plot['id']} overlaps road {road['id']}")
        if _polyline_distance(plot["location_cm"], river["points_cm"]) <= radius + river["width_cm"] / 2.0 + 400.0:
            raise ValueError(f"additional building plot {plot['id']} overlaps river")
        for anchor in base_config["fixed_anchors"]:
            if math.dist(plot["location_cm"][:2], anchor["location_cm"][:2]) <= radius + anchor["protected_radius_cm"]:
                raise ValueError(f"additional building plot {plot['id']} overlaps anchor {anchor['id']}")


def validate_config(data, *, base_config=None):
    """Validate a fully merged B1 configuration without reading UE assets."""

    data = _require_object(data, "config")
    base_config = copy.deepcopy(base_config) if base_config is not None else _load_base_config()
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
    if landscape.get("height_encoding") != {
        "value_type": "uint16",
        "formula": "uint16=clamp(round(32768+elevation_cm*128/scale_z),0,65535)",
    }:
        raise ValueError("landscape.height_encoding is invalid")

    quickroad = _require_object(data.get("quickroad"), "quickroad")
    if _require_package_path(quickroad.get("road_material"), "quickroad.road_material") != \
            "/Game/GameXXK/Environment/TownPCG/B1/Materials/MI_QS_B1_Road_Earth":
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
        _require_vector(plot.get("location_cm"), f"building_plots[{index}].location_cm")
        _require_vector(plot.get("size_cm"), f"building_plots[{index}].size_cm")
        _require_number(plot.get("yaw_degrees"), f"building_plots[{index}].yaw_degrees")
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
        if plot["id"] in base_by_id:
            if {field: plot.get(field) for field in _GEOMETRY_FIELDS} != {
                    field: base_by_id[plot["id"]].get(field) for field in _GEOMETRY_FIELDS}:
                raise ValueError(f"B0R geometry drift for {plot['id']}")
    if size_counts != {"large": 1, "medium": 7, "small": 18}:
        raise ValueError("building size-class counts must be 1/7/18")
    if observed_archetypes != _ARCHETYPES or observed_palettes != _ROOF_PALETTES:
        raise ValueError("building styles must cover six archetypes and four roof palettes")
    if {plot["id"] for plot in plots if plot["id"] not in base_by_id} != _ADDITIONAL_PLOT_IDS:
        raise ValueError("additional building IDs must match the approved B1 set")
    _validate_additional_plot_clearance(plots, base_config)

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
        _require_vector(item.get("location_cm"), f"prop_records[{index}].location_cm")
        _require_number(item.get("yaw_degrees"), f"prop_records[{index}].yaw_degrees")
        scale_value = _require_number(item.get("scale"), f"prop_records[{index}].scale")
        if scale_value <= 0:
            raise ValueError(f"prop_records[{index}].scale must be positive")
        if item.get("collision_enabled") is not collision_policy[prop_type]:
            raise ValueError(f"prop_records[{index}].collision_enabled violates collision_policy")
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
        _require_vector(item.get("location_cm"), f"vegetation_records[{index}].location_cm")
        _require_number(item.get("yaw_degrees"), f"vegetation_records[{index}].yaw_degrees")
        if _require_number(item.get("scale"), f"vegetation_records[{index}].scale") <= 0:
            raise ValueError(f"vegetation_records[{index}].scale must be positive")
        if item.get("collision_enabled") is not False:
            raise ValueError(f"vegetation_records[{index}].collision_enabled must be false")
        (animated if mode == "animated_flipbook" else static).append(item)
    if len(animated) != 30 or len(static) != 70:
        raise ValueError("vegetation records must split into 30 animated and 70 static")
    for index, item in enumerate(animated):
        if "frame_variant" in item:
            raise ValueError("animated vegetation must not contain frame_variant")
        if item.get("start_frame") != index % 4:
            raise ValueError("animated vegetation start_frame assignment is not deterministic")
    for index, item in enumerate(static):
        if item.get("frame_variant") != index % 4:
            raise ValueError("static vegetation frame_variant assignment is not deterministic")
        if "start_frame" in item:
            raise ValueError("static vegetation must not contain start_frame")

    mountains = _require_list(data.get("mountain_records"), "mountain_records")
    if len(mountains) != 24:
        raise ValueError("mountain_records must contain exactly 24 records")
    for index, item in enumerate(mountains):
        item = _require_object(item, f"mountain_records[{index}]")
        register(item.get("id"), f"mountain_records[{index}].id")
        if item.get("asset_type") != "mountain":
            raise ValueError(f"mountain_records[{index}].asset_type is invalid")
        _require_vector(item.get("location_cm"), f"mountain_records[{index}].location_cm")
        _require_number(item.get("yaw_degrees"), f"mountain_records[{index}].yaw_degrees")
        _require_vector(item.get("scale"), f"mountain_records[{index}].scale")
        if item.get("collision_enabled") is not False:
            raise ValueError(f"mountain_records[{index}].collision_enabled must be false")

    cameras = _require_list(data.get("cameras"), "cameras")
    if len(cameras) != 4 or {item.get("id") for item in cameras if isinstance(item, dict)} != _CAMERA_IDS:
        raise ValueError("cameras must contain the four approved B1 review cameras")
    for index, item in enumerate(cameras):
        item = _require_object(item, f"cameras[{index}]")
        register(item.get("id"), f"cameras[{index}].id")
        _require_vector(item.get("location_cm"), f"cameras[{index}].location_cm")
        _require_vector(item.get("target_cm"), f"cameras[{index}].target_cm")
        fov = _require_number(item.get("fov_degrees"), f"cameras[{index}].fov_degrees")
        if not 35 <= fov <= 65:
            raise ValueError(f"cameras[{index}].fov_degrees must be between 35 and 65")

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
        "seed": config["seed"],
        "coordinate_space": config["coordinate_space"],
        "landscape": config["landscape"],
        "quickroad": config["quickroad"],
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
