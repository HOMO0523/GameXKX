"""Add a reversible Eastern Village art layer to the existing Qingshan B1 scene."""

from __future__ import annotations

import json
import math
from pathlib import Path

import unreal
import gamexxk_assemble_qingshan_dress_b1 as b1_helpers


B1_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1"
TAG = "QingshanAsianVillageIntegrated"
EVIDENCE = Path(__file__).resolve().parents[2] / "docs/production/evidence/asian-village-integration/b1-integration-report.json"
TEMPLATE_REPORT = Path(__file__).resolve().parents[2] / "docs/production/evidence/asian-village-integration/vendor-demo-assemblies.json"

TEMPLATES = {
    "thatched_shop": {"bounds": (12600, 13400, -2500, -1500, 650, 1800), "anchor": (13030.0, -2000.0, 1000.0), "expected": 35},
    "canopy_shop": {"bounds": (17500, 18700, 150, 1400, 400, 1600), "anchor": (18080.0, 800.0, 1000.0), "expected": 33},
    "low_thatch_house": {"bounds": (11100, 12100, -4700, -3500, 900, 1800), "anchor": (11600.0, -4010.0, 1000.0), "expected": 23},
    "bridge_house": {"bounds": (14600, 16400, 6450, 8300, 350, 1800), "anchor": (15450.0, 7320.0, 1000.0), "expected": 41},
    "tower": {"bounds": (17500, 18600, 4550, 6000, 350, 3000), "anchor": (18000.0, 5240.0, 1000.0), "expected": 38},
}

MESH = {
    "bridge": "/Game/Asian_Village/meshes/building/SM_bridge_01.SM_bridge_01",
    "roof_canopy": "/Game/Asian_Village/meshes/building/SM_roof_canopy_01.SM_roof_canopy_01",
    "roof": "/Game/Asian_Village/meshes/building/SM_roof_03.SM_roof_03",
    "floor": "/Game/Asian_Village/meshes/building/SM_floor_01.SM_floor_01",
    "wall": "/Game/Asian_Village/meshes/building/SM_wall_01.SM_wall_01",
    "wall_window": "/Game/Asian_Village/meshes/building/SM_wall_window_01.SM_wall_window_01",
    "wall_door": "/Game/Asian_Village/meshes/building/SM_wall_door_01.SM_wall_door_01",
    "tent": "/Game/Asian_Village/meshes/props/SM_market_tent_01.SM_market_tent_01",
    "lantern": "/Game/Asian_Village/meshes/props/SM_lantern_01.SM_lantern_01",
    "banner": "/Game/Asian_Village/meshes/props/SM_flag_banner_01.SM_flag_banner_01",
    "table": "/Game/Asian_Village/meshes/props/SM_table_01.SM_table_01",
    "box": "/Game/Asian_Village/meshes/props/SM_bamboo_box_01.SM_bamboo_box_01",
    "tree": "/Game/Asian_Village/meshes/trees/SM_tree_01.SM_tree_01",
    "bamboo": "/Game/Asian_Village/meshes/trees/SM_bamboo_01.SM_bamboo_01",
    "bush": "/Game/Asian_Village/meshes/plants/SM_bush_01.SM_bush_01",
    "cliff": "/Game/Asian_Village/meshes/cliff/SM_cliff_01.SM_cliff_01",
}

