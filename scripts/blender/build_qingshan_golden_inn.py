"""Build the deterministic Qingshan golden-inn Gate 1 Blender scene.

Run with Blender, keeping Blender's own arguments before the ``--`` delimiter::

    blender --background --factory-startup --python scripts/blender/build_qingshan_golden_inn.py \
        -- --project-root <project> --version v001

The committed JSON asset record and :mod:`qingshan_golden_inn` are the source of
truth.  This file is intentionally Blender-only; ordinary Python tests inspect
its structure without importing ``bpy``.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
import traceback
from pathlib import Path
from typing import Iterable, Sequence

import bmesh
import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector


SCRIPT_ROOT = Path(__file__).resolve().parents[1]
if str(SCRIPT_ROOT) not in sys.path:
    sys.path.insert(0, str(SCRIPT_ROOT))

from qingshan_golden_inn import (  # noqa: E402
    REQUIRED_VIEWS,
    VERSIONS,
    build_component_plan,
    load_golden_contract,
    output_paths,
    sha256_file,
)


ASSET_RECORD = Path(
    "SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/asset.json"
)
CANONICAL_OUTPUT_RELATIVE = Path(
    "SourceAssets/TownPCG/QingshanEnvironment/assets/"
    "BLD_QS_M_A_INN/source/blender"
)
MODULE_NAMES = ("BODY", "ROOF_A", "DOOR_A", "WINDOW_A", "EAVE_A")
MAIN_RESOLUTION = (1536, 1024)
SILHOUETTE_RESOLUTION = (128, 128)
IDENTITY_TOLERANCE = 1.0e-6
SMALL_BEVEL_WIDTH_M = 0.01
LARGE_FORM_CONTRACT = {
    "storeys": 2,
    "body_width_m": 4.0,
    "ground_floor_top_z_m": 1.96,
    "upper_floor_top_z_m": 3.58,
    "upper_floor_shift_x_m": 0.24,
    "door_width_m": 1.62,
    "door_height_m": 1.68,
    "window_count": 3,
    "window_divisions": 2,
    "min_window_width_m": 1.20,
    "capital_top_width_m": 0.92,
    "balcony_band_z_m": 1.98,
}


def _parse_blender_args() -> argparse.Namespace:
    """Parse only arguments supplied after Blender's ``--`` boundary."""

    argv = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project-root", type=Path, required=True)
    parser.add_argument("--version", choices=VERSIONS, required=True)
    parser.add_argument("--self-test-audit", action="store_true")
    return parser.parse_args(argv)


def _require_supported_background_blender() -> None:
    """Refuse destructive scene reset outside supported background Blender."""

    if not bpy.app.background:
        raise RuntimeError("golden-inn generation requires Blender background mode")
    if tuple(bpy.app.version[:3]) < (4, 2, 0):
        raise RuntimeError("golden-inn generation requires Blender 4.2 or newer")


def _validate_output_destinations(
    project_root: Path, paths: dict[str, Path]
) -> tuple[Path, dict[str, Path]]:
    """Resolve and contain every output path before any mutation or scene reset."""

    try:
        resolved_root = project_root.expanduser().resolve(strict=True)
    except (OSError, RuntimeError, ValueError) as exc:
        raise RuntimeError(f"project root cannot be resolved: {exc}") from exc
    if not resolved_root.is_dir():
        raise RuntimeError(f"project root must be a directory: {resolved_root}")

    canonical_output = (resolved_root / CANONICAL_OUTPUT_RELATIVE).absolute()
    ancestor = resolved_root
    for part in CANONICAL_OUTPUT_RELATIVE.parts:
        ancestor = ancestor / part
        if ancestor.exists() or ancestor.is_symlink():
            if not ancestor.is_dir():
                raise RuntimeError(f"output ancestor must be a directory: {ancestor}")
            resolved_ancestor = ancestor.resolve(strict=True)
            if resolved_ancestor != ancestor.absolute():
                raise RuntimeError(f"output ancestor redirects through symlink/junction: {ancestor}")
            try:
                resolved_ancestor.relative_to(resolved_root)
            except ValueError as exc:
                raise RuntimeError(f"output ancestor escapes project root: {ancestor}") from exc

    validated: dict[str, Path] = {}
    for key, supplied in paths.items():
        lexical = supplied.expanduser().absolute()
        if lexical.parent != canonical_output:
            raise RuntimeError(f"{key} must remain in canonical output directory")
        try:
            resolved = supplied.resolve(strict=False)
        except (OSError, RuntimeError, ValueError) as exc:
            raise RuntimeError(f"{key} output cannot be resolved: {exc}") from exc
        if resolved != lexical:
            raise RuntimeError(f"{key} output redirects through symlink/junction")
        try:
            resolved.relative_to(resolved_root)
        except ValueError as exc:
            raise RuntimeError(f"{key} output escapes resolved project root") from exc
        if supplied.exists() and not supplied.is_file():
            raise RuntimeError(f"{key} output must be a file path")
        validated[key] = lexical

    scoped_sidecars = (
        validated["blend"].with_name(f"{validated['blend'].name}1"),
        validated["report"].with_name(f"{validated['report'].name}.tmp"),
    )
    for sidecar in scoped_sidecars:
        resolved_sidecar = sidecar.resolve(strict=False)
        if resolved_sidecar != sidecar.absolute():
            raise RuntimeError(f"scoped sidecar redirects through symlink/junction: {sidecar}")
        if sidecar.exists() and not sidecar.is_file():
            raise RuntimeError(f"scoped sidecar must be a file path: {sidecar}")
    return resolved_root, validated


def _remove_stale_blend_backup(blend_path: Path) -> None:
    """Remove only Blender's sidecar for this canonical blend output."""

    backup_path = blend_path.with_name(f"{blend_path.name}1")
    if backup_path.is_file():
        backup_path.unlink()


def _invalidate_previous_outputs(paths: dict[str, Path]) -> None:
    """Delete only this version's validated artifacts and known sidecars."""

    for path in paths.values():
        if path.is_file():
            path.unlink()
    blend_backup = paths["blend"].with_name(f"{paths['blend'].name}1")
    report_temp = paths["report"].with_name(f"{paths['report'].name}.tmp")
    for sidecar in (blend_backup, report_temp):
        if sidecar.is_file():
            sidecar.unlink()


def _refuse_existing_self_test_outputs(paths: dict[str, Path]) -> None:
    """Keep audit-only mode from overwriting or deleting production artifacts."""

    scoped = [*paths.values()]
    scoped.extend(
        (
            paths["blend"].with_name(f"{paths['blend'].name}1"),
            paths["report"].with_name(f"{paths['report'].name}.tmp"),
        )
    )
    existing = sorted(path for path in scoped if path.exists() or path.is_symlink())
    if existing:
        rendered = ", ".join(str(path) for path in existing)
        raise RuntimeError(f"self-test refuses existing canonical output: {rendered}")


def _require_finished(result: set[str], operation: str) -> None:
    """Require a Blender operator to explicitly report successful completion."""

    if "FINISHED" not in result:
        raise RuntimeError(f"{operation} did not finish: {sorted(result)}")


def _assert_current_run_outputs(
    paths: dict[str, Path], created_keys: set[str]
) -> None:
    """Prove every hashed artifact was created successfully in this run."""

    expected = {"blend", *REQUIRED_VIEWS, "silhouette_128"}
    if created_keys != expected:
        missing = sorted(expected.difference(created_keys))
        unexpected = sorted(created_keys.difference(expected))
        raise RuntimeError(
            f"current-run outputs mismatch; missing={missing}, unexpected={unexpected}"
        )
    for key in sorted(expected):
        path = paths[key]
        if not path.is_file() or path.stat().st_size <= 0:
            raise RuntimeError(f"current-run output is missing or empty: {key}: {path}")


