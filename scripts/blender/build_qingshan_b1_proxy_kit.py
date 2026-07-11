"""Build the deterministic 18-mesh Qingshan B1 low-poly proxy kit.

Run with Blender 4.2+::

    blender --background --factory-startup \
      --python scripts/blender/build_qingshan_b1_proxy_kit.py -- \
      --output Saved/Automation/QingshanB1ProxyKit

The module deliberately remains importable in ordinary Python so host tests can
inspect the immutable mesh contract without importing Blender.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
import traceback
from pathlib import Path
from typing import Iterable, Sequence


try:  # Blender-only imports; the dry-run contract is host-safe.
    import bmesh
    import bpy
    from mathutils import Vector
except ModuleNotFoundError:  # pragma: no cover - exercised by host import tests
    bmesh = None
    bpy = None
    Vector = None


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUTPUT = PROJECT_ROOT / "Saved" / "Automation" / "QingshanB1ProxyKit"
SMALL_BEVEL_WIDTH_M = 0.018
IDENTITY_TOLERANCE = 1.0e-7

BUILDING_SLOT_ORDER = ["Wall", "Timber", "WindowPaper", "Roof"]
PROP_SLOT_TABLE = {
    "sign": ["Timber", "Prop"],
    "lantern": ["Timber", "WindowPaper"],
    "banner": ["Timber", "Prop"],
    "fence": ["Timber"],
    "crate": ["Timber"],
    "stall": ["Timber", "Prop"],
    "well": ["Prop", "Timber"],
    "cart": ["Timber"],
    "rock": ["Prop"],
    "dock_post": ["Timber"],
    "plant_card": ["Foliage"],
    "mountain": ["Mountain"],
}
EXPORT_SETTINGS = {
    "axis_forward": "-Y",
    "axis_up": "Z",
    "use_triangles": True,
    "apply_unit_scale": True,
    "apply_scale_options": "FBX_SCALE_UNITS",
    "path_mode": "STRIP",
    "bake_anim": False,
}

BUILDING_SPECS = {
    "gable_shop": {
        "file": "SM_QS_B1_GableShop.fbx",
        "story_count": 1,
        "width_m": 5.8,
        "depth_m": 4.6,
        "wall_height_m": 3.15,
        "roof_height_m": 1.55,
        "roof_sections": 5,
        "asymmetry_ratio": 0.07,
        "window_count": 2,
        "features": ["front_awning", "wide_facade"],
        "triangle_budget": 2500,
    },
    "tall_house": {
        "file": "SM_QS_B1_TallHouse.fbx",
        "story_count": 2,
        "width_m": 4.0,
        "depth_m": 4.2,
        "wall_height_m": 5.2,
        "roof_height_m": 1.45,
        "roof_sections": 3,
        "asymmetry_ratio": 0.08,
        "window_count": 2,
        "features": ["shifted_upper_storey", "tall_narrow"],
        "triangle_budget": 2500,
    },
    "wide_house": {
        "file": "SM_QS_B1_WideHouse.fbx",
        "story_count": 1,
        "width_m": 7.0,
        "depth_m": 4.4,
        "wall_height_m": 3.25,
        "roof_height_m": 1.35,
        "roof_sections": 4,
        "asymmetry_ratio": 0.06,
        "window_count": 2,
        "features": ["low_broad_roof", "side_canopy"],
        "triangle_budget": 2500,
    },
    "courtyard_wing": {
        "file": "SM_QS_B1_CourtyardWing.fbx",
        "story_count": 1,
        "width_m": 6.2,
        "depth_m": 5.4,
        "wall_height_m": 3.2,
        "roof_height_m": 1.5,
        "roof_sections": 3,
        "asymmetry_ratio": 0.09,
        "window_count": 2,
        "features": ["left_rear_wing", "l_plan"],
        "triangle_budget": 2500,
    },
    "bridge_house": {
        "file": "SM_QS_B1_BridgeHouse.fbx",
        "story_count": 2,
        "width_m": 4.4,
        "depth_m": 5.4,
        "wall_height_m": 4.6,
        "roof_height_m": 1.65,
        "roof_sections": 5,
        "asymmetry_ratio": 0.075,
        "window_count": 2,
        "features": ["raised_plinth", "deep_eaves"],
        "triangle_budget": 2500,
    },
    "dock_shed": {
        "file": "SM_QS_B1_DockShed.fbx",
        "story_count": 1,
        "width_m": 6.0,
        "depth_m": 4.8,
        "wall_height_m": 2.75,
        "roof_height_m": 1.2,
        "roof_sections": 4,
        "asymmetry_ratio": 0.10,
        "window_count": 1,
        "features": ["front_deck", "open_work_awning"],
        "triangle_budget": 2500,
    },
}

PROP_SPECS = {
    "sign": {"file": "SM_QS_B1_Sign.fbx", "triangle_budget": 400},
    "lantern": {"file": "SM_QS_B1_Lantern.fbx", "triangle_budget": 400},
    "banner": {"file": "SM_QS_B1_Banner.fbx", "triangle_budget": 400},
    "fence": {"file": "SM_QS_B1_Fence.fbx", "triangle_budget": 500},
    "crate": {"file": "SM_QS_B1_Crate.fbx", "triangle_budget": 400},
    "stall": {"file": "SM_QS_B1_Stall.fbx", "triangle_budget": 800},
    "well": {"file": "SM_QS_B1_Well.fbx", "triangle_budget": 800},
    "cart": {"file": "SM_QS_B1_Cart.fbx", "triangle_budget": 800},
    "rock": {"file": "SM_QS_B1_Rock.fbx", "triangle_budget": 300},
    "dock_post": {"file": "SM_QS_B1_DockPost.fbx", "triangle_budget": 300},
    "plant_card": {"file": "SM_QS_B1_PlantCard.fbx", "triangle_budget": 100},
    "mountain": {"file": "SM_QS_B1_Mountain.fbx", "triangle_budget": 300},
}


def _dry_building_entry(spec: dict) -> dict:
    return {
        "file": spec["file"],
        "material_slots": list(BUILDING_SLOT_ORDER),
        "normalized_bounds_m": [1.0, 1.0, 1.0],
        "pivot": "bottom_center",
        "front_axis": "+Y",
        "window_paper_closed": True,
        "story_count": spec["story_count"],
        "asymmetry_ratio": spec["asymmetry_ratio"],
        "roof_sections": spec["roof_sections"],
        "door_count": 1,
        "window_count": spec["window_count"],
        "triangle_budget": spec["triangle_budget"],
    }


def build_manifest_dry_run() -> dict:
    """Return the exact immutable contract without requiring Blender."""

    return {
        "schema_version": 1,
        "buildings": {
            name: _dry_building_entry(spec) for name, spec in BUILDING_SPECS.items()
        },
        "props": {
            name: {
                "file": spec["file"],
                "material_slots": list(PROP_SLOT_TABLE[name]),
                "pivot": "bottom_center",
                "front_axis": "+Y",
                "triangle_budget": spec["triangle_budget"],
            }
            for name, spec in PROP_SPECS.items()
        },
        "export_settings": dict(EXPORT_SETTINGS),
    }


def _parse_blender_args() -> argparse.Namespace:
    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    return parser.parse_args(argv)


def _require_blender_runtime() -> None:
    if bpy is None or bmesh is None or Vector is None:
        raise RuntimeError("proxy generation must run inside Blender")
    if not bpy.app.background:
        raise RuntimeError("proxy generation requires Blender background mode")
    if tuple(bpy.app.version[:3]) < (4, 2, 3):
        raise RuntimeError("proxy generation requires Blender 4.2.3 or newer")


def _require_finished(result: set[str], operation: str) -> None:
    if "FINISHED" not in result:
        raise RuntimeError(f"{operation} did not finish: {sorted(result)}")


def _apply_small_bevel(obj, width: float = SMALL_BEVEL_WIDTH_M) -> None:
    modifier = obj.modifiers.new(name="Small_Form_Bevel", type="BEVEL")
    modifier.width = width
    modifier.segments = 1
    modifier.affect = "EDGES"
    modifier.limit_method = "ANGLE"
    modifier.angle_limit = math.radians(30.0)
    modifier.offset_type = "OFFSET"
    modifier.use_clamp_overlap = True
    modifier.profile = 0.5
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    _require_finished(
        bpy.ops.object.modifier_apply(modifier=modifier.name),
        f"apply bevel {obj.name}",
    )
    obj.select_set(False)
    bpy.context.view_layer.objects.active = None


def _create_mesh_object(
    name: str,
    vertices: Sequence[Sequence[float]],
    faces: Sequence[Sequence[int]],
    material,
    *,
    bevel_width: float = 0.0,
    uv_from_xz: bool = False,
):
    mesh = bpy.data.meshes.new(f"{name}_Mesh")
    mesh.from_pydata([tuple(vertex) for vertex in vertices], [], faces)
    mesh.materials.append(material)
    mesh.validate(verbose=False, clean_customdata=False)
    mesh.update(calc_edges=True)
    if uv_from_xz:
        uv_layer = mesh.uv_layers.new(name="UVMap")
        xs = [float(vertex[0]) for vertex in vertices]
        zs = [float(vertex[2]) for vertex in vertices]
        min_x, max_x = min(xs), max(xs)
        min_z, max_z = min(zs), max(zs)
        extent_x = max(max_x - min_x, 1.0e-8)
        extent_z = max(max_z - min_z, 1.0e-8)
        for loop in mesh.loops:
            vertex = mesh.vertices[loop.vertex_index].co
            uv_layer.data[loop.index].uv = (
                (vertex.x - min_x) / extent_x,
                (vertex.z - min_z) / extent_z,
            )
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    if bevel_width > 0.0:
        _apply_small_bevel(obj, bevel_width)
    return obj


def create_tapered_box(
    name: str,
    *,
    bottom_center: Sequence[float],
    bottom_size: Sequence[float],
    top_center: Sequence[float],
    top_size: Sequence[float],
    z_min: float,
    z_max: float,
    material,
    bevel_width: float = SMALL_BEVEL_WIDTH_M,
):
    if z_max <= z_min or min(*bottom_size, *top_size) <= 0.0:
        raise ValueError(f"{name}: tapered box dimensions must be positive")
    bx, by = (float(value) for value in bottom_center)
    tx, ty = (float(value) for value in top_center)
    bhx, bhy = (float(value) * 0.5 for value in bottom_size)
    thx, thy = (float(value) * 0.5 for value in top_size)
    vertices = (
        (bx - bhx, by - bhy, z_min),
        (bx + bhx, by - bhy, z_min),
        (bx + bhx, by + bhy, z_min),
        (bx - bhx, by + bhy, z_min),
        (tx - thx, ty - thy, z_max),
        (tx + thx, ty - thy, z_max),
        (tx + thx, ty + thy, z_max),
        (tx - thx, ty + thy, z_max),
    )
    faces = (
        (3, 2, 1, 0),
        (4, 5, 6, 7),
        (0, 1, 5, 4),
        (1, 2, 6, 5),
        (2, 3, 7, 6),
        (3, 0, 4, 7),
    )
    return _create_mesh_object(
        name, vertices, faces, material, bevel_width=bevel_width
    )


def create_beam_between(
    name: str,
    start: Sequence[float],
    end: Sequence[float],
    *,
    width: float,
    depth: float,
    material,
):
    start_vector = Vector(start)
    end_vector = Vector(end)
    axis = end_vector - start_vector
    if axis.length <= 1.0e-8 or width <= 0.0 or depth <= 0.0:
        raise ValueError(f"{name}: beam dimensions and length must be positive")
    axis.normalize()
    reference = Vector((0.0, 0.0, 1.0))
    if abs(axis.dot(reference)) > 0.92:
        reference = Vector((0.0, 1.0, 0.0))
    side = axis.cross(reference).normalized()
    other = side.cross(axis).normalized()
    offsets = (
        -side * width * 0.5 - other * depth * 0.5,
        side * width * 0.5 - other * depth * 0.5,
        side * width * 0.5 + other * depth * 0.5,
        -side * width * 0.5 + other * depth * 0.5,
    )
    vertices = tuple(start_vector + offset for offset in offsets) + tuple(
        end_vector + offset for offset in offsets
    )
    faces = (
        (1, 2, 3, 0),
        (7, 6, 5, 4),
        (4, 5, 1, 0),
        (5, 6, 2, 1),
        (6, 7, 3, 2),
        (7, 4, 0, 3),
    )
    return _create_mesh_object(
        name, vertices, faces, material, bevel_width=SMALL_BEVEL_WIDTH_M
    )


def _create_closed_window_pane(
    name: str,
    corners: Sequence,
    *,
    thickness: float,
    material,
):
    if len(corners) != 4 or thickness <= 0.0:
        raise ValueError(f"{name}: pane requires four corners and positive thickness")
    horizontal = corners[1] - corners[0]
    vertical = corners[3] - corners[0]
    normal = horizontal.cross(vertical)
    if normal.length <= 1.0e-8:
        raise ValueError(f"{name}: pane corners must span a plane")
    normal.normalize()
    half = thickness * 0.5
    vertices = tuple(corner + normal * half for corner in corners) + tuple(
        corner - normal * half for corner in corners
    )
    faces = (
        (0, 1, 2, 3),
        (7, 6, 5, 4),
        (0, 4, 5, 1),
        (1, 5, 6, 2),
        (2, 6, 7, 3),
        (3, 7, 4, 0),
    )
    return _create_mesh_object(name, vertices, faces, material)


def create_chunky_window(
    name: str,
    *,
    center: Sequence[float],
    width: float,
    height: float,
    frame_width: float,
    depth: float,
    timber_material,
    paper_material,
):
    horizontal = Vector((1.0, 0.0, 0.0))
    center_vector = Vector(center)
    lower_center = center_vector - Vector((0.0, 0.0, height * 0.5))
    upper_center = center_vector + Vector((0.0, 0.0, height * 0.5))
    upper_center += horizontal * (width * 0.04)
    lower_left = lower_center - horizontal * (width * 0.5)
    lower_right = lower_center + horizontal * (width * 0.5)
    upper_left = upper_center - horizontal * (width * 0.46)
    upper_right = upper_center + horizontal * (width * 0.46)
    objects = [
        create_beam_between(
            f"{name}_Post_L", lower_left, upper_left,
            width=frame_width, depth=depth, material=timber_material,
        ),
        create_beam_between(
            f"{name}_Post_R", lower_right, upper_right,
            width=frame_width, depth=depth, material=timber_material,
        ),
        create_beam_between(
            f"{name}_Rail_B", lower_left, lower_right,
            width=frame_width, depth=depth, material=timber_material,
        ),
        create_beam_between(
            f"{name}_Rail_T", upper_left, upper_right,
            width=frame_width, depth=depth, material=timber_material,
        ),
    ]
    lower_middle = lower_left.lerp(lower_right, 0.5)
    upper_middle = upper_left.lerp(upper_right, 0.5)
    objects.append(
        create_beam_between(
            f"{name}_Division", lower_middle, upper_middle,
            width=frame_width * 0.9, depth=depth, material=timber_material,
        )
    )
    vertical_inset = Vector((0.0, 0.0, frame_width * 0.62))
    horizontal_inset = horizontal * (frame_width * 0.62)
    for index, (lower_start, lower_end, upper_start, upper_end) in enumerate(
        (
            (lower_left, lower_middle, upper_left, upper_middle),
            (lower_middle, lower_right, upper_middle, upper_right),
        ),
        start=1,
    ):
        objects.append(
            _create_closed_window_pane(
                f"{name}_Pane_{index:02d}",
                (
                    lower_start + horizontal_inset + vertical_inset,
                    lower_end - horizontal_inset + vertical_inset,
                    upper_end - horizontal_inset - vertical_inset,
                    upper_start + horizontal_inset - vertical_inset,
                ),
                thickness=min(depth * 0.2, 0.035),
                material=paper_material,
            )
        )
    return objects


def create_low_segment_roof(
    name: str,
    *,
    width: float,
    depth: float,
    eave_z: float,
    roof_height: float,
    section_count: int,
    ridge_offset: float,
    material,
    center_x: float = 0.0,
    center_y: float = 0.0,
):
    """Create one closed 3-5-section tile-free roof shell with broad upturned ends."""

    if section_count not in {3, 4, 5}:
        raise ValueError("low segment roof requires 3-5 longitudinal sections")
    half_x = width * 0.5
    half_y = depth * 0.5
    thickness = max(0.08, roof_height * 0.08)
    cross_x = (-half_x, ridge_offset, half_x)
    top_vertices: list[tuple[float, float, float]] = []
    for station in range(section_count + 1):
        v = -1.0 + 2.0 * station / section_count
        y = center_y + half_y * v
        end_lift = 0.11 * roof_height * abs(v) ** 2
        ridge_bow = 0.045 * roof_height * math.cos(v * math.pi * 0.5)
        top_vertices.extend(
            (
                (center_x + cross_x[0], y, eave_z + end_lift),
                (center_x + cross_x[1], y, eave_z + roof_height + ridge_bow),
                (center_x + cross_x[2], y, eave_z + end_lift + 0.035 * roof_height),
            )
        )
    vertices = top_vertices + [
        (x, y, z - thickness) for x, y, z in top_vertices
    ]
    layer_size = len(top_vertices)
    faces: list[tuple[int, int, int, int]] = []
    for station in range(section_count):
        for across in range(2):
            first = station * 3 + across
            following = (station + 1) * 3 + across
            faces.append((first, first + 1, following + 1, following))
            faces.append(
                (
                    following + layer_size,
                    following + 1 + layer_size,
                    first + 1 + layer_size,
                    first + layer_size,
                )
            )
    for across in range(2):
        faces.append(
            (
                across + layer_size,
                across + 1 + layer_size,
                across + 1,
                across,
            )
        )
        last = section_count * 3 + across
        faces.append(
            (
                last,
                last + 1,
                last + 1 + layer_size,
                last + layer_size,
            )
        )
    for station in range(section_count):
        left = station * 3
        next_left = (station + 1) * 3
        faces.append(
            (next_left, left, left + layer_size, next_left + layer_size)
        )
        right = station * 3 + 2
        next_right = (station + 1) * 3 + 2
        faces.append(
            (right, next_right, next_right + layer_size, right + layer_size)
        )
    return _create_mesh_object(name, vertices, faces, material)


def _make_material(name: str, color: Sequence[float], roughness: float):
    material = bpy.data.materials.new(name)
    material.diffuse_color = tuple(float(value) for value in color)
    material.use_nodes = True
    principled = material.node_tree.nodes.get("Principled BSDF")
    if principled is not None:
        principled.inputs["Base Color"].default_value = material.diffuse_color
        principled.inputs["Roughness"].default_value = roughness
        metallic = principled.inputs.get("Metallic")
        if metallic is not None:
            metallic.default_value = 0.0
    return material


def _create_materials() -> dict:
    return {
        "Wall": _make_material("Wall", (0.72, 0.54, 0.34, 1.0), 0.90),
        "Timber": _make_material("Timber", (0.075, 0.05, 0.037, 1.0), 0.86),
        "WindowPaper": _make_material(
            "WindowPaper", (0.86, 0.66, 0.30, 1.0), 0.96
        ),
        "Roof": _make_material("Roof", (0.42, 0.105, 0.055, 1.0), 0.88),
        "Prop": _make_material("Prop", (0.20, 0.43, 0.40, 1.0), 0.91),
        "Foliage": _make_material("Foliage", (0.15, 0.38, 0.28, 1.0), 0.95),
        "Mountain": _make_material("Mountain", (0.26, 0.38, 0.42, 1.0), 0.97),
    }


def _apply_mesh_transforms(objects: Iterable) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    for obj in sorted(objects, key=lambda candidate: candidate.name):
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        _require_finished(
            bpy.ops.object.transform_apply(location=True, rotation=True, scale=True),
            f"apply transforms {obj.name}",
        )
        obj.select_set(False)
    bpy.context.view_layer.objects.active = None


def _join_and_finalize(
    name: str,
    objects: Sequence,
    slot_names: Sequence[str],
    materials: dict,
    *,
    normalize_unit_bounds: bool,
):
    if not objects:
        raise RuntimeError(f"{name}: cannot finalize an empty mesh")
    _apply_mesh_transforms(objects)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    active = objects[0]
    bpy.context.view_layer.objects.active = active
    if len(objects) > 1:
        _require_finished(bpy.ops.object.join(), f"join {name}")
    active.name = name
    active.data.name = f"{name}_Mesh"

    old_materials = list(active.data.materials)
    polygon_materials = [
        old_materials[polygon.material_index].name for polygon in active.data.polygons
    ]
    unexpected = sorted(set(polygon_materials).difference(slot_names))
    if unexpected:
        raise RuntimeError(f"{name}: unexpected material slots {unexpected}")
    active.data.materials.clear()
    for slot_name in slot_names:
        active.data.materials.append(materials[slot_name])
    slot_index = {slot_name: index for index, slot_name in enumerate(slot_names)}
    for polygon, material_name in zip(active.data.polygons, polygon_materials):
        polygon.material_index = slot_index[material_name]

    coordinates = [vertex.co.copy() for vertex in active.data.vertices]
    minimum = Vector(
        (
            min(point.x for point in coordinates),
            min(point.y for point in coordinates),
            min(point.z for point in coordinates),
        )
    )
    maximum = Vector(
        (
            max(point.x for point in coordinates),
            max(point.y for point in coordinates),
            max(point.z for point in coordinates),
        )
    )
    size = maximum - minimum
    if min(size) <= 1.0e-8:
        raise RuntimeError(f"{name}: mesh has a zero-size bound")
    center_x = (minimum.x + maximum.x) * 0.5
    center_y = (minimum.y + maximum.y) * 0.5
    for vertex in active.data.vertices:
        vertex.co.x -= center_x
        vertex.co.y -= center_y
        vertex.co.z -= minimum.z
        if normalize_unit_bounds:
            vertex.co.x /= size.x
            vertex.co.y /= size.y
            vertex.co.z /= size.z
    active.location = (0.0, 0.0, 0.0)
    active.rotation_euler = (0.0, 0.0, 0.0)
    active.scale = (1.0, 1.0, 1.0)
    active.data.validate(verbose=False, clean_customdata=False)
    active.data.update(calc_edges=True)
    bpy.context.view_layer.objects.active = active
    return active


def _add_main_body_and_roof(name: str, spec: dict, materials: dict) -> list:
    width = float(spec["width_m"])
    depth = float(spec["depth_m"])
    wall_height = float(spec["wall_height_m"])
    asymmetry = float(spec["asymmetry_ratio"])
    shift_x = width * asymmetry
    deep_eaves = "deep_eaves" in set(spec["features"])
    objects = []
    if int(spec["story_count"]) == 2:
        lower_top = wall_height * 0.52
        objects.append(
            create_tapered_box(
                f"{name}_Body_Lower",
                bottom_center=(-shift_x * 0.25, 0.0),
                bottom_size=(width * 0.92, depth * 0.92),
                top_center=(shift_x * 0.20, -depth * 0.015),
                top_size=(width * 0.88, depth * 0.88),
                z_min=0.0,
                z_max=lower_top,
                material=materials["Wall"],
            )
        )
        objects.append(
            create_tapered_box(
                f"{name}_Body_Upper",
                bottom_center=(shift_x * 0.18, -depth * 0.01),
                bottom_size=(width * 0.88, depth * 0.88),
                top_center=(shift_x, -depth * 0.05),
                top_size=(width * 0.80, depth * 0.82),
                z_min=lower_top * 0.96,
                z_max=wall_height,
                material=materials["Wall"],
            )
        )
    else:
        objects.append(
            create_tapered_box(
                f"{name}_Body",
                bottom_center=(-shift_x * 0.22, 0.0),
                bottom_size=(width * 0.92, depth * 0.92),
                top_center=(shift_x, -depth * 0.04),
                top_size=(width * 0.84, depth * 0.84),
                z_min=0.0,
                z_max=wall_height,
                material=materials["Wall"],
            )
        )
    objects.append(
        create_low_segment_roof(
            f"{name}_Roof",
            width=width * (1.14 if deep_eaves else 1.08),
            depth=depth * (1.18 if deep_eaves else 1.10),
            eave_z=wall_height * 0.96,
            roof_height=float(spec["roof_height_m"]),
            section_count=int(spec["roof_sections"]),
            ridge_offset=-shift_x * 0.45,
            material=materials["Roof"],
        )
    )
    return objects


def _add_door_windows_and_columns(name: str, spec: dict, materials: dict) -> list:
    width = float(spec["width_m"])
    depth = float(spec["depth_m"])
    wall_height = float(spec["wall_height_m"])
    asymmetry = float(spec["asymmetry_ratio"])
    facade_y = depth * 0.46 + 0.08
    door_height = min(1.9, wall_height * 0.58)
    door_width = max(1.05, width * 0.20)
    objects = [
        create_tapered_box(
            f"{name}_Door",
            bottom_center=(width * asymmetry * 0.40, facade_y),
            bottom_size=(door_width, 0.18),
            top_center=(width * asymmetry * 0.48, facade_y - 0.01),
            top_size=(door_width * 0.93, 0.18),
            z_min=0.03,
            z_max=door_height,
            material=materials["Timber"],
        )
    ]
    count = int(spec["window_count"])
    story_count = int(spec["story_count"])
    if count == 1:
        x_positions = [-width * 0.22]
    elif count == 2:
        x_positions = [-width * 0.27, width * 0.27]
    else:
        x_positions = [-width * 0.31, 0.0, width * 0.31]
    for index, x_position in enumerate(x_positions):
        if abs(x_position - width * asymmetry * 0.40) < door_width * 0.75:
            z = wall_height * (0.73 if story_count == 2 else 0.54)
        else:
            z = wall_height * (0.30 if story_count == 2 else 0.50)
        objects.extend(
            create_chunky_window(
                f"{name}_Window_{index + 1:02d}",
                center=(x_position, facade_y + 0.035, z),
                width=max(1.0, width * 0.19),
                height=max(0.78, min(1.15, wall_height * 0.22)),
                frame_width=0.13,
                depth=0.18,
                timber_material=materials["Timber"],
                paper_material=materials["WindowPaper"],
            )
        )
    for index, x_position in enumerate((-width * 0.43, width * 0.41)):
        lean = width * asymmetry * (0.55 if index == 0 else -0.35)
        objects.append(
            create_beam_between(
                f"{name}_Column_{index + 1:02d}",
                (x_position, facade_y + 0.08, 0.05),
                (x_position + lean, facade_y, wall_height * 0.98),
                width=0.22,
                depth=0.24,
                material=materials["Timber"],
            )
        )
    return objects


def _add_building_feature(name: str, spec: dict, materials: dict) -> list:
    features = set(spec["features"])
    width = float(spec["width_m"])
    depth = float(spec["depth_m"])
    wall_height = float(spec["wall_height_m"])
    objects = []
    if "front_awning" in features or "open_work_awning" in features:
        awning_y = depth * 0.50 + 0.55
        objects.append(
            create_low_segment_roof(
                f"{name}_Awning",
                width=width * (0.76 if "front_awning" in features else 0.62),
                depth=1.15,
                eave_z=wall_height * 0.56,
                roof_height=0.38,
                section_count=3,
                ridge_offset=-width * 0.025,
                material=materials["Roof"],
                center_y=awning_y,
            )
        )
    if "side_canopy" in features:
        objects.append(
            create_low_segment_roof(
                f"{name}_SideCanopy",
                width=width * 0.38,
                depth=depth * 0.55,
                eave_z=wall_height * 0.62,
                roof_height=0.45,
                section_count=3,
                ridge_offset=0.0,
                center_x=-width * 0.46,
                material=materials["Roof"],
            )
        )
    if "left_rear_wing" in features:
        wing_x = -width * 0.42
        wing_y = -depth * 0.30
        wing_width = width * 0.48
        wing_depth = depth * 0.62
        objects.append(
            create_tapered_box(
                f"{name}_WingBody",
                bottom_center=(wing_x, wing_y),
                bottom_size=(wing_width, wing_depth),
                top_center=(wing_x - width * 0.025, wing_y + depth * 0.02),
                top_size=(wing_width * 0.88, wing_depth * 0.86),
                z_min=0.0,
                z_max=wall_height * 0.72,
                material=materials["Wall"],
            )
        )
        objects.append(
            create_low_segment_roof(
                f"{name}_WingRoof",
                width=wing_width * 1.12,
                depth=wing_depth * 1.12,
                eave_z=wall_height * 0.69,
                roof_height=float(spec["roof_height_m"]) * 0.72,
                section_count=3,
                ridge_offset=-width * 0.025,
                center_x=wing_x,
                center_y=wing_y,
                material=materials["Roof"],
            )
        )
    if "raised_plinth" in features:
        objects.append(
            create_tapered_box(
                f"{name}_Plinth",
                bottom_center=(0.0, 0.0),
                bottom_size=(width * 0.98, depth * 0.98),
                top_center=(width * 0.015, 0.0),
                top_size=(width * 0.93, depth * 0.93),
                z_min=0.0,
                z_max=0.28,
                material=materials["Timber"],
            )
        )
    if "front_deck" in features:
        objects.append(
            create_tapered_box(
                f"{name}_Deck",
                bottom_center=(0.0, depth * 0.60),
                bottom_size=(width * 0.82, depth * 0.32),
                top_center=(width * 0.02, depth * 0.60),
                top_size=(width * 0.80, depth * 0.30),
                z_min=0.0,
                z_max=0.18,
                material=materials["Timber"],
            )
        )
    return objects


def _build_building(name: str, spec: dict, materials: dict):
    objects = _add_main_body_and_roof(name, spec, materials)
    objects.extend(_add_door_windows_and_columns(name, spec, materials))
    objects.extend(_add_building_feature(name, spec, materials))
    return _join_and_finalize(
        f"SM_QS_B1_{name}",
        objects,
        BUILDING_SLOT_ORDER,
        materials,
        normalize_unit_bounds=True,
    )


def _box(
    name: str,
    center: Sequence[float],
    size: Sequence[float],
    material,
    *,
    top_shift: Sequence[float] = (0.0, 0.0),
):
    return create_tapered_box(
        name,
        bottom_center=(center[0], center[1]),
        bottom_size=(size[0], size[1]),
        top_center=(center[0] + top_shift[0], center[1] + top_shift[1]),
        top_size=(size[0] * 0.94, size[1] * 0.94),
        z_min=center[2] - size[2] * 0.5,
        z_max=center[2] + size[2] * 0.5,
        material=material,
    )


def _cylinder(name: str, *, radius: float, depth: float, location, material, rotation=None):
    result = bpy.ops.mesh.primitive_cylinder_add(
        vertices=8,
        radius=radius,
        depth=depth,
        end_fill_type="NGON",
        location=location,
        rotation=rotation or (0.0, 0.0, 0.0),
    )
    _require_finished(result, f"create cylinder {name}")
    obj = bpy.context.active_object
    obj.name = name
    obj.data.name = f"{name}_Mesh"
    obj.data.materials.append(material)
    _apply_small_bevel(obj, min(SMALL_BEVEL_WIDTH_M, radius * 0.08))
    return obj


def _build_prop(name: str, materials: dict):
    timber = materials["Timber"]
    prop = materials["Prop"]
    objects = []
    if name == "sign":
        objects = [
            _box("Sign_Post", (0.0, 0.0, 0.85), (0.13, 0.13, 1.7), timber),
            _box("Sign_Board", (0.18, 0.02, 1.45), (0.95, 0.16, 0.58), prop, top_shift=(0.04, -0.01)),
        ]
    elif name == "lantern":
        objects = [
            _box("Lantern_Top", (0.0, 0.0, 1.05), (0.62, 0.52, 0.12), timber),
            _box("Lantern_Paper", (0.02, 0.0, 0.67), (0.48, 0.40, 0.68), materials["WindowPaper"], top_shift=(0.04, 0.0)),
            _box("Lantern_Base", (0.03, 0.0, 0.27), (0.58, 0.48, 0.12), timber),
            _box("Lantern_Stem", (0.0, 0.0, 0.13), (0.10, 0.10, 0.26), timber),
        ]
    elif name == "banner":
        objects = [
            _box("Banner_Pole", (-0.38, 0.0, 1.15), (0.12, 0.12, 2.30), timber),
            _box("Banner_Crossbar", (0.0, 0.0, 2.05), (0.88, 0.12, 0.12), timber),
            _box("Banner_Cloth", (0.08, 0.02, 1.38), (0.72, 0.08, 1.22), prop, top_shift=(0.05, 0.0)),
        ]
    elif name == "fence":
        objects = [
            _box("Fence_Post_L", (-0.95, 0.0, 0.60), (0.16, 0.18, 1.20), timber, top_shift=(-0.04, 0.0)),
            _box("Fence_Post_R", (0.92, 0.0, 0.56), (0.16, 0.18, 1.12), timber, top_shift=(0.05, 0.0)),
            create_beam_between("Fence_Rail_A", (-0.93, 0.0, 0.37), (0.94, 0.0, 0.45), width=0.13, depth=0.15, material=timber),
            create_beam_between("Fence_Rail_B", (-0.96, 0.0, 0.82), (0.91, 0.0, 0.75), width=0.13, depth=0.15, material=timber),
        ]
    elif name == "crate":
        objects = [
            _box("Crate_Body", (0.0, 0.0, 0.52), (1.0, 0.92, 1.04), timber, top_shift=(0.04, -0.03)),
            create_beam_between("Crate_Brace_A", (-0.45, 0.48, 0.10), (0.45, 0.48, 0.94), width=0.10, depth=0.09, material=timber),
            create_beam_between("Crate_Brace_B", (0.45, 0.49, 0.10), (-0.45, 0.49, 0.94), width=0.10, depth=0.09, material=timber),
        ]
    elif name == "stall":
        objects = [
            _box("Stall_Post_L", (-0.92, 0.0, 1.05), (0.14, 0.14, 2.10), timber, top_shift=(-0.06, 0.0)),
            _box("Stall_Post_R", (0.88, 0.0, 1.02), (0.14, 0.14, 2.04), timber, top_shift=(0.08, 0.0)),
            _box("Stall_Counter", (0.0, 0.0, 0.78), (2.0, 0.72, 0.20), timber, top_shift=(0.05, 0.0)),
            _box("Stall_Base", (0.0, -0.12, 0.39), (1.75, 0.62, 0.70), timber),
            _box("Stall_Canopy", (0.0, 0.0, 2.18), (2.30, 1.12, 0.18), prop, top_shift=(0.12, -0.03)),
        ]
    elif name == "well":
        objects = [
            _cylinder("Well_Ring", radius=0.72, depth=0.55, location=(0.0, 0.0, 0.275), material=prop),
            _box("Well_Post_L", (-0.78, 0.0, 1.05), (0.14, 0.16, 1.65), timber, top_shift=(-0.04, 0.0)),
            _box("Well_Post_R", (0.76, 0.0, 1.04), (0.14, 0.16, 1.62), timber, top_shift=(0.05, 0.0)),
            create_beam_between("Well_Crossbar", (-0.82, 0.0, 1.73), (0.80, 0.0, 1.78), width=0.15, depth=0.16, material=timber),
        ]
    elif name == "cart":
        objects = [
            _box("Cart_Bed", (0.0, 0.0, 0.68), (0.92, 1.80, 0.34), timber, top_shift=(0.0, 0.05)),
            _cylinder("Cart_Wheel_L", radius=0.55, depth=0.16, location=(-0.56, -0.58, 0.55), material=timber, rotation=(0.0, math.radians(90.0), 0.0)),
            _cylinder("Cart_Wheel_R", radius=0.55, depth=0.16, location=(0.56, -0.58, 0.55), material=timber, rotation=(0.0, math.radians(90.0), 0.0)),
            create_beam_between("Cart_Handle_L", (-0.30, 0.72, 0.68), (-0.34, 2.05, 0.48), width=0.12, depth=0.12, material=timber),
            create_beam_between("Cart_Handle_R", (0.30, 0.72, 0.68), (0.34, 2.05, 0.48), width=0.12, depth=0.12, material=timber),
        ]
    elif name == "rock":
        vertices = (
            (-0.72, -0.48, 0.0), (0.58, -0.55, 0.0), (0.76, 0.32, 0.0), (-0.45, 0.58, 0.0),
            (-0.42, -0.25, 0.72), (0.28, -0.34, 0.92), (0.45, 0.20, 0.66), (-0.20, 0.32, 0.82),
        )
        faces = ((3, 2, 1, 0), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7))
        objects = [_create_mesh_object("Rock_Main", vertices, faces, prop, bevel_width=0.025)]
    elif name == "dock_post":
        objects = [
            _box("DockPost_Main", (0.0, 0.0, 0.90), (0.32, 0.34, 1.80), timber, top_shift=(0.10, -0.04)),
            _box("DockPost_Cap", (0.10, -0.04, 1.82), (0.46, 0.48, 0.18), timber, top_shift=(0.03, 0.0)),
        ]
    elif name == "plant_card":
        vertices = (
            (-0.65, -0.012, 0.0), (0.65, -0.012, 0.0), (0.65, -0.012, 1.8), (-0.65, -0.012, 1.8),
            (-0.65, 0.012, 0.0), (0.65, 0.012, 0.0), (0.65, 0.012, 1.8), (-0.65, 0.012, 1.8),
        )
        faces = ((3, 2, 1, 0), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7))
        objects = [_create_mesh_object("PlantCard_Main", vertices, faces, materials["Foliage"], uv_from_xz=True)]
    elif name == "mountain":
        vertices = (
            (-1.30, -0.58, 0.0), (1.12, -0.62, 0.0), (1.28, 0.50, 0.0), (-1.18, 0.60, 0.0),
            (-0.38, -0.20, 1.70), (0.20, -0.28, 2.55), (0.52, 0.18, 1.52), (-0.46, 0.28, 2.08),
        )
        faces = ((3, 2, 1, 0), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7))
        objects = [_create_mesh_object("Mountain_Main", vertices, faces, materials["Mountain"])]
    else:
        raise ValueError(f"unknown proxy prop: {name}")
    return _join_and_finalize(
        f"SM_QS_B1_{name}",
        objects,
        PROP_SLOT_TABLE[name],
        materials,
        normalize_unit_bounds=False,
    )


def _mesh_bounds(obj) -> dict:
    points = [vertex.co for vertex in obj.data.vertices]
    minimum = [min(float(point[axis]) for point in points) for axis in range(3)]
    maximum = [max(float(point[axis]) for point in points) for axis in range(3)]
    size = [upper - lower for lower, upper in zip(minimum, maximum)]
    return {
        "min": [round(value, 6) for value in minimum],
        "max": [round(value, 6) for value in maximum],
        "size": [round(value, 6) for value in size],
    }


def _geometry_digest(obj, material_slots: Sequence[str]) -> str:
    mesh = obj.data
    mesh.calc_loop_triangles()
    triangles = sorted(
        (
            tuple(int(index) for index in triangle.vertices),
            int(triangle.material_index),
        )
        for triangle in mesh.loop_triangles
    )
    uv_layers = {}
    for layer in mesh.uv_layers:
        uv_layers[layer.name] = [
            [round(float(item.uv.x), 8), round(float(item.uv.y), 8)]
            for item in layer.data
        ]
    payload = {
        "vertices": [
            [round(float(vertex.co[axis]), 8) for axis in range(3)]
            for vertex in mesh.vertices
        ],
        "triangles": triangles,
        "material_slots": list(material_slots),
        "uv_layers": uv_layers,
    }
    encoded = json.dumps(
        payload, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def _non_manifold_edges(obj) -> int:
    audit = bmesh.new()
    try:
        audit.from_mesh(obj.data)
        return sum(1 for edge in audit.edges if not edge.is_manifold)
    finally:
        audit.free()


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _export_fbx(obj, path: Path) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    result = bpy.ops.export_scene.fbx(
        filepath=str(path),
        use_selection=True,
        object_types={"MESH"},
        axis_forward=EXPORT_SETTINGS["axis_forward"],
        axis_up=EXPORT_SETTINGS["axis_up"],
        use_triangles=EXPORT_SETTINGS["use_triangles"],
        apply_unit_scale=EXPORT_SETTINGS["apply_unit_scale"],
        apply_scale_options=EXPORT_SETTINGS["apply_scale_options"],
        path_mode=EXPORT_SETTINGS["path_mode"],
        bake_anim=EXPORT_SETTINGS["bake_anim"],
        add_leaf_bones=False,
    )
    _require_finished(result, f"export {obj.name}")
    if not path.is_file() or path.stat().st_size <= 256:
        raise RuntimeError(f"FBX export is missing or empty: {path}")


def _audit_entry(obj, path: Path, slot_names: Sequence[str], triangle_budget: int) -> dict:
    mesh = obj.data
    mesh.calc_loop_triangles()
    bounds = _mesh_bounds(obj)
    entry = {
        "file": path.name,
        "file_sha256": _sha256_file(path),
        "geometry_digest": _geometry_digest(obj, slot_names),
        "bounds_m": bounds,
        "normalized_bounds_m": list(bounds["size"]),
        "vertex_count": len(mesh.vertices),
        "triangle_count": len(mesh.loop_triangles),
        "triangle_budget": int(triangle_budget),
        "material_slots": [material.name for material in mesh.materials],
        "uv_channels": [layer.name for layer in mesh.uv_layers],
        "uv_channel_count": len(mesh.uv_layers),
        "non_manifold_edges": _non_manifold_edges(obj),
        "pivot": "bottom_center",
        "front_axis": "+Y",
        "export_settings": dict(EXPORT_SETTINGS),
    }
    if entry["material_slots"] != list(slot_names):
        raise RuntimeError(f"{obj.name}: material slot order drifted")
    if entry["triangle_count"] > triangle_budget:
        raise RuntimeError(
            f"{obj.name}: {entry['triangle_count']} triangles exceed {triangle_budget}"
        )
    if entry["non_manifold_edges"] != 0:
        raise RuntimeError(f"{obj.name}: mesh has non-manifold edges")
    return entry


def _building_silhouette_signature(spec: dict) -> str:
    payload = {
        "story_count": spec["story_count"],
        "width_depth_ratio": round(spec["width_m"] / spec["depth_m"], 6),
        "wall_roof_ratio": round(spec["wall_height_m"] / spec["roof_height_m"], 6),
        "roof_sections": spec["roof_sections"],
        "features": list(spec["features"]),
    }
    return hashlib.sha256(
        json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()


def _remove_object(obj) -> None:
    mesh = obj.data
    bpy.data.objects.remove(obj, do_unlink=True)
    if mesh.users == 0:
        bpy.data.meshes.remove(mesh)


def _validate_output_directory(path: Path) -> Path:
    resolved = path.expanduser().resolve()
    if resolved == Path(resolved.anchor) or resolved == PROJECT_ROOT.resolve():
        raise RuntimeError(f"unsafe proxy output directory: {resolved}")
    if resolved.exists() and not resolved.is_dir():
        raise RuntimeError(f"proxy output must be a directory: {resolved}")
    resolved.mkdir(parents=True, exist_ok=True)
    if resolved.is_symlink():
        raise RuntimeError(f"proxy output cannot be a symlink: {resolved}")
    return resolved


def _clear_known_outputs(output: Path) -> None:
    known = [spec["file"] for spec in BUILDING_SPECS.values()]
    known.extend(spec["file"] for spec in PROP_SPECS.values())
    known.append("proxy-kit-manifest.json")
    for filename in known:
        candidate = output / filename
        if candidate.is_file():
            candidate.unlink()


def build_proxy_kit(output: Path) -> dict:
    _require_blender_runtime()
    output = _validate_output_directory(output)
    _clear_known_outputs(output)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "SCN_Qingshan_B1_ProxyKit"
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    bpy.context.preferences.filepaths.save_version = 0
    materials = _create_materials()

    manifest = {
        "schema_version": 1,
        "blender_version": [int(value) for value in bpy.app.version[:3]],
        "export_settings": dict(EXPORT_SETTINGS),
        "buildings": {},
        "props": {},
    }
    for name, spec in BUILDING_SPECS.items():
        obj = _build_building(name, spec, materials)
        path = output / spec["file"]
        _export_fbx(obj, path)
        entry = _audit_entry(obj, path, BUILDING_SLOT_ORDER, spec["triangle_budget"])
        entry.update(
            {
                "normalized_bounds_m": [1.0, 1.0, 1.0],
                "window_paper_closed": True,
                "window_paper_color_rgba": [0.86, 0.66, 0.3, 1.0],
                "story_count": spec["story_count"],
                "asymmetry_ratio": spec["asymmetry_ratio"],
                "roof_sections": spec["roof_sections"],
                "door_count": 1,
                "window_count": spec["window_count"],
                "features": list(spec["features"]),
                "silhouette_signature": _building_silhouette_signature(spec),
            }
        )
        if entry["bounds_m"] != {
            "min": [-0.5, -0.5, 0.0],
            "max": [0.5, 0.5, 1.0],
            "size": [1.0, 1.0, 1.0],
        }:
            raise RuntimeError(f"{name}: normalized one-metre bounds drifted")
        manifest["buildings"][name] = entry
        _remove_object(obj)

    for name, spec in PROP_SPECS.items():
        obj = _build_prop(name, materials)
        path = output / spec["file"]
        _export_fbx(obj, path)
        manifest["props"][name] = _audit_entry(
            obj, path, PROP_SLOT_TABLE[name], spec["triangle_budget"]
        )
        _remove_object(obj)

    manifest_path = output / "proxy-kit-manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    return {
        "ok": True,
        "manifest": str(manifest_path),
        "fbx_count": 18,
        "geometry_digest": hashlib.sha256(
            json.dumps(
                {
                    name: entry["geometry_digest"]
                    for name, entry in {
                        **manifest["buildings"],
                        **manifest["props"],
                    }.items()
                },
                sort_keys=True,
                separators=(",", ":"),
            ).encode("utf-8")
        ).hexdigest(),
    }


def main() -> None:
    args = _parse_blender_args()
    result = build_proxy_kit(args.output)
    print(json.dumps(result, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except Exception:
        traceback.print_exc()
        raise SystemExit(1)