# Locations are gate-local, so the layer follows the existing North Gate F anchor.
# They deliberately avoid road corridors and only enrich focal areas already present.
ITEMS = (
    ("Bridge_Main", "bridge", (-15500, -6500, 260), -35, (2.5, 3.5, 1.2), "bridge"),
    ("MarketTent_West", "tent", (-12500, -1700, 380), 10, (2.1, 2.1, 2.1), "market"),
    ("MarketTent_East", "tent", (-14600, -1500, 380), -18, (1.8, 1.8, 1.8), "market"),
    ("MarketTable_West", "table", (-12400, -2300, 330), 15, (1.8, 1.8, 1.8), "market"),
    ("MarketBoxes", "box", (-12750, -2080, 330), 25, (1.6, 1.6, 1.6), "market"),
    ("Lantern_Core_L", "lantern", (-13200, 1250, 980), 25, (1.6, 1.6, 1.6), "market"),
    ("Lantern_Core_R", "lantern", (-14000, 1250, 980), 25, (1.6, 1.6, 1.6), "market"),
    ("Banner_Core", "banner", (-14800, 500, 850), 25, (1.5, 1.5, 1.5), "market"),
    ("Tree_Approach", "tree", (-4500, 4800, 320), -10, (1.25, 1.25, 1.25), "vegetation"),
    ("Bamboo_Approach", "bamboo", (-5600, 4100, 320), 12, (1.35, 1.35, 1.35), "vegetation"),
    ("Tree_BridgeBank", "tree", (-18400, -3500, 220), 0, (1.5, 1.5, 1.5), "vegetation"),
    ("Bamboo_BridgeBank", "bamboo", (-19300, -4700, 200), 22, (1.45, 1.45, 1.45), "vegetation"),
    ("Bush_BridgeBank", "bush", (-18000, -5100, 190), 0, (2.3, 2.3, 2.3), "vegetation"),
    ("Cliff_RiverFrame", "cliff", (-21000, -7000, -80), -20, (2.0, 2.0, 1.6), "terrain"),
)


def _tags(actor) -> set[str]:
    return {str(tag) for tag in actor.get_editor_property("tags")}


def _load_b1() -> None:
    world = unreal.EditorLevelLibrary.get_editor_world()
    current = "" if world is None else str(world.get_outermost().get_path_name()).split(".", 1)[0]
    if current != B1_MAP and not unreal.EditorLoadingAndSavingUtils.load_map(B1_MAP):
        raise RuntimeError(f"could not load {B1_MAP}")
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None or str(world.get_outermost().get_path_name()).split(".", 1)[0] != B1_MAP:
        raise RuntimeError("B1 map did not become the current editor world")


def _gate_transform():
    matches = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if str(actor.get_actor_label()) == "QingshanInn_TownExit"]
    if len(matches) != 1:
        raise RuntimeError(f"expected one North Gate F anchor, found {len(matches)}")
    return matches[0].get_actor_transform(), matches[0].get_actor_transform()


def _world_location(anchor_transform, local):
    return unreal.MathLibrary.transform_location(anchor_transform, unreal.Vector(*map(float, local)))


def _world_yaw(anchor_transform, local_yaw: float) -> float:
    return float(anchor_transform.rotation.rotator().yaw) + float(local_yaw)


def _clear_owned() -> int:
    removed = 0
    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        if TAG in _tags(actor):
            if not unreal.EditorLevelLibrary.destroy_actor(actor):
                raise RuntimeError(f"could not remove owned actor {actor.get_actor_label()}")
            removed += 1
    return removed


def _load_mesh(key: str):
    path = key if key.startswith("/Game/") else MESH[key]
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError(f"missing vendor mesh for {key}: {path}")
    return asset


def _spawn(anchor_transform, record):
    label, key, local, local_yaw, scale, category = record
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor,
        _world_location(anchor_transform, local),
        unreal.Rotator(0.0, _world_yaw(anchor_transform, local_yaw), 0.0),
    )
    if actor is None:
        raise RuntimeError(f"failed to spawn {label}")
    actor.set_actor_label(f"AV_B1_{label}")
    actor.set_actor_scale3d(unreal.Vector(*map(float, scale)))
    component = actor.static_mesh_component
    component.set_static_mesh(_load_mesh(key))
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_cull_distance(45000.0)
    actor.set_editor_property("tags", [unreal.Name(TAG), unreal.Name(f"AV_B1_{category}")])
    component.set_editor_property("component_tags", [unreal.Name(TAG), unreal.Name(f"AV_B1_{category}")])
    return actor.get_actor_label()


def _rotate_offset(origin, yaw_degrees: float, right_cm: float, forward_cm: float, z_cm: float):
    radians = math.radians(float(yaw_degrees))
    cosine, sine = math.cos(radians), math.sin(radians)
    return (
        float(origin[0]) + right_cm * cosine - forward_cm * sine,
        float(origin[1]) + right_cm * sine + forward_cm * cosine,
        float(origin[2]) + z_cm,
    )