def _apply_small_bevel(obj: bpy.types.Object, width: float) -> None:
    """Apply one conservative 1-segment bevel to a closed block primitive."""

    modifier = obj.modifiers.new(name="Small_Form_Bevel", type="BEVEL")
    modifier.width = width
    modifier.segments = 1
    modifier.affect = "EDGES"
    modifier.limit_method = "ANGLE"
    modifier.angle_limit = math.radians(30.0)
    modifier.offset_type = "OFFSET"
    modifier.use_clamp_overlap = True
    modifier.profile = 0.5
    modifier.show_render = True

    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    result = bpy.ops.object.modifier_apply(modifier=modifier.name)
    _require_finished(result, f"apply bevel {obj.name}")
    obj.select_set(False)
    bpy.context.view_layer.objects.active = None


def _create_mesh_object(
    name: str,
    collection: bpy.types.Collection,
    vertices: Sequence[Sequence[float] | Vector],
    faces: Sequence[Sequence[int]],
    material: bpy.types.Material,
    *,
    bevel_width: float = 0.0,
) -> bpy.types.Object:
    """Create and link one deterministic mesh object with identity transforms."""

    mesh = bpy.data.meshes.new(f"{name}_Mesh")
    mesh.from_pydata([tuple(vertex) for vertex in vertices], [], faces)
    mesh.materials.append(material)
    mesh.validate(verbose=False, clean_customdata=False)
    mesh.update(calc_edges=True)
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    if bevel_width > 0.0:
        _apply_small_bevel(obj, bevel_width)
    return obj


def create_tapered_box(
    name: str,
    collection: bpy.types.Collection,
    *,
    bottom_center: Sequence[float],
    bottom_size: Sequence[float],
    top_center: Sequence[float],
    top_size: Sequence[float],
    z_min: float,
    z_max: float,
    material: bpy.types.Material,
) -> bpy.types.Object:
    """Create a closed six-quad box with independently shifted top and bottom."""

    if z_max <= z_min:
        raise ValueError(f"{name}: z_max must exceed z_min")
    if min(*bottom_size, *top_size) <= 0.0:
        raise ValueError(f"{name}: tapered-box dimensions must be positive")

    bx, by = (float(value) for value in bottom_center)
    tx, ty = (float(value) for value in top_center)
    bottom_half_x, bottom_half_y = (float(value) * 0.5 for value in bottom_size)
    top_half_x, top_half_y = (float(value) * 0.5 for value in top_size)
    vertices = (
        (bx - bottom_half_x, by - bottom_half_y, z_min),
        (bx + bottom_half_x, by - bottom_half_y, z_min),
        (bx + bottom_half_x, by + bottom_half_y, z_min),
        (bx - bottom_half_x, by + bottom_half_y, z_min),
        (tx - top_half_x, ty - top_half_y, z_max),
        (tx + top_half_x, ty - top_half_y, z_max),
        (tx + top_half_x, ty + top_half_y, z_max),
        (tx - top_half_x, ty + top_half_y, z_max),
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
        name,
        collection,
        vertices,
        faces,
        material,
        bevel_width=SMALL_BEVEL_WIDTH_M,
    )


def create_beam_between(
    name: str,
    collection: bpy.types.Collection,
    start: Sequence[float],
    end: Sequence[float],
    *,
    width: float,
    depth: float,
    material: bpy.types.Material,
) -> bpy.types.Object:
    """Create a closed rectangular beam whose long axis joins two points."""

    start_vector = Vector(start)
    end_vector = Vector(end)
    axis = end_vector - start_vector
    if axis.length <= 1.0e-8:
        raise ValueError(f"{name}: beam endpoints must differ")
    if width <= 0.0 or depth <= 0.0:
        raise ValueError(f"{name}: beam cross-section must be positive")
    axis.normalize()

    reference = Vector((0.0, 0.0, 1.0))
    if abs(axis.dot(reference)) > 0.92:
        reference = Vector((0.0, 1.0, 0.0))
    side = axis.cross(reference).normalized()
    other = side.cross(axis).normalized()
    half_width = width * 0.5
    half_depth = depth * 0.5
    offsets = (
        -side * half_width - other * half_depth,
        side * half_width - other * half_depth,
        side * half_width + other * half_depth,
        -side * half_width + other * half_depth,
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
        name,
        collection,
        vertices,
        faces,
        material,
        bevel_width=SMALL_BEVEL_WIDTH_M,
    )


def create_curved_roof(
    name: str,
    collection: bpy.types.Collection,
    *,
    ridge_offset_m: float,
    material: bpy.types.Material,
    x_samples: int = 151,
    y_samples: int = 101,
) -> bpy.types.Object:
    """Create one closed, high-resolution, tile-free xieshan-inspired roof shell.

    The 151-by-101 top grid plus its bottom and perimeter closes into exactly
    30,500 quads.  Its silhouette, rather than hidden padding, carries the Gate
    1 topology budget.
    """

    if x_samples < 3 or y_samples < 3:
        raise ValueError("roof grid requires at least three samples per axis")

    half_x = 2.4
    half_y = 2.8
    thickness = 0.16
    raw_heights: list[list[float]] = []
    coordinates: list[list[tuple[float, float]]] = []
    for y_index in range(y_samples):
        v = -1.0 + (2.0 * y_index / (y_samples - 1))
        y = half_y * v
        height_row: list[float] = []
        coordinate_row: list[tuple[float, float]] = []
        for x_index in range(x_samples):
            u = -1.0 + (2.0 * x_index / (x_samples - 1))
            x = half_x * u

            ridge_x = ridge_offset_m + 0.04 * (1.0 - v * v) + 0.035 * v
            ridge_u = ridge_x / half_x
            span = 1.0 + ridge_u if u <= ridge_u else 1.0 - ridge_u
            across = abs(u - ridge_u) / span
            hip = max(0.0, (abs(v) - 0.48) / 0.52)
            main_drop = max(across, 0.92 * hip)

            x_edge = across**8
            y_edge = max(0.0, (abs(v) - 0.70) / 0.30) ** 3
            front_back_lift = (0.19 if v >= 0.0 else 0.10) * y_edge
            lateral_lift = (0.14 if u <= ridge_u else 0.065) * x_edge
            upturn = lateral_lift + front_back_lift + 0.08 * x_edge * y_edge
            warp = 0.045 * math.sin(math.pi * (u + 0.15)) * math.cos(
                math.pi * v * 0.5
            )
            warp += 0.035 * u * v
            ridge_bow = 0.035 * math.cos(math.pi * min(abs(v), 0.48) / 0.96)
            height = 5.00 + ridge_bow - 1.24 * main_drop + upturn + warp
            height_row.append(height)
            coordinate_row.append((x, y))
        raw_heights.append(height_row)
        coordinates.append(coordinate_row)

    raw_max = max(max(row) for row in raw_heights)
    height_adjustment = 5.08 - raw_max
    top_heights = [
        [height + height_adjustment for height in row] for row in raw_heights
    ]

    layer_size = x_samples * y_samples

    def vertex_index(x_index: int, y_index: int, *, bottom: bool = False) -> int:
        index = y_index * x_samples + x_index
        return index + layer_size if bottom else index

    vertices: list[tuple[float, float, float]] = []
    for y_index in range(y_samples):
        for x_index in range(x_samples):
            x, y = coordinates[y_index][x_index]
            vertices.append((x, y, top_heights[y_index][x_index]))
    for y_index in range(y_samples):
        for x_index in range(x_samples):
            x, y = coordinates[y_index][x_index]
            vertices.append((x, y, top_heights[y_index][x_index] - thickness))

    faces: list[tuple[int, int, int, int]] = []
    for y_index in range(y_samples - 1):
        for x_index in range(x_samples - 1):
            top_00 = vertex_index(x_index, y_index)
            top_10 = vertex_index(x_index + 1, y_index)
            top_11 = vertex_index(x_index + 1, y_index + 1)
            top_01 = vertex_index(x_index, y_index + 1)
            faces.append((top_00, top_10, top_11, top_01))

            bottom_00 = vertex_index(x_index, y_index, bottom=True)
            bottom_10 = vertex_index(x_index + 1, y_index, bottom=True)
            bottom_11 = vertex_index(x_index + 1, y_index + 1, bottom=True)
            bottom_01 = vertex_index(x_index, y_index + 1, bottom=True)
            faces.append((bottom_01, bottom_11, bottom_10, bottom_00))

    last_x = x_samples - 1
    last_y = y_samples - 1
    for x_index in range(x_samples - 1):
        faces.append(
            (
                vertex_index(x_index, 0),
                vertex_index(x_index, 0, bottom=True),
                vertex_index(x_index + 1, 0, bottom=True),
                vertex_index(x_index + 1, 0),
            )
        )
        faces.append(
            (
                vertex_index(x_index, last_y),
                vertex_index(x_index + 1, last_y),
                vertex_index(x_index + 1, last_y, bottom=True),
                vertex_index(x_index, last_y, bottom=True),
            )
        )
    for y_index in range(y_samples - 1):
        faces.append(
            (
                vertex_index(0, y_index),
                vertex_index(0, y_index + 1),
                vertex_index(0, y_index + 1, bottom=True),
                vertex_index(0, y_index, bottom=True),
            )
        )
        faces.append(
            (
                vertex_index(last_x, y_index + 1),
                vertex_index(last_x, y_index),
                vertex_index(last_x, y_index, bottom=True),
                vertex_index(last_x, y_index + 1, bottom=True),
            )
        )

    roof = _create_mesh_object(name, collection, vertices, faces, material)
    surface_face_count = 2 * (x_samples - 1) * (y_samples - 1)
    for polygon in roof.data.polygons[:surface_face_count]:
        polygon.use_smooth = True
    return roof