def _house_records(prefix, origin, yaw_degrees, width_segments, depth_segments, scale):
    """Assemble a small complete vendor house from coherent 400 cm modules."""
    records = []
    spacing = 400.0 * scale
    x_offsets = [((index - (width_segments - 1) * 0.5) * spacing) for index in range(width_segments)]
    y_offsets = [((index - (depth_segments - 1) * 0.5) * spacing) for index in range(depth_segments)]
    for x in x_offsets:
        for y in y_offsets:
            records.append((f"{prefix}_Floor_{x:.0f}_{y:.0f}", "floor", _rotate_offset(origin, yaw_degrees, x, y, 0.0), yaw_degrees, (scale, scale, scale), "vendor_house"))
            records.append((f"{prefix}_Roof_{x:.0f}_{y:.0f}", "roof", _rotate_offset(origin, yaw_degrees, x, y, 600.0 * scale), yaw_degrees, (scale, scale, scale), "vendor_house"))
    # Door and paper windows form the front face; solid walls wrap the other sides.
    for index, x in enumerate(x_offsets):
        key = "wall_door" if index == width_segments // 2 else "wall_window"
        records.append((f"{prefix}_Front_{index}", key, _rotate_offset(origin, yaw_degrees, x, y_offsets[-1], 0.0), yaw_degrees, (scale, scale, scale), "vendor_house"))
        records.append((f"{prefix}_Back_{index}", "wall", _rotate_offset(origin, yaw_degrees, x, y_offsets[0], 0.0), yaw_degrees + 180.0, (scale, scale, scale), "vendor_house"))
    for index, y in enumerate(y_offsets):
        records.append((f"{prefix}_SideL_{index}", "wall", _rotate_offset(origin, yaw_degrees, x_offsets[0], y, 0.0), yaw_degrees - 90.0, (scale, scale, scale), "vendor_house"))
        records.append((f"{prefix}_SideR_{index}", "wall", _rotate_offset(origin, yaw_degrees, x_offsets[-1], y, 0.0), yaw_degrees + 90.0, (scale, scale, scale), "vendor_house"))
    return records


def _template_entries(template_id: str):
    """Read an exact, complete vendor assembly from the read-only demo report."""
    definition = TEMPLATES.get(template_id)
    if definition is None:
        raise RuntimeError(f"unknown vendor building template: {template_id}")
    if not TEMPLATE_REPORT.exists():
        raise RuntimeError(f"missing read-only vendor assembly report: {TEMPLATE_REPORT}")
    data = json.loads(TEMPLATE_REPORT.read_text(encoding="utf-8"))
    x0, x1, y0, y1, z0, z1 = definition["bounds"]
    entries = [
        entry for entry in data.get("actors", [])
        if x0 <= entry["location_cm"][0] <= x1
        and y0 <= entry["location_cm"][1] <= y1
        and z0 <= entry["location_cm"][2] <= z1
    ]
    if len(entries) != definition["expected"]:
        raise RuntimeError(
            f"vendor template {template_id} drifted: expected {definition['expected']} parts, got {len(entries)}")
    return entries