def _create_curved_ridge(
    collection: bpy.types.Collection, material: bpy.types.Material
) -> bpy.types.Object:
    """Create one broad, off-centre ridge stroke with a gentle plan-view bow."""

    stations = (
        (-0.34, -1.48, 5.07),
        (-0.28, -0.88, 5.09),
        (-0.21, -0.30, 5.11),
        (-0.18, 0.28, 5.12),
        (-0.24, 0.86, 5.10),
        (-0.32, 1.43, 5.08),
    )
    half_width = 0.10
    half_height = 0.08
    vertices: list[tuple[float, float, float]] = []
    for x, y, z in stations:
        vertices.extend(
            (
                (x - half_width, y, z - half_height),
                (x + half_width, y, z - half_height),
                (x + half_width, y, z + half_height),
                (x - half_width, y, z + half_height),
            )
        )
    faces: list[tuple[int, int, int, int]] = [(1, 2, 3, 0)]
    for index in range(len(stations) - 1):
        current = index * 4
        following = (index + 1) * 4
        faces.extend(
            (
                (following, following + 1, current + 1, current),
                (following + 1, following + 2, current + 2, current + 1),
                (following + 2, following + 3, current + 3, current + 2),
                (following + 3, following, current, current + 3),
            )
        )
    last = (len(stations) - 1) * 4
    faces.append((last + 3, last + 2, last + 1, last))
    return _create_mesh_object(
        "Roof_Ridge_Bowed",
        collection,
        vertices,
        faces,
        material,
        bevel_width=SMALL_BEVEL_WIDTH_M,
    )


def _create_closed_window_pane(
    name: str,
    collection: bpy.types.Collection,
    corners: Sequence[Vector],
    *,
    thickness: float,
    material: bpy.types.Material,
) -> bpy.types.Object:
    """Create one thin, closed, shadeless-free paper pane behind a timber frame."""

    if len(corners) != 4 or thickness <= 0.0:
        raise ValueError(f"{name}: pane requires four corners and positive thickness")
    horizontal = corners[1] - corners[0]
    vertical = corners[3] - corners[0]
    normal = horizontal.cross(vertical)
    if normal.length <= 1.0e-8:
        raise ValueError(f"{name}: pane corners must span a plane")
    normal.normalize()
    half_thickness = thickness * 0.5
    vertices = tuple(corner + normal * half_thickness for corner in corners) + tuple(
        corner - normal * half_thickness for corner in corners
    )
    faces = (
        (0, 1, 2, 3),
        (7, 6, 5, 4),
        (0, 4, 5, 1),
        (1, 5, 6, 2),
        (2, 6, 7, 3),
        (3, 7, 4, 0),
    )
    return _create_mesh_object(name, collection, vertices, faces, material)


def create_chunky_window(
    name: str,
    collection: bpy.types.Collection,
    *,
    center: Sequence[float],
    horizontal: Sequence[float],
    width: float,
    height: float,
    frame_width: float,
    depth: float,
    divisions: int,
    material: bpy.types.Material,
    pane_material: bpy.types.Material,
) -> list[bpy.types.Object]:
    """Create one thick mildly trapezoidal frame with broad panel divisions."""

    if divisions != 2:
        raise ValueError("golden inn windows require exactly two broad divisions")
    horizontal_vector = Vector(horizontal).normalized()
    center_vector = Vector(center)
    lower_center = center_vector - Vector((0.0, 0.0, height * 0.5))
    upper_center = center_vector + Vector((0.0, 0.0, height * 0.5))
    upper_center += horizontal_vector * (width * 0.035)
    lower_width = width
    upper_width = width * 0.925

    lower_left = lower_center - horizontal_vector * (lower_width * 0.5)
    lower_right = lower_center + horizontal_vector * (lower_width * 0.5)
    upper_left = upper_center - horizontal_vector * (upper_width * 0.5)
    upper_right = upper_center + horizontal_vector * (upper_width * 0.5)
    objects = [
        create_beam_between(
            f"{name}_Post_L",
            collection,
            lower_left,
            upper_left,
            width=frame_width,
            depth=depth,
            material=material,
        ),
        create_beam_between(
            f"{name}_Post_R",
            collection,
            lower_right,
            upper_right,
            width=frame_width,
            depth=depth,
            material=material,
        ),
        create_beam_between(
            f"{name}_Rail_Bottom",
            collection,
            lower_left,
            lower_right,
            width=frame_width,
            depth=depth,
            material=material,
        ),
        create_beam_between(
            f"{name}_Rail_Top",
            collection,
            upper_left,
            upper_right,
            width=frame_width,
            depth=depth,
            material=material,
        ),
    ]
    for division_index in range(1, divisions):
        fraction = division_index / divisions
        lower_divider = lower_left.lerp(lower_right, fraction)
        upper_divider = upper_left.lerp(upper_right, fraction)
        objects.append(
            create_beam_between(
                f"{name}_Division_{division_index:02d}",
                collection,
                lower_divider,
                upper_divider,
                width=frame_width * 0.9,
                depth=depth,
                material=material,
            )
        )
    vertical_inset = Vector((0.0, 0.0, frame_width * 0.58))
    horizontal_inset = horizontal_vector * (frame_width * 0.58)
    for pane_index in range(divisions):
        start_fraction = pane_index / divisions
        end_fraction = (pane_index + 1) / divisions
        lower_start = lower_left.lerp(lower_right, start_fraction) + horizontal_inset
        lower_end = lower_left.lerp(lower_right, end_fraction) - horizontal_inset
        upper_start = upper_left.lerp(upper_right, start_fraction) + horizontal_inset
        upper_end = upper_left.lerp(upper_right, end_fraction) - horizontal_inset
        objects.append(
            _create_closed_window_pane(
                f"{name}_Pane_{pane_index + 1:02d}",
                collection,
                (
                    lower_start + vertical_inset,
                    lower_end + vertical_inset,
                    upper_end - vertical_inset,
                    upper_start - vertical_inset,
                ),
                thickness=min(depth * 0.2, 0.035),
                material=pane_material,
            )
        )
    return objects


def create_single_capital(
    name: str,
    collection: bpy.types.Collection,
    *,
    center: Sequence[float],
    material: bpy.types.Material,
) -> bpy.types.Object:
    """Create exactly one oversized tapered capital layer for a main column."""

    x, y, z = (float(value) for value in center)
    return create_tapered_box(
        name,
        collection,
        bottom_center=(x, y),
        bottom_size=(0.64, 0.56),
        top_center=(x + 0.02, y - 0.015),
        top_size=(LARGE_FORM_CONTRACT["capital_top_width_m"], 0.78),
        z_min=z - 0.15,
        z_max=z + 0.15,
        material=material,
    )


def _make_material(
    name: str, color: Sequence[float], roughness: float
) -> bpy.types.Material:
    material = bpy.data.materials.new(name)
    material.use_nodes = True
    material.diffuse_color = (*color[:3], 1.0)
    principled = material.node_tree.nodes.get("Principled BSDF")
    if principled is not None:
        base_color = principled.inputs.get("Base Color")
        if base_color is not None:
            base_color.default_value = (*color[:3], 1.0)
        roughness_input = principled.inputs.get("Roughness")
        if roughness_input is not None:
            roughness_input.default_value = roughness
        metallic_input = principled.inputs.get("Metallic")
        if metallic_input is not None:
            metallic_input.default_value = 0.0
        specular_input = principled.inputs.get("Specular IOR Level")
        if specular_input is not None:
            specular_input.default_value = 0.22
    return material


def _make_emission_material(
    name: str, color: Sequence[float]
) -> bpy.types.Material:
    """Create a direct-emission material for a lighting-independent mask."""

    material = bpy.data.materials.new(name)
    material.use_nodes = True
    material.diffuse_color = (*color[:3], 1.0)
    nodes = material.node_tree.nodes
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    emission = nodes.new("ShaderNodeEmission")
    emission.inputs["Color"].default_value = (*color[:3], 1.0)
    emission.inputs["Strength"].default_value = 1.0
    material.node_tree.links.new(emission.outputs["Emission"], output.inputs["Surface"])
    return material


def _create_materials() -> dict[str, bpy.types.Material]:
    return {
        "plaster": _make_material("MAT_Warm_Plaster", (0.72, 0.54, 0.34), 0.88),
        "timber": _make_material("MAT_Dark_Timber", (0.075, 0.050, 0.037), 0.84),
        "roof": _make_material("MAT_Terracotta_Roof", (0.42, 0.105, 0.055), 0.86),
        "stone": _make_material("MAT_Gray_Stone", (0.25, 0.27, 0.25), 0.93),
        "paper": _make_material("MAT_XuanPaperWarm", (0.86, 0.66, 0.30), 0.96),
    }


def _create_body(
    collection: bpy.types.Collection, materials: dict[str, bpy.types.Material]
) -> None:
    body_width = LARGE_FORM_CONTRACT["body_width_m"]
    storeys = (
        {
            "name": "Body_Ground_Storey",
            "bottom_center": (-0.08, -0.02),
            "bottom_size": (body_width + 0.18, 4.72),
            "top_center": (0.00, -0.05),
            "top_size": (body_width + 0.02, 4.60),
            "z_min": 0.28,
            "z_max": LARGE_FORM_CONTRACT["ground_floor_top_z_m"],
        },
        {
            "name": "Body_Upper_Storey",
            "bottom_center": (0.04, -0.08),
            "bottom_size": (body_width + 0.02, 4.58),
            "top_center": (LARGE_FORM_CONTRACT["upper_floor_shift_x_m"], -0.16),
            "top_size": (body_width - 0.25, 4.30),
            "z_min": 1.90,
            "z_max": LARGE_FORM_CONTRACT["upper_floor_top_z_m"],
        },
    )
    if len(storeys) != LARGE_FORM_CONTRACT["storeys"]:
        raise RuntimeError("large-form storey count drifted from deterministic contract")
    for storey in storeys:
        create_tapered_box(
            storey["name"],
            collection,
            bottom_center=storey["bottom_center"],
            bottom_size=storey["bottom_size"],
            top_center=storey["top_center"],
            top_size=storey["top_size"],
            z_min=storey["z_min"],
            z_max=storey["z_max"],
            material=materials["plaster"],
        )

    stone_blocks = (
        ("Stone_Base_Front_01", (-1.54, 2.18), (1.00, 0.48), (-1.50, 2.16), (0.91, 0.43), 0.34),
        ("Stone_Base_Front_02", (-0.43, 2.20), (1.10, 0.44), (-0.47, 2.19), (1.00, 0.40), 0.29),
        ("Stone_Base_Front_03", (0.76, 2.17), (1.18, 0.50), (0.72, 2.15), (1.07, 0.43), 0.37),
        ("Stone_Base_Front_04", (1.73, 2.15), (0.72, 0.46), (1.69, 2.14), (0.65, 0.40), 0.31),
        ("Stone_Base_Back_01", (-1.20, -2.20), (1.48, 0.42), (-1.16, -2.18), (1.34, 0.38), 0.30),
        ("Stone_Base_Back_02", (0.72, -2.18), (1.70, 0.48), (0.67, -2.16), (1.57, 0.41), 0.36),
        ("Stone_Base_Left", (-1.98, -0.55), (0.46, 1.44), (-1.96, -0.50), (0.40, 1.31), 0.33),
        ("Stone_Base_Right", (2.00, 0.42), (0.44, 1.62), (1.97, 0.37), (0.39, 1.48), 0.35),
    )
    for name, bottom_center, bottom_size, top_center, top_size, height in stone_blocks:
        create_tapered_box(
            name,
            collection,
            bottom_center=bottom_center,
            bottom_size=bottom_size,
            top_center=top_center,
            top_size=top_size,
            z_min=0.0,
            z_max=height,
            material=materials["stone"],
        )


def _create_door(
    collection: bpy.types.Collection,
    materials: dict[str, bpy.types.Material],
    facade_offset_m: float,
) -> None:
    panel_gap = 0.03
    panel_width = (LARGE_FORM_CONTRACT["door_width_m"] - panel_gap) * 0.5
    door_top_z = 0.28 + LARGE_FORM_CONTRACT["door_height_m"]
    for suffix, direction in (("L", -1.0), ("R", 1.0)):
        center_x = facade_offset_m + direction * (panel_width + panel_gap) * 0.5
        create_tapered_box(
            f"Door_Double_{suffix}",
            collection,
            bottom_center=(center_x, 2.34),
            bottom_size=(panel_width, 0.16),
            top_center=(center_x + direction * 0.016, 2.33),
            top_size=(panel_width * 0.94, 0.16),
            z_min=0.28,
            z_max=door_top_z,
            material=materials["timber"],
        )


def _create_windows(
    collection: bpy.types.Collection,
    materials: dict[str, bpy.types.Material],
    divisions: int,
) -> None:
    if divisions != LARGE_FORM_CONTRACT["window_divisions"]:
        raise ValueError("large-form window divisions differ from deterministic contract")
    minimum_width = LARGE_FORM_CONTRACT["min_window_width_m"]
    specifications = (
        (
            "Window_Front_Ground",
            (-1.32, 2.34, 1.14),
            (1.0, 0.0, 0.0),
            minimum_width,
            0.82,
        ),
        (
            "Window_Front_Upper",
            (-0.62, 2.17, 2.70),
            (1.0, 0.0, 0.0),
            minimum_width + 0.25,
            0.90,
        ),
        (
            "Window_Right_Upper",
            (2.14, 0.42, 2.62),
            (0.0, 1.0, 0.0),
            minimum_width + 0.12,
            0.92,
        ),
    )
    if len(specifications) != LARGE_FORM_CONTRACT["window_count"]:
        raise RuntimeError("large-form window count drifted from deterministic contract")
    for name, center, horizontal, width, height in specifications:
        create_chunky_window(
            name,
            collection,
            center=center,
            horizontal=horizontal,
            width=width,
            height=height,
            frame_width=0.13,
            depth=0.18,
            divisions=divisions,
            material=materials["timber"],
            pane_material=materials["paper"],
        )