def _spawn_vendor_template(anchor_transform, prefix, template_id, destination_local, destination_yaw, scale):
    labels = []
    source_anchor = TEMPLATES[template_id]["anchor"]
    entries = _template_entries(template_id)
    # Compute the lowest source geometry conservatively, then translate the full
    # assembly as one rigid unit onto B1's landscape.  Never ground-snap parts
    # independently: that would shear roofs, walls and beams apart.
    source_lowest_offset = math.inf
    for entry in entries:
        mesh_package = str(entry["mesh"])
        mesh_name = mesh_package.rsplit("/", 1)[-1]
        bounds = _load_mesh(f"{mesh_package}.{mesh_name}").get_bounds()
        extent = max(abs(float(bounds.box_extent.x)), abs(float(bounds.box_extent.y)), abs(float(bounds.box_extent.z)))
        mesh_scale = max(abs(float(value)) for value in entry["scale"])
        source_lowest_offset = min(source_lowest_offset, float(entry["location_cm"][2]) - extent * mesh_scale - source_anchor[2])
    if not math.isfinite(source_lowest_offset):
        raise RuntimeError(f"could not compute ground offset for template {template_id}")
    nominal_anchor_world = _world_location(anchor_transform, destination_local)
    ground_world = b1_helpers._ground_snap_world(anchor_transform, destination_local)
    vertical_delta = float(ground_world.z) - (float(nominal_anchor_world.z) + source_lowest_offset * scale) + 12.0
    for index, entry in enumerate(entries):
        source = entry["location_cm"]
        relative = (source[0] - source_anchor[0], source[1] - source_anchor[1], source[2] - source_anchor[2])
        local = _rotate_offset(destination_local, destination_yaw, relative[0] * scale, relative[1] * scale, relative[2] * scale)
        mesh_package = str(entry["mesh"])
        mesh_name = mesh_package.rsplit("/", 1)[-1]
        mesh = _load_mesh(f"{mesh_package}.{mesh_name}")
        source_rotation = entry["rotation"]
        world_location = _world_location(anchor_transform, local)
        world_location = unreal.Vector(float(world_location.x), float(world_location.y), float(world_location.z) + vertical_delta)
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor,
            world_location,
            unreal.Rotator(float(source_rotation[1]), _world_yaw(anchor_transform, float(source_rotation[2]) + destination_yaw), float(source_rotation[0])),
        )
        if actor is None:
            raise RuntimeError(f"failed to spawn template part {prefix}_{index}")
        actor.set_actor_label(f"AV_B1_{prefix}_{index:02d}_{mesh_name}")
        source_scale = entry["scale"]
        actor.set_actor_scale3d(unreal.Vector(float(source_scale[0]) * scale, float(source_scale[1]) * scale, float(source_scale[2]) * scale))
        component = actor.static_mesh_component
        component.set_static_mesh(mesh)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_cull_distance(45000.0)
        actor.set_editor_property("tags", [unreal.Name(TAG), unreal.Name("AV_B1_vendor_house")])
        component.set_editor_property("component_tags", [unreal.Name(TAG), unreal.Name("AV_B1_vendor_house")])
        labels.append(actor.get_actor_label())
    return labels


def _transform_snapshot(actor):
    transform = actor.get_actor_transform()
    rotation = actor.get_actor_rotation()
    return {
        "location": [float(transform.translation.x), float(transform.translation.y), float(transform.translation.z)],
        "rotation": [float(rotation.roll), float(rotation.pitch), float(rotation.yaw)],
        "scale": [float(transform.scale3d.x), float(transform.scale3d.y), float(transform.scale3d.z)],
    }


def main() -> None:
    _load_b1()
    protected_before = {label: next(
        actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if str(actor.get_actor_label()) == label
    ) for label in ("QingshanInn_TownExit", "PlayerStart_QingshanInn")}
    protected_before = {label: _transform_snapshot(actor) for label, actor in protected_before.items()}
    anchor_transform, _unused = _gate_transform()
    removed = _clear_owned()
    # The vendor demo's apparent buildings are interconnected city fragments, not
    # standalone products. Do not slice them into B1: they expose open frames
    # once the original surrounding modules are absent. Complete replacement
    # buildings must arrive as independent Tripo/Blender assets.
    labels = [_spawn(anchor_transform, record) for record in ITEMS]
    protected_after = {label: next(
        actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if str(actor.get_actor_label()) == label
    ) for label in protected_before}
    protected_after = {label: _transform_snapshot(actor) for label, actor in protected_after.items()}
    if protected_before != protected_after:
        raise RuntimeError("protected North Gate F or PlayerStart changed during vendor integration")
    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError("failed to save B1 after Asian Village integration")
    report = {"ok": True, "map": B1_MAP, "removed_prior_owned": removed, "count": len(labels), "labels": labels}
    EVIDENCE.parent.mkdir(parents=True, exist_ok=True)
    EVIDENCE.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("[GAMEXXK] " + json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