def _column_top(column: dict, index: int) -> Vector:
    start_z = 0.32
    end_z = 3.47
    directions = (
        Vector((-0.96, 0.28, 0.0)),
        Vector((-0.80, -0.60, 0.0)),
        Vector((-0.55, -0.84, 0.0)),
        Vector((-0.62, 0.78, 0.0)),
    )
    direction = directions[index].normalized()
    horizontal_offset = math.tan(math.radians(float(column["lean_deg"]))) * (
        end_z - start_z
    )
    return Vector((float(column["x_m"]), float(column["y_m"]), end_z)) + (
        direction * horizontal_offset
    )


def _create_eave_structure(
    collection: bpy.types.Collection,
    materials: dict[str, bpy.types.Material],
    columns: Iterable[dict],
) -> None:
    timber = materials["timber"]
    column_data = list(columns)
    for index, column in enumerate(column_data):
        start = Vector((float(column["x_m"]), float(column["y_m"]), 0.32))
        end = _column_top(column, index)
        create_beam_between(
            str(column["name"]),
            collection,
            start,
            end,
            width=0.25,
            depth=0.27,
            material=timber,
        )
        create_single_capital(
            f"Capital_{index + 1:02d}",
            collection,
            center=(end.x, end.y, 3.33),
            material=timber,
        )

    supports = (
        ("Eave_Beam_Front", (-2.18, 2.40, 3.59), (2.10, 2.36, 3.56), 0.24, 0.25),
        ("Eave_Beam_Back", (-2.15, -2.34, 3.56), (2.10, -2.39, 3.53), 0.21, 0.23),
        ("Eave_Beam_Left", (-2.07, -2.40, 3.57), (-2.11, 2.45, 3.59), 0.21, 0.23),
        ("Eave_Beam_Right", (2.04, -2.38, 3.54), (2.09, 2.43, 3.57), 0.21, 0.23),
        (
            "Balcony_Ledge_Front",
            (-2.02, 2.37, LARGE_FORM_CONTRACT["balcony_band_z_m"]),
            (1.90, 2.43, LARGE_FORM_CONTRACT["balcony_band_z_m"] + 0.02),
            0.40,
            0.18,
        ),
        (
            "Balcony_Band_Right",
            (2.06, -2.10, LARGE_FORM_CONTRACT["balcony_band_z_m"] - 0.01),
            (2.10, 2.30, LARGE_FORM_CONTRACT["balcony_band_z_m"] + 0.01),
            0.18,
            0.18,
        ),
    )
    for name, start, end, width, depth in supports:
        create_beam_between(
            name,
            collection,
            start,
            end,
            width=width,
            depth=depth,
            material=timber,
        )


def _point_camera(
    obj: bpy.types.Object, target: Sequence[float]
) -> None:
    target_vector = Vector(target)
    direction = target_vector - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    obj["target_m"] = list(target_vector)


def create_fixed_cameras(scene: bpy.types.Scene) -> dict[str, bpy.types.Object]:
    """Create the three exact Gate 1 cameras with deterministic aim targets."""

    specifications = {
        "hero_3q": ((8.4, 9.6, 7.1), (0.0, 0.0, 2.5), 52.0),
        "front": ((0.0, 11.8, 3.2), (0.0, 0.0, 2.4), 58.0),
        "player_camera": ((7.2, 10.5, 8.8), (0.0, 0.0, 2.1), 48.0),
    }
    cameras: dict[str, bpy.types.Object] = {}
    for view_name, (location, target, lens) in specifications.items():
        camera_data = bpy.data.cameras.new(f"CAM_{view_name}_Data")
        camera_data.lens = lens
        camera_data.sensor_width = 52.0
        camera_data.clip_start = 0.05
        camera_data.clip_end = 100.0
        camera = bpy.data.objects.new(f"CAM_{view_name}", camera_data)
        scene.collection.objects.link(camera)
        camera.location = location
        _point_camera(camera, target)
        camera["view_name"] = view_name
        cameras[view_name] = camera
    scene.camera = cameras["hero_3q"]
    return cameras


def validate_camera_framing(
    scene: bpy.types.Scene,
    cameras: dict[str, bpy.types.Object],
    *,
    margin: float = 0.04,
) -> None:
    """Fail before rendering if a fixed view clips any visible mesh vertex."""

    bpy.context.view_layer.update()
    world_vertices = [
        obj.matrix_world @ vertex.co
        for obj in scene.objects
        if obj.type == "MESH"
        for vertex in obj.data.vertices
    ]
    if not world_vertices:
        raise RuntimeError("cannot validate camera framing without mesh vertices")

    for view_name in REQUIRED_VIEWS:
        camera = cameras[view_name]
        projected = [
            world_to_camera_view(scene, camera, vertex) for vertex in world_vertices
        ]
        if any(point.z <= 0.0 for point in projected):
            raise RuntimeError(f"{view_name} has building vertices behind the camera")
        minimum_x = min(point.x for point in projected)
        maximum_x = max(point.x for point in projected)
        minimum_y = min(point.y for point in projected)
        maximum_y = max(point.y for point in projected)
        camera["framing_ndc"] = [minimum_x, maximum_x, minimum_y, maximum_y]
        if (
            minimum_x < margin
            or maximum_x > 1.0 - margin
            or minimum_y < margin
            or maximum_y > 1.0 - margin
        ):
            raise RuntimeError(
                f"{view_name} clips the golden inn: "
                f"x=[{minimum_x:.4f}, {maximum_x:.4f}], "
                f"y=[{minimum_y:.4f}, {maximum_y:.4f}]"
            )


def _create_light(
    scene: bpy.types.Scene,
    name: str,
    light_type: str,
    *,
    energy: float,
    location: Sequence[float],
    target: Sequence[float] | None = None,
    size: float | None = None,
) -> bpy.types.Object:
    light_data = bpy.data.lights.new(f"{name}_Data", light_type)
    light_data.energy = energy
    light_data.use_shadow = True
    if size is not None and hasattr(light_data, "shape"):
        light_data.shape = "DISK"
        light_data.size = size
    light = bpy.data.objects.new(name, light_data)
    scene.collection.objects.link(light)
    light.location = location
    if target is not None:
        _point_camera(light, target)
    return light


def _configure_scene(scene: bpy.types.Scene) -> None:
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    bpy.context.preferences.filepaths.save_version = 0
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = MAIN_RESOLUTION[0]
    scene.render.resolution_y = MAIN_RESOLUTION[1]
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.render.image_settings.compression = 15
    scene.render.film_transparent = False
    scene.render.use_file_extension = True
    scene.render.use_freestyle = True
    scene.render.line_thickness_mode = "ABSOLUTE"
    scene.render.line_thickness = 1.0
    view_layer = bpy.context.view_layer
    view_layer.use_freestyle = True
    freestyle = view_layer.freestyle_settings
    freestyle.mode = "EDITOR"
    freestyle.as_render_pass = False
    freestyle.crease_angle = math.radians(135.0)
    lineset = (
        freestyle.linesets[0]
        if len(freestyle.linesets) > 0
        else freestyle.linesets.new("Ink_Contours")
    )
    lineset.name = "Ink_Contours"
    lineset.show_render = True
    lineset.select_by_edge_types = True
    lineset.select_by_visibility = True
    lineset.visibility = "VISIBLE"
    lineset.select_silhouette = True
    lineset.select_border = True
    lineset.select_crease = True
    lineset.select_external_contour = True
    lineset.select_material_boundary = False
    lineset.select_edge_mark = False
    linestyle = lineset.linestyle
    if linestyle is None:
        linestyle = bpy.data.linestyles.new("Ink_Contours_Style")
        lineset.linestyle = linestyle
    linestyle.color = (0.025, 0.018, 0.012)
    linestyle.alpha = 1.0
    linestyle.thickness = 3.0
    linestyle.use_chaining = True
    linestyle.chaining = "PLAIN"
    linestyle.caps = "BUTT"
    scene.view_settings.view_transform = "AgX"
    try:
        scene.view_settings.look = "AgX - Medium High Contrast"
    except TypeError:
        pass

    world = bpy.data.worlds.new("World_Warm_OffWhite")
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    if background is not None:
        background.inputs["Color"].default_value = (0.78, 0.68, 0.54, 1.0)
        background.inputs["Strength"].default_value = 0.65
    world.color = (0.78, 0.68, 0.54)
    scene.world = world

    sun = _create_light(
        scene,
        "LIGHT_Sun_Key",
        "SUN",
        energy=2.4,
        location=(4.0, 5.0, 9.0),
    )
    sun.rotation_euler = (math.radians(28.0), math.radians(-22.0), math.radians(-32.0))
    if hasattr(sun.data, "angle"):
        sun.data.angle = math.radians(14.0)
    _create_light(
        scene,
        "LIGHT_Area_Fill",
        "AREA",
        energy=700.0,
        location=(-5.4, 3.0, 6.8),
        target=(0.0, 0.0, 2.4),
        size=5.0,
    )
    _create_light(
        scene,
        "LIGHT_Area_Rim",
        "AREA",
        energy=520.0,
        location=(4.0, -5.5, 7.5),
        target=(0.0, 0.0, 3.0),
        size=4.0,
    )


def _render_view(
    scene: bpy.types.Scene, camera: bpy.types.Object, output_path: Path
) -> None:
    scene.camera = camera
    scene.render.filepath = str(output_path)
    result = bpy.ops.render.render(write_still=True)
    _require_finished(result, f"render {camera.name}")
    if not output_path.is_file() or output_path.stat().st_size <= 0:
        raise RuntimeError(f"Blender did not write render: {output_path}")


def _render_silhouette(
    scene: bpy.types.Scene,
    camera: bpy.types.Object,
    output_path: Path,
) -> None:
    view_layer = bpy.context.view_layer
    saved_camera = scene.camera
    saved_filepath = scene.render.filepath
    saved_resolution = (
        scene.render.resolution_x,
        scene.render.resolution_y,
        scene.render.resolution_percentage,
    )
    saved_film_transparent = scene.render.film_transparent
    saved_freestyle = (
        scene.render.use_freestyle,
        [(layer, layer.use_freestyle) for layer in scene.view_layers],
    )
    saved_world = scene.world
    mesh_objects = sorted(
        (obj for obj in scene.objects if obj.type == "MESH"), key=lambda obj: obj.name
    )
    saved_materials = {obj.name: list(obj.data.materials) for obj in mesh_objects}
    saved_material_override = view_layer.material_override
    saved_view_settings = {
        "display_device": scene.display_settings.display_device,
        "view_transform": scene.view_settings.view_transform,
        "look": scene.view_settings.look,
        "exposure": scene.view_settings.exposure,
        "gamma": scene.view_settings.gamma,
        "use_curve_mapping": scene.view_settings.use_curve_mapping,
        "use_hdr_view": scene.view_settings.use_hdr_view,
        "dither_intensity": scene.render.dither_intensity,
        "use_compositing": scene.render.use_compositing,
        "color_management": scene.render.image_settings.color_management,
        "file_format": scene.render.image_settings.file_format,
        "color_mode": scene.render.image_settings.color_mode,
        "color_depth": scene.render.image_settings.color_depth,
    }

    silhouette = _make_emission_material(
        "MAT_Silhouette_Temporary", (0.0, 0.0, 0.0, 1.0)
    )
    silhouette_world = bpy.data.worlds.new("World_Silhouette_White_Temporary")
    silhouette_world.use_nodes = True
    world_nodes = silhouette_world.node_tree.nodes
    world_nodes.clear()
    world_output = world_nodes.new("ShaderNodeOutputWorld")
    white_background = world_nodes.new("ShaderNodeBackground")
    white_background.inputs["Color"].default_value = (1.0, 1.0, 1.0, 1.0)
    white_background.inputs["Strength"].default_value = 1.0
    silhouette_world.node_tree.links.new(
        white_background.outputs["Background"], world_output.inputs["Surface"]
    )
    silhouette_world.color = (1.0, 1.0, 1.0)

    try:
        scene.world = silhouette_world
        view_layer.material_override = None
        for obj in mesh_objects:
            obj.data.materials.clear()
            obj.data.materials.append(silhouette)
        scene.render.resolution_x = SILHOUETTE_RESOLUTION[0]
        scene.render.resolution_y = SILHOUETTE_RESOLUTION[1]
        scene.render.resolution_percentage = 100
        scene.render.film_transparent = False
        scene.render.use_freestyle = False
        for layer in scene.view_layers:
            layer.use_freestyle = False
        scene.display_settings.display_device = "sRGB"
        scene.view_settings.view_transform = "Standard"
        scene.view_settings.look = "None"
        scene.view_settings.exposure = 0.0
        scene.view_settings.gamma = 1.0
        scene.view_settings.use_curve_mapping = False
        scene.view_settings.use_hdr_view = False
        scene.render.dither_intensity = 0.0
        scene.render.use_compositing = False
        scene.render.image_settings.color_management = "FOLLOW_SCENE"
        scene.render.image_settings.file_format = "PNG"
        scene.render.image_settings.color_mode = "RGBA"
        scene.render.image_settings.color_depth = "8"
        _render_view(scene, camera, output_path)
    finally:
        for obj in mesh_objects:
            obj.data.materials.clear()
            for material in saved_materials[obj.name]:
                obj.data.materials.append(material)
        view_layer.material_override = saved_material_override
        scene.world = saved_world
        scene.camera = saved_camera
        scene.render.filepath = saved_filepath
        scene.render.resolution_x = saved_resolution[0]
        scene.render.resolution_y = saved_resolution[1]
        scene.render.resolution_percentage = saved_resolution[2]
        scene.render.film_transparent = saved_film_transparent
        scene.render.use_freestyle = saved_freestyle[0]
        for layer, use_freestyle in saved_freestyle[1]:
            layer.use_freestyle = use_freestyle
        scene.display_settings.display_device = saved_view_settings["display_device"]
        scene.view_settings.view_transform = saved_view_settings["view_transform"]
        scene.view_settings.look = saved_view_settings["look"]
        scene.view_settings.exposure = saved_view_settings["exposure"]
        scene.view_settings.gamma = saved_view_settings["gamma"]
        scene.view_settings.use_curve_mapping = saved_view_settings["use_curve_mapping"]
        scene.view_settings.use_hdr_view = saved_view_settings["use_hdr_view"]
        scene.render.dither_intensity = saved_view_settings["dither_intensity"]
        scene.render.use_compositing = saved_view_settings["use_compositing"]
        scene.render.image_settings.color_management = saved_view_settings[
            "color_management"
        ]
        scene.render.image_settings.file_format = saved_view_settings["file_format"]
        scene.render.image_settings.color_mode = saved_view_settings["color_mode"]
        scene.render.image_settings.color_depth = saved_view_settings["color_depth"]
        bpy.data.materials.remove(silhouette)
        bpy.data.worlds.remove(silhouette_world)


def _validate_silhouette_pixels(
    pixels: Sequence[float], width: int, height: int
) -> dict[str, int]:
    """Require an opaque neutral mask, white border, and nontrivial black form."""

    if width < 3 or height < 3:
        raise RuntimeError("silhouette self-test requires at least a 3x3 image")
    expected_components = width * height * 4
    if len(pixels) != expected_components:
        raise RuntimeError(
            "silhouette self-test pixel count mismatch: "
            f"expected {expected_components}, got {len(pixels)}"
        )

    tolerance = 1.0e-6
    rgba_pixels = [
        tuple(float(pixels[index + offset]) for offset in range(4))
        for index in range(0, len(pixels), 4)
    ]
    for red, green, blue, alpha in rgba_pixels:
        if abs(alpha - 1.0) > tolerance:
            raise RuntimeError("silhouette self-test contains non-opaque pixels")
        if max(red, green, blue) - min(red, green, blue) > tolerance:
            raise RuntimeError("silhouette self-test contains colored pixels")

    border_indices = {
        *(range(width)),
        *(range((height - 1) * width, height * width)),
        *(row * width for row in range(1, height - 1)),
        *(row * width + width - 1 for row in range(1, height - 1)),
    }
    if any(
        min(rgba_pixels[index][:3]) < 1.0 - tolerance for index in border_indices
    ):
        raise RuntimeError("silhouette self-test border/background is not pure white")

    pure_black = sum(
        1 for red, green, blue, _alpha in rgba_pixels if max(red, green, blue) <= tolerance
    )
    pure_white = sum(
        1
        for red, green, blue, _alpha in rgba_pixels
        if min(red, green, blue) >= 1.0 - tolerance
    )
    minimum_black = max(4, len(rgba_pixels) // 1000)
    if pure_black < minimum_black or pure_white < len(border_indices):
        raise RuntimeError(
            "silhouette self-test lacks nontrivial pure black geometry or white background"
        )
    return {"pure_black": pure_black, "pure_white": pure_white}


def _apply_mesh_transforms(scene: bpy.types.Scene) -> None:
    """Bake any accidental object transforms before the final audit."""

    bpy.ops.object.select_all(action="DESELECT")
    for obj in sorted(
        (candidate for candidate in scene.objects if candidate.type == "MESH"),
        key=lambda candidate: candidate.name,
    ):
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        obj.select_set(False)
    bpy.context.view_layer.objects.active = None


def audit_scene(
    scene: bpy.types.Scene,
    module_collections: dict[str, bpy.types.Collection],
    plan: dict,
) -> dict:
    """Audit actual final mesh data and return the verifier's exact report facts."""

    mesh_objects = sorted(
        (obj for obj in scene.objects if obj.type == "MESH"), key=lambda obj: obj.name
    )
    if not mesh_objects:
        raise RuntimeError("golden-inn scene contains no mesh objects")

    face_counts = {"triangles": 0, "quads": 0, "ngons": 0}
    non_manifold_edges = 0
    negative_scale_objects: list[str] = []
    unapplied_transform_objects: list[str] = []
    inward_normal_objects: list[str] = []
    minimum = Vector((math.inf, math.inf, math.inf))
    maximum = Vector((-math.inf, -math.inf, -math.inf))

    for obj in mesh_objects:
        for polygon in obj.data.polygons:
            if polygon.loop_total == 3:
                face_counts["triangles"] += 1
            elif polygon.loop_total == 4:
                face_counts["quads"] += 1
            else:
                face_counts["ngons"] += 1

        audit_mesh = bmesh.new()
        try:
            audit_mesh.from_mesh(obj.data)
            non_manifold_edges += sum(
                1 for edge in audit_mesh.edges if not edge.is_manifold
            )
            if audit_mesh.calc_volume(signed=True) <= 0.0:
                inward_normal_objects.append(obj.name)
        finally:
            audit_mesh.free()

        if any(component < 0.0 for component in obj.scale):
            negative_scale_objects.append(obj.name)
        if (
            any(abs(component) > IDENTITY_TOLERANCE for component in obj.location)
            or any(abs(component) > IDENTITY_TOLERANCE for component in obj.rotation_euler)
            or any(abs(component - 1.0) > IDENTITY_TOLERANCE for component in obj.scale)
        ):
            unapplied_transform_objects.append(obj.name)

        for vertex in obj.data.vertices:
            point = obj.matrix_world @ vertex.co
            for axis in range(3):
                minimum[axis] = min(minimum[axis], point[axis])
                maximum[axis] = max(maximum[axis], point[axis])

    if inward_normal_objects:
        raise RuntimeError(
            "mesh objects have inward or zero-volume winding: "
            + ", ".join(inward_normal_objects)
        )

    bounds_min = [round(float(component), 6) for component in minimum]
    bounds_max = [round(float(component), 6) for component in maximum]
    bounds_size = [
        round(upper - lower, 6) for lower, upper in zip(bounds_min, bounds_max)
    ]

    module_material_counts: dict[str, int] = {}
    for module_name in MODULE_NAMES:
        collection = module_collections[module_name]
        material_names = {
            material.name
            for obj in collection.objects
            if obj.type == "MESH"
            for material in obj.data.materials
            if material is not None
        }
        module_material_counts[module_name] = len(material_names)

    eave_meshes = [
        obj for obj in module_collections["EAVE_A"].objects if obj.type == "MESH"
    ]
    column_count = sum(obj.name.startswith("Column_") for obj in eave_meshes)
    capital_count = sum(obj.name.startswith("Capital_") for obj in eave_meshes)
    if column_count != len(plan["columns"]):
        raise RuntimeError(
            f"expected {len(plan['columns'])} main columns, found {column_count}"
        )
    measured_capital_layers, capital_remainder = divmod(capital_count, column_count)
    if capital_remainder or measured_capital_layers <= 0:
        raise RuntimeError(
            f"capital objects do not form complete column layers: {capital_count}/{column_count}"
        )
    if measured_capital_layers != int(plan["capital_layers"]):
        raise RuntimeError("measured capital layers differ from component plan")

    window_meshes = [
        obj for obj in module_collections["WINDOW_A"].objects if obj.type == "MESH"
    ]
    window_prefixes = sorted(
        obj.name.removesuffix("_Post_L")
        for obj in window_meshes
        if obj.name.endswith("_Post_L")
    )
    if len(window_prefixes) != LARGE_FORM_CONTRACT["window_count"]:
        raise RuntimeError(
            f"expected {LARGE_FORM_CONTRACT['window_count']} windows, "
            f"found {len(window_prefixes)}"
        )
    division_counts = {
        sum(
            obj.name.startswith(f"{prefix}_Division_") for obj in window_meshes
        )
        + 1
        for prefix in window_prefixes
    }
    if len(division_counts) != 1:
        raise RuntimeError("window broad-division counts are inconsistent")
    measured_window_divisions = division_counts.pop()
    if measured_window_divisions != int(plan["window_divisions"]):
        raise RuntimeError("measured window divisions differ from component plan")
    pane_counts = {
        sum(obj.name.startswith(f"{prefix}_Pane_") for obj in window_meshes)
        for prefix in window_prefixes
    }
    if pane_counts != {measured_window_divisions}:
        raise RuntimeError("warm paper pane counts must match window divisions")

    return {
        "asset_id": plan["asset_id"],
        "blender_version": [int(component) for component in bpy.app.version],
        "render_views": list(REQUIRED_VIEWS),
        "ground_min_z_m": bounds_min[2],
        "object_count": len(scene.objects),
        "mesh_object_count": len(mesh_objects),
        "faces": face_counts,
        "non_manifold_edges": non_manifold_edges,
        "negative_scale_objects": negative_scale_objects,
        "unapplied_transform_objects": unapplied_transform_objects,
        "module_material_counts": module_material_counts,
        "capital_layers": measured_capital_layers,
        "window_divisions": measured_window_divisions,
        "bounds_m": {"min": bounds_min, "max": bounds_max, "size": bounds_size},
    }


def _run_audit_mutation_self_test(
    scene: bpy.types.Scene,
    module_collections: dict[str, bpy.types.Collection],
    plan: dict,
    silhouette_path: Path,
) -> dict[str, bool]:
    """Prove measured geometry rejection and exact silhouette restoration."""

    audit_scene(scene, module_collections, plan)
    capitals = sorted(
        (
            obj
            for obj in module_collections["EAVE_A"].objects
            if obj.type == "MESH" and obj.name.startswith("Capital_")
        ),
        key=lambda obj: obj.name,
    )
    dividers = sorted(
        (
            obj
            for obj in module_collections["WINDOW_A"].objects
            if obj.type == "MESH" and "_Division_" in obj.name
        ),
        key=lambda obj: obj.name,
    )
    if not capitals or not dividers:
        raise RuntimeError("audit self-test requires capital and divider objects")

    capital = capitals[0]
    capital_name = capital.name
    capital_mutation_rejected = False
    try:
        capital.name = f"Audit_Missing_{capital_name}"
        try:
            audit_scene(scene, module_collections, plan)
        except RuntimeError as exc:
            if "capital" not in str(exc).lower():
                raise
            capital_mutation_rejected = True
        else:
            raise RuntimeError("audit accepted a renamed capital")
    finally:
        capital.name = capital_name

    divider = dividers[0]
    divider_name = divider.name
    divider_mutation_rejected = False
    try:
        divider.name = f"Audit_Missing_{divider_name}"
        try:
            audit_scene(scene, module_collections, plan)
        except RuntimeError as exc:
            if "window" not in str(exc).lower():
                raise
            divider_mutation_rejected = True
        else:
            raise RuntimeError("audit accepted a renamed window divider")
    finally:
        divider.name = divider_name

    audit_scene(scene, module_collections, plan)
    view_layer = bpy.context.view_layer

    def state_signature() -> tuple:
        mesh_materials = tuple(
            (
                obj.name,
                tuple(material.name for material in obj.data.materials),
            )
            for obj in sorted(
                (candidate for candidate in scene.objects if candidate.type == "MESH"),
                key=lambda candidate: candidate.name,
            )
        )
        return (
            scene.camera.name if scene.camera is not None else None,
            scene.render.filepath,
            scene.render.resolution_x,
            scene.render.resolution_y,
            scene.render.resolution_percentage,
            scene.render.film_transparent,
            scene.render.use_freestyle,
            tuple(layer.use_freestyle for layer in scene.view_layers),
            scene.world.name if scene.world is not None else None,
            view_layer.material_override.name
            if view_layer.material_override is not None
            else None,
            scene.display_settings.display_device,
            scene.view_settings.view_transform,
            scene.view_settings.look,
            scene.view_settings.exposure,
            scene.view_settings.gamma,
            scene.view_settings.use_curve_mapping,
            scene.view_settings.use_hdr_view,
            scene.render.dither_intensity,
            scene.render.use_compositing,
            scene.render.image_settings.color_management,
            scene.render.image_settings.file_format,
            scene.render.image_settings.color_mode,
            scene.render.image_settings.color_depth,
            mesh_materials,
        )

    before_silhouette = state_signature()
    loaded_image = None
    silhouette_black_white_verified = False
    try:
        _render_silhouette(scene, scene.camera, silhouette_path)
        loaded_image = bpy.data.images.load(str(silhouette_path), check_existing=False)
        pixels = list(loaded_image.pixels)
        _validate_silhouette_pixels(
            pixels,
            int(loaded_image.size[0]),
            int(loaded_image.size[1]),
        )
        if state_signature() != before_silhouette:
            raise RuntimeError("silhouette render did not restore incoming scene state")
        silhouette_black_white_verified = True
    finally:
        if loaded_image is not None:
            bpy.data.images.remove(loaded_image)
        if silhouette_path.is_file():
            silhouette_path.unlink()

    return {
        "capital_mutation_rejected": capital_mutation_rejected,
        "divider_mutation_rejected": divider_mutation_rejected,
        "silhouette_black_white_verified": silhouette_black_white_verified,
    }


def _reset_and_build(plan: dict) -> tuple[
    bpy.types.Scene,
    dict[str, bpy.types.Collection],
    dict[str, bpy.types.Object],
]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "SCN_Qingshan_Golden_Inn"
    _configure_scene(scene)

    module_collections: dict[str, bpy.types.Collection] = {}
    for name in MODULE_NAMES:
        collection = bpy.data.collections.new(name)
        scene.collection.children.link(collection)
        module_collections[name] = collection

    materials = _create_materials()
    _create_body(module_collections["BODY"], materials)
    _create_door(module_collections["DOOR_A"], materials, float(plan["facade_offset_m"]))
    _create_windows(
        module_collections["WINDOW_A"], materials, int(plan["window_divisions"])
    )
    _create_eave_structure(module_collections["EAVE_A"], materials, plan["columns"])
    create_curved_roof(
        "Roof_Main_Asymmetric_Xieshan",
        module_collections["ROOF_A"],
        ridge_offset_m=float(plan["roof_ridge_offset_m"]),
        material=materials["roof"],
    )
    _create_curved_ridge(module_collections["ROOF_A"], materials["timber"])
    _apply_mesh_transforms(scene)
    cameras = create_fixed_cameras(scene)
    validate_camera_framing(scene, cameras)
    return scene, module_collections, cameras


def main() -> None:
    args = _parse_blender_args()
    _require_supported_background_blender()
    requested_root = args.project_root.expanduser()
    raw_paths = output_paths(requested_root, args.version)
    project_root, paths = _validate_output_destinations(requested_root, raw_paths)
    contract = load_golden_contract(project_root / ASSET_RECORD)
    plan = build_component_plan(contract)
    if args.self_test_audit:
        output_directory = paths["blend"].parent
        _refuse_existing_self_test_outputs(paths)
        output_directory.mkdir(parents=True, exist_ok=True)
        project_root, paths = _validate_output_destinations(project_root, paths)
        _refuse_existing_self_test_outputs(paths)
        try:
            scene, module_collections, _cameras = _reset_and_build(plan)
            result = _run_audit_mutation_self_test(
                scene, module_collections, plan, paths["silhouette_128"]
            )
        finally:
            for directory in (output_directory, output_directory.parent):
                try:
                    directory.rmdir()
                except OSError:
                    pass
        print(json.dumps(result, sort_keys=True))
        return
    paths["blend"].parent.mkdir(parents=True, exist_ok=True)
    project_root, paths = _validate_output_destinations(project_root, paths)
    _invalidate_previous_outputs(paths)

    scene, module_collections, cameras = _reset_and_build(plan)
    created_keys: set[str] = set()
    for view_name in REQUIRED_VIEWS:
        _render_view(scene, cameras[view_name], paths[view_name])
        created_keys.add(view_name)
    _render_silhouette(scene, cameras["hero_3q"], paths["silhouette_128"])
    created_keys.add("silhouette_128")

    report = audit_scene(scene, module_collections, plan)
    report["version"] = args.version
    scene.camera = cameras["hero_3q"]
    scene.render.filepath = str(paths["hero_3q"])
    _remove_stale_blend_backup(paths["blend"])
    save_result = bpy.ops.wm.save_as_mainfile(filepath=str(paths["blend"]))
    _require_finished(save_result, "save blend")
    created_keys.add("blend")
    _assert_current_run_outputs(paths, created_keys)

    report["output_sha256"] = {
        key: sha256_file(path) for key, path in paths.items() if key != "report"
    }
    paths["report"].write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(
        json.dumps(
            {
                "ok": True,
                "version": args.version,
                "blend": str(paths["blend"]),
                "report": str(paths["report"]),
                "faces": report["faces"],
                "bounds_m": report["bounds_m"],
            },
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    try:
        main()
    except Exception:
        traceback.print_exc()
        raise SystemExit(1)
