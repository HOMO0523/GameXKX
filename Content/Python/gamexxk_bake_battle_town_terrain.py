"""Guarded writer for the approved Qingshan-town battle-terrain transfer.

This entry point intentionally refuses to write unless an operator supplies
``--execute`` after reviewing an audited manifest.  It owns only the battle
terrain output root and will never call the legacy map-reset utility.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import tempfile
import struct
import zlib
from pathlib import Path
from typing import Any

try:
    import unreal
except ModuleNotFoundError:
    unreal = None


PROJECT_ROOT = Path(__file__).resolve().parents[2]
OUTPUT_ROOT = "/Game/GameXXK/Environment/Battle/TownTerrain"
OUTPUT_MESH = OUTPUT_ROOT + "/SM_Battle_QingshanGround_01"
OUTPUT_MATERIAL = OUTPUT_ROOT + "/M_Battle_QingshanGround_01"
OUTPUT_ALBEDO = OUTPUT_ROOT + "/T_Battle_QingshanGround_Albedo_01"
ENCOUNTER_FLOOR_ID = "GameXXK_Encounter_Floor"
MANIFEST_ROOT = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "battle-town-terrain"
INVALID_CAPTURE_SRGB = 173
INVALID_CAPTURE_TOLERANCE = 1
CAPTURE_CAMERA_RESOLUTION = 1024
CAPTURE_HEIGHT_PADDING_CM = 100.0


def _parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    environment_manifest = os.environ.get("GAMEXXK_BATTLE_TERRAIN_MANIFEST", "").strip()
    parser.add_argument(
        "--manifest",
        required=False,
        default=Path(environment_manifest) if environment_manifest else None,
        type=Path,
        help="Reviewed battle-town-terrain manifest under SourceAssets/PartyDeck/battle-town-terrain.",
    )
    parser.add_argument(
        "--execute",
        action="store_true",
        help="Explicitly allow the UE-only write phase after a reviewed before snapshot.",
    )
    parser.add_argument(
        "--rollback-output",
        action="store_true",
        help="Explicitly remove only stale assets owned by this baker's output contract.",
    )
    parser.add_argument(
        "--dry-run-capture",
        action="store_true",
        help=(
            "Capture and validate the town terrain only into Saved; creates no UE assets "
            "and never binds the battle floor."
        ),
    )
    parser.add_argument(
        "--capture-report",
        type=Path,
        default=None,
        help=(
            "Required JSON report path under <Project>/Saved for --dry-run-capture or --execute. "
            "A sibling .capture.png is retained as pixel-level evidence."
        ),
    )
    args = parser.parse_args(argv)
    if args.manifest is None:
        parser.error("--manifest or GAMEXXK_BATTLE_TERRAIN_MANIFEST is required")
    if os.environ.get("GAMEXXK_BATTLE_TERRAIN_EXECUTE", "").strip() == "1":
        args.execute = True
    if args.dry_run_capture and args.execute:
        parser.error("--dry-run-capture and --execute are mutually exclusive")
    if args.rollback_output and (args.dry_run_capture or args.execute):
        parser.error("--rollback-output cannot be combined with --dry-run-capture or --execute")
    if (args.dry_run_capture or args.execute) and args.capture_report is None:
        parser.error("--capture-report is required for --dry-run-capture or --execute")
    if args.capture_report is not None:
        args.capture_report = _capture_report_under_project_saved(args.capture_report)
    return args


def _project_saved_dir() -> Path:
    return (PROJECT_ROOT / "Saved").resolve()


def _capture_report_under_project_saved(filename: Path) -> Path:
    resolved = filename.resolve()
    saved_dir = _project_saved_dir()
    try:
        resolved.relative_to(saved_dir)
    except ValueError as exc:
        raise RuntimeError(f"--capture-report must be inside the project Saved directory: {saved_dir}") from exc
    if resolved == saved_dir or resolved.suffix.lower() != ".json":
        raise RuntimeError("--capture-report must be a .json file below the project Saved directory")
    return resolved


def _load_manifest(filename: Path) -> dict[str, Any]:
    resolved = filename.resolve()
    try:
        resolved.relative_to(MANIFEST_ROOT.resolve())
    except ValueError as exc:
        raise RuntimeError(f"manifest must remain under {MANIFEST_ROOT}: {resolved}") from exc
    if not resolved.is_file():
        raise RuntimeError(f"manifest is missing: {resolved}")
    try:
        payload = json.loads(resolved.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"manifest is not valid JSON: {resolved}") from exc
    if not isinstance(payload, dict):
        raise RuntimeError("manifest root must be a JSON object")
    return payload


def _require_ready_manifest(manifest: dict[str, Any]) -> None:
    if manifest.get("status") != "audited":
        raise RuntimeError("Refusing to write: manifest must have reviewed status 'audited'.")
    if not manifest.get("source_landscape"):
        raise RuntimeError("Refusing to write: manifest has no audited source Landscape.")
    if not manifest.get("slice_world_bounds_cm"):
        raise RuntimeError("Refusing to write: manifest has no audited candidate slice bounds.")
    if not manifest.get("source_hashes"):
        raise RuntimeError("Refusing to write: manifest has no before-snapshot source hashes.")


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("UE Python is required for the explicit terrain write phase.")


def _require_absent_output_assets() -> None:
    asset_library = unreal.EditorAssetLibrary
    existing = [
        path
        for path in (OUTPUT_MESH, OUTPUT_MATERIAL, OUTPUT_ALBEDO)
        if asset_library.does_asset_exist(path)
    ]
    if existing:
        raise RuntimeError(
            "Refusing to overwrite existing terrain output assets: " + ", ".join(existing)
        )


def _delete_owned_output_assets() -> list[str]:
    """Remove only a failed bake's exact output assets after explicit confirmation."""

    asset_library = unreal.EditorAssetLibrary
    deleted: list[str] = []
    for path in (OUTPUT_MESH, OUTPUT_MATERIAL, OUTPUT_ALBEDO):
        if asset_library.does_asset_exist(path):
            if not asset_library.delete_asset(path):
                raise RuntimeError(f"could not remove stale baker output asset: {path}")
            deleted.append(path)
    return deleted


def _manifest_bounds(manifest: dict[str, Any]) -> tuple[float, float, float, float]:
    bounds = manifest.get("slice_world_bounds_cm")
    if not isinstance(bounds, dict):
        raise RuntimeError("Refusing to write: audited slice bounds are malformed.")
    minimum = bounds.get("min")
    maximum = bounds.get("max")
    if not isinstance(minimum, dict) or not isinstance(maximum, dict):
        raise RuntimeError("Refusing to write: audited slice bounds require min and max coordinates.")
    try:
        min_x = float(minimum["x"])
        min_y = float(minimum["y"])
        max_x = float(maximum["x"])
        max_y = float(maximum["y"])
    except (KeyError, TypeError, ValueError) as exc:
        raise RuntimeError("Refusing to write: audited slice bounds have invalid coordinates.") from exc
    if max_x <= min_x or max_y <= min_y:
        raise RuntimeError("Refusing to write: audited slice bounds have non-positive dimensions.")
    return min_x, min_y, max_x, max_y


def _verify_manifest_source_hashes(manifest: dict[str, Any], audit: Any) -> None:
    expected = manifest.get("source_hashes")
    if not isinstance(expected, dict):
        raise RuntimeError("Refusing to write: source hashes must be an object.")
    required = (
        ("town_package_sha256", audit.TOWN_MAP),
        ("battle_package_sha256", audit.BATTLE_MAP),
    )
    for key, package_path in required:
        expected_hash = str(expected.get(key, ""))
        current_hash = audit.sha256_file(audit._package_file(package_path))
        if not expected_hash or current_hash != expected_hash:
            raise RuntimeError(
                f"Refusing to write: baseline mismatch for {package_path}; re-audit before baking."
            )


def _find_source_landscape(audit: Any, required_name: str) -> object:
    matches = [
        actor
        for actor in audit._all_level_actors()
        if audit._is_landscape(actor) and str(actor.get_name()) == required_name
    ]
    if len(matches) != 1:
        raise RuntimeError(
            f"Refusing to write: expected exactly one audited Landscape {required_name}, got {len(matches)}."
        )
    return matches[0]


def _sample_landscape_height(world: object, x: float, y: float) -> float:
    hit = unreal.SystemLibrary.line_trace_single(
        world,
        unreal.Vector(float(x), float(y), 100000.0),
        unreal.Vector(float(x), float(y), -50000.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        True,
        [],
        unreal.DrawDebugTrace.NONE,
        True,
    )
    if hit is None:
        raise RuntimeError(f"could not sample Landscape height at ({x:.3f}, {y:.3f})")
    payload = hit.to_dict()
    point = payload.get("impact_point") if isinstance(payload, dict) else None
    if point is None:
        raise RuntimeError(f"Landscape trace did not provide an impact point at ({x:.3f}, {y:.3f})")
    return float(point.z)


def _build_sampled_dynamic_mesh(
    world: object,
    min_x: float,
    min_y: float,
    max_x: float,
    max_y: float,
) -> tuple[object, object, dict[str, float]]:
    """Create an in-memory grid whose local heights are sampled from the town Landscape."""

    width = max_x - min_x
    depth = max_y - min_y
    center_x = min_x + width * 0.5
    center_y = min_y + depth * 0.5
    reference_height = _sample_landscape_height(world, center_x, center_y)

    mesh = unreal.DynamicMesh()
    primitive_options = unreal.GeometryScriptPrimitiveOptions()
    # 49 x 29 vertices provides a 50cm sampling cadence while keeping the battle
    # ground compact. These are mesh vertex counts, not an approximation texture.
    unreal.GeometryScript_Primitives.append_rectangle_xy(
        mesh,
        primitive_options,
        unreal.Transform(),
        width,
        depth,
        49,
        29,
    )
    _, position_list, has_gaps = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False)
    if has_gaps:
        raise RuntimeError("generated terrain grid has unsupported vertex ID gaps")
    local_positions = unreal.GeometryScript_List.convert_vector_list_to_array(position_list)
    sampled_positions = []
    min_height = reference_height
    max_height = reference_height
    for local in local_positions:
        world_x = center_x + float(local.x)
        world_y = center_y + float(local.y)
        world_z = _sample_landscape_height(world, world_x, world_y)
        min_height = min(min_height, world_z)
        max_height = max(max_height, world_z)
        sampled_positions.append(unreal.Vector(float(local.x), float(local.y), world_z - reference_height))
    sampled_list = unreal.GeometryScript_List.convert_array_to_vector_list(sampled_positions)
    unreal.GeometryScript_MeshEdits.set_all_mesh_vertex_positions(mesh, sampled_list)
    source_transform = unreal.Transform(location=unreal.Vector(center_x, center_y, reference_height))
    return mesh, source_transform, {
        "min_x": min_x,
        "min_y": min_y,
        "max_x": max_x,
        "max_y": max_y,
        "center_x": center_x,
        "center_y": center_y,
        "reference_height": reference_height,
        "min_height": min_height,
        "max_height": max_height,
        "vertex_count": float(len(sampled_positions)),
    }


def _capture_town_albedo(
    mesh: object,
    source_transform: object,
    landscape: object,
    geometry: dict[str, float],
) -> tuple[object, dict[str, object]]:
    """Bake only the audited landscape slice with explicit local cameras.

    An empty camera list makes UE derive an exterior camera set from the full
    Landscape bounds. That is far too broad for this 24m x 14m target and was
    the source of the invalid-capture fallback gray in v1.
    """
    target_options = unreal.GeometryScriptBakeTargetMeshOptions()
    target_options.set_editor_property("target_uv_layer", 0)

    capture_box = unreal.Box(
        unreal.Vector(
            float(geometry["min_x"]),
            float(geometry["min_y"]),
            float(geometry["min_height"]) - CAPTURE_HEIGHT_PADDING_CM,
        ),
        unreal.Vector(
            float(geometry["max_x"]),
            float(geometry["max_y"]),
            float(geometry["max_height"]) + CAPTURE_HEIGHT_PADDING_CM,
        ),
    )
    camera_options = unreal.GeometryScriptRenderCaptureCamerasForBoxOptions()
    camera_options.set_editor_property("resolution", CAPTURE_CAMERA_RESOLUTION)
    camera_options.set_editor_property("field_of_view_degrees", 45.0)
    camera_options.set_editor_property("view_from_box_faces", True)
    camera_options.set_editor_property("view_from_upper_corners", True)
    camera_options.set_editor_property("view_from_lower_corners", False)
    camera_options.set_editor_property("view_from_upper_edges", False)
    camera_options.set_editor_property("view_from_lower_edges", False)
    camera_options.set_editor_property("view_from_side_edges", False)
    cameras = list(
        unreal.GeometryScript_MeshSampling.compute_render_capture_cameras_for_box(
            capture_box,
            camera_options,
        )
    )
    if not cameras:
        raise RuntimeError("town terrain local render capture did not produce any cameras")

    capture_options = unreal.GeometryScriptBakeRenderCaptureOptions()
    capture_options.set_editor_property(
        "resolution", unreal.GeometryScriptBakeResolution.RESOLUTION1024
    )
    capture_options.set_editor_property(
        "render_capture_resolution", unreal.GeometryScriptBakeResolution.RESOLUTION1024
    )
    capture_options.set_editor_property("samples_per_pixel", unreal.GeometryScriptBakeSamplesPerPixel.SAMPLE4)
    capture_options.set_editor_property("base_color_map", True)
    # The sampled height grid has valid geometric normals after asset creation,
    # but it does not carry a tangent layer while still in DynamicMesh form.
    # Request only the verified base-color capture instead of fabricating one.
    capture_options.set_editor_property("normal_map", False)
    # Target and source are sampled from the same Landscape. The default depth
    # cleanup rejects valid local correspondences against large-landscape cameras.
    capture_options.set_editor_property("cleanup_tolerance", 0.0)
    capture_options.set_editor_property("cameras", cameras)
    captured = unreal.GeometryScript_Bake.bake_texture_from_render_captures(
        mesh,
        source_transform,
        target_options,
        [landscape],
        capture_options,
    )
    if not bool(captured.get_editor_property("has_base_color_map")):
        raise RuntimeError("town Landscape render capture did not yield a base-color texture")
    texture = captured.get_editor_property("base_color_map")
    if texture is None:
        raise RuntimeError("town Landscape base-color capture is null")
    return texture, {
        "camera_count": len(cameras),
        "camera_resolution": CAPTURE_CAMERA_RESOLUTION,
        "cleanup_tolerance": 0.0,
        "capture_box": {
            "min": {
                "x": float(geometry["min_x"]),
                "y": float(geometry["min_y"]),
                "z": float(geometry["min_height"]) - CAPTURE_HEIGHT_PADDING_CM,
            },
            "max": {
                "x": float(geometry["max_x"]),
                "y": float(geometry["max_y"]),
                "z": float(geometry["max_height"]) + CAPTURE_HEIGHT_PADDING_CM,
            },
        },
    }


def _capture_preview_path(report_path: Path) -> Path:
    safe_report = _capture_report_under_project_saved(report_path)
    return safe_report.with_suffix(".capture.png")


def _write_capture_report(report_path: Path, payload: dict[str, object]) -> None:
    safe_report = _capture_report_under_project_saved(report_path)
    if safe_report.exists():
        raise RuntimeError(f"refusing to overwrite existing capture report: {safe_report}")
    safe_report.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{safe_report.stem}.",
        suffix=".tmp",
        dir=str(safe_report.parent),
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as handle:
            json.dump(payload, handle, ensure_ascii=False, indent=2, sort_keys=True)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        if safe_report.exists():
            raise RuntimeError(f"refusing to overwrite existing capture report: {safe_report}")
        os.replace(temporary, safe_report)
    finally:
        if temporary.exists():
            temporary.unlink()


def _export_capture_preview(captured_texture: object, preview_path: Path) -> None:
    safe_preview = preview_path.resolve()
    try:
        safe_preview.relative_to(_project_saved_dir())
    except ValueError as exc:
        raise RuntimeError(f"capture preview must remain under project Saved: {safe_preview}") from exc
    if safe_preview.suffix.lower() != ".png":
        raise RuntimeError(f"capture preview must be a .png: {safe_preview}")
    if safe_preview.exists():
        raise RuntimeError(f"refusing to overwrite existing capture preview: {safe_preview}")
    safe_preview.parent.mkdir(parents=True, exist_ok=True)
    task = unreal.AssetExportTask()
    task.set_editor_property("object", captured_texture)
    task.set_editor_property("filename", str(safe_preview))
    task.set_editor_property("automated", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("replace_identical", False)
    if not unreal.Exporter.run_asset_export_task(task) or not safe_preview.is_file():
        raise RuntimeError(f"could not export transient town terrain capture preview: {safe_preview}")


def _paeth_predictor(left: int, above: int, upper_left: int) -> int:
    estimate = left + above - upper_left
    left_distance = abs(estimate - left)
    above_distance = abs(estimate - above)
    upper_left_distance = abs(estimate - upper_left)
    if left_distance <= above_distance and left_distance <= upper_left_distance:
        return left
    if above_distance <= upper_left_distance:
        return above
    return upper_left


def _parse_png_rgb_statistics(filename: Path) -> dict[str, object]:
    """Read an exported 8-bit RGB/RGBA PNG without pulling in a host image library."""
    raw = filename.read_bytes()
    signature = b"\x89PNG\r\n\x1a\n"
    if len(raw) < len(signature) or raw[:8] != signature:
        raise RuntimeError(f"capture preview is not a PNG: {filename}")
    cursor = len(signature)
    width = height = bit_depth = color_type = compression = filter_method = interlace = None
    idat_chunks: list[bytes] = []
    while cursor + 12 <= len(raw):
        length = struct.unpack(">I", raw[cursor : cursor + 4])[0]
        kind = raw[cursor + 4 : cursor + 8]
        payload_start = cursor + 8
        payload_end = payload_start + length
        if payload_end + 4 > len(raw):
            raise RuntimeError(f"capture preview PNG chunk is truncated: {filename}")
        payload = raw[payload_start:payload_end]
        cursor = payload_end + 4
        if kind == b"IHDR":
            if len(payload) != 13:
                raise RuntimeError(f"capture preview IHDR is malformed: {filename}")
            width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
        elif kind == b"IDAT":
            idat_chunks.append(payload)
        elif kind == b"IEND":
            break
    if (
        not width
        or not height
        or bit_depth != 8
        or color_type not in (2, 6)
        or compression != 0
        or filter_method != 0
        or interlace != 0
        or not idat_chunks
    ):
        raise RuntimeError(f"capture preview PNG format is unsupported: {filename}")
    channels = 3 if color_type == 2 else 4
    stride = int(width) * channels
    decoded = zlib.decompress(b"".join(idat_chunks))
    expected = int(height) * (stride + 1)
    if len(decoded) != expected:
        raise RuntimeError(f"capture preview PNG scanline size is unexpected: {filename}")

    minimum = [255, 255, 255]
    maximum = [0, 0, 0]
    previous = bytearray(stride)
    offset = 0
    for _row in range(int(height)):
        filter_type = decoded[offset]
        offset += 1
        row = bytearray(decoded[offset : offset + stride])
        offset += stride
        for index, value in enumerate(row):
            left = row[index - channels] if index >= channels else 0
            above = previous[index]
            upper_left = previous[index - channels] if index >= channels else 0
            if filter_type == 0:
                reconstructed = value
            elif filter_type == 1:
                reconstructed = (value + left) & 0xFF
            elif filter_type == 2:
                reconstructed = (value + above) & 0xFF
            elif filter_type == 3:
                reconstructed = (value + ((left + above) // 2)) & 0xFF
            elif filter_type == 4:
                reconstructed = (value + _paeth_predictor(left, above, upper_left)) & 0xFF
            else:
                raise RuntimeError(f"capture preview PNG uses unsupported filter {filter_type}: {filename}")
            row[index] = reconstructed
        for index in range(0, stride, channels):
            for channel in range(3):
                value = row[index + channel]
                minimum[channel] = min(minimum[channel], value)
                maximum[channel] = max(maximum[channel], value)
        previous = row
    return {
        "width": int(width),
        "height": int(height),
        "min_rgb": minimum,
        "max_rgb": maximum,
        "sample_count": int(width) * int(height),
    }


def _validate_capture_statistics(statistics: dict[str, object]) -> dict[str, object]:
    minimum = [int(value) for value in statistics.get("min_rgb", [])]
    maximum = [int(value) for value in statistics.get("max_rgb", [])]
    if len(minimum) != 3 or len(maximum) != 3 or int(statistics.get("sample_count", 0)) <= 0:
        return {"ok": False, "reason": "missing_capture_statistics"}
    invalid_low = INVALID_CAPTURE_SRGB - INVALID_CAPTURE_TOLERANCE
    invalid_high = INVALID_CAPTURE_SRGB + INVALID_CAPTURE_TOLERANCE
    if all(invalid_low <= value <= invalid_high for value in minimum + maximum):
        return {"ok": False, "reason": "invalid_capture_fallback"}
    if minimum == maximum:
        return {"ok": False, "reason": "flat_capture"}
    return {"ok": True, "reason": ""}


def _validate_capture_preview(
    captured_texture: object,
    report_path: Path,
    capture_metadata: dict[str, object],
) -> dict[str, object]:
    preview_path = _capture_preview_path(report_path)
    _export_capture_preview(captured_texture, preview_path)
    statistics = _parse_png_rgb_statistics(preview_path)
    validation = _validate_capture_statistics(statistics)
    return {
        "preview": str(preview_path),
        "capture": capture_metadata,
        "statistics": statistics,
        "validation": validation,
    }


def _save_captured_texture(captured_texture: object) -> object:
    options = unreal.GeometryScriptCreateNewTexture2DAssetOptions()
    texture, outcome = unreal.GeometryScript_NewAssetUtils.create_new_texture2d_asset(
        captured_texture,
        OUTPUT_ALBEDO,
        options,
    )
    if texture is None:
        raise RuntimeError(f"could not create captured town albedo asset ({outcome})")
    texture.set_editor_property("srgb", True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError("could not save captured town albedo asset")
    return texture


def _connect_expression(material: object, source: object, source_output: str, target: object, inputs: tuple[str, ...]) -> None:
    for input_name in inputs:
        if unreal.MaterialEditingLibrary.connect_material_expressions(
            source,
            source_output,
            target,
            input_name,
        ):
            return
    raise RuntimeError(f"could not connect terrain material input candidates: {inputs}")


def _create_town_ground_material(texture: object) -> object:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(
        OUTPUT_MATERIAL.rsplit("/", 1)[-1],
        OUTPUT_ROOT,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if material is None:
        raise RuntimeError("could not create town-ground material")
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("two_sided", False)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    texture_node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        -420,
        0,
    )
    texture_node.set_editor_property("texture", texture)
    texture_node.set_editor_property("parameter_name", "TownTerrainAlbedo")
    unlit_node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionSubstrateUnlitBSDF,
        20,
        0,
    )
    _connect_expression(material, texture_node, "RGB", unlit_node, ("Emissive Color", "EmissiveColor"))
    if not unreal.MaterialEditingLibrary.connect_material_property(
        unlit_node,
        "",
        unreal.MaterialProperty.MP_FRONT_MATERIAL,
    ):
        raise RuntimeError("could not connect town-ground material to Front Material")
    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    errors = [str(item) for item in unreal.MaterialEditingLibrary.recompile_material(material)]
    if errors:
        raise RuntimeError(f"town-ground material failed to compile: {errors}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("could not save town-ground material")
    return material


def _save_static_mesh(mesh: object) -> object:
    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.set_editor_property("enable_recompute_normals", True)
    options.set_editor_property("enable_recompute_tangents", True)
    options.set_editor_property("enable_collision", True)
    static_mesh, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
        mesh,
        OUTPUT_MESH,
        options,
    )
    if static_mesh is None:
        raise RuntimeError(f"could not create sampled town terrain mesh ({outcome})")
    if not unreal.EditorAssetLibrary.save_loaded_asset(static_mesh):
        raise RuntimeError("could not save sampled town terrain mesh")
    return static_mesh


def _find_single_floor_component(audit: Any) -> tuple[object, object]:
    floor = audit.find_single_encounter_floor()
    components = list(floor.get_components_by_class(unreal.StaticMeshComponent))
    if len(components) != 1:
        raise RuntimeError(
            f"{ENCOUNTER_FLOOR_ID} must expose exactly one StaticMeshComponent, got {len(components)}"
        )
    return floor, components[0]


def _apply_only_to_encounter_floor(audit: Any, static_mesh: object, material: object) -> dict[str, Any]:
    floor, component = _find_single_floor_component(audit)
    before_transform = audit._transform_payload(floor)
    component.set_static_mesh(static_mesh)
    component.set_material(0, material)
    after_transform = audit._transform_payload(floor)
    if before_transform != after_transform:
        raise RuntimeError("Refusing to continue: encounter floor transform changed during resource replacement")
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("could not save L_BattleScene after replacing the protected encounter floor resources")
    return {
        "actor_label": audit._actor_label(floor),
        "mesh": audit._object_path(component.get_editor_property("static_mesh")),
        "material": audit._object_path(component.get_material(0)),
        "transform": after_transform,
    }


def _asset_package_hash(asset_path: str, audit: Any) -> str:
    relative = Path(asset_path.removeprefix("/Game/")).with_suffix(".uasset")
    filename = audit._project_content_dir() / relative
    if not filename.is_file():
        raise RuntimeError(f"expected output package was not saved: {filename}")
    return hashlib.sha256(filename.read_bytes()).hexdigest()


def _write_baked_manifest(filename: Path, manifest: dict[str, Any], result: dict[str, Any], audit: Any) -> None:
    updated = dict(manifest)
    updated["status"] = "baked"
    updated["output_assets"] = [OUTPUT_MESH, OUTPUT_MATERIAL, OUTPUT_ALBEDO]
    updated["bake"] = result
    updated["output_hashes"] = {
        path: _asset_package_hash(path, audit)
        for path in (OUTPUT_MESH, OUTPUT_MATERIAL, OUTPUT_ALBEDO)
    }
    temporary = filename.with_suffix(filename.suffix + ".tmp")
    if temporary.exists():
        raise RuntimeError(f"refusing to overwrite stale temporary manifest: {temporary}")
    try:
        temporary.write_text(json.dumps(updated, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        os.replace(temporary, filename)
    finally:
        if temporary.exists():
            temporary.unlink()


def _capture_and_validate_town_slice(
    manifest: dict[str, Any],
    audit: Any,
    report_path: Path,
) -> tuple[object, object, dict[str, float], dict[str, object]]:
    min_x, min_y, max_x, max_y = _manifest_bounds(manifest)
    audit._load_map_for_reading(audit.TOWN_MAP)
    source_landscape = _find_source_landscape(audit, str(manifest["source_landscape"]))
    world = audit._editor_world()
    mesh, source_transform, geometry = _build_sampled_dynamic_mesh(
        world,
        min_x,
        min_y,
        max_x,
        max_y,
    )
    captured_texture, capture_metadata = _capture_town_albedo(
        mesh,
        source_transform,
        source_landscape,
        geometry,
    )
    preview = _validate_capture_preview(captured_texture, report_path, capture_metadata)
    return mesh, captured_texture, geometry, preview


def _capture_report_payload(
    operation: str,
    manifest: dict[str, Any],
    geometry: dict[str, float],
    preview: dict[str, object],
) -> dict[str, object]:
    return {
        "ok": bool(preview["validation"]["ok"]),
        "operation": operation,
        "source_map": manifest["source_map"],
        "source_landscape": manifest["source_landscape"],
        "slice_world_bounds_cm": manifest["slice_world_bounds_cm"],
        "geometry": geometry,
        "preview": preview,
    }


def _raise_if_capture_invalid(report: dict[str, object]) -> None:
    preview = report.get("preview", {})
    validation = preview.get("validation", {}) if isinstance(preview, dict) else {}
    if not bool(validation.get("ok")):
        raise RuntimeError(
            "town terrain capture rejected before any UE asset write: "
            + str(validation.get("reason", "unknown_capture_error"))
        )


def _execute_dry_run_capture(manifest: dict[str, Any], report_path: Path) -> dict[str, object]:
    import gamexxk_audit_battle_town_terrain as audit

    _verify_manifest_source_hashes(manifest, audit)
    audit._require_audit_session_safe()
    original_map = audit._capture_original_map()
    try:
        _mesh, _captured_texture, geometry, preview = _capture_and_validate_town_slice(
            manifest,
            audit,
            report_path,
        )
    finally:
        audit._restore_original_map(original_map)
    report = _capture_report_payload("dry_run_capture", manifest, geometry, preview)
    _write_capture_report(report_path, report)
    _raise_if_capture_invalid(report)
    audit._require_audit_session_safe()
    return report


def _execute_bake(
    manifest_filename: Path,
    manifest: dict[str, Any],
    capture_report_path: Path,
) -> dict[str, Any]:
    import gamexxk_audit_battle_town_terrain as audit

    _verify_manifest_source_hashes(manifest, audit)
    audit._require_audit_session_safe()
    original_map = audit._capture_original_map()
    try:
        mesh, captured_texture, geometry, preview = _capture_and_validate_town_slice(
            manifest,
            audit,
            capture_report_path,
        )
        capture_report = _capture_report_payload("execute", manifest, geometry, preview)
        _write_capture_report(capture_report_path, capture_report)
        _raise_if_capture_invalid(capture_report)
        albedo = _save_captured_texture(captured_texture)
        material = _create_town_ground_material(albedo)
        static_mesh = _save_static_mesh(mesh)

        audit._require_audit_session_safe()
        audit._load_map_for_reading(audit.BATTLE_MAP)
        floor = _apply_only_to_encounter_floor(audit, static_mesh, material)
        audit._require_audit_session_safe()
    finally:
        audit._restore_original_map(original_map)

    result = {
        "source_landscape": str(manifest["source_landscape"]),
        "slice_world_bounds_cm": manifest["slice_world_bounds_cm"],
        "geometry": geometry,
        "encounter_floor": floor,
        "capture_report": str(capture_report_path),
        "engine_version": str(unreal.SystemLibrary.get_engine_version()),
    }
    _write_baked_manifest(manifest_filename, manifest, result, audit)
    return result


def main(argv: list[str] | None = None) -> dict[str, Any]:
    args = _parse_args(argv)

    if args.rollback_output:
        _require_unreal()
        return {"deleted_output_assets": _delete_owned_output_assets()}

    if not args.execute and not args.dry_run_capture:
        raise RuntimeError("Refusing to write: pass --execute after a reviewed before snapshot.")

    manifest = _load_manifest(args.manifest)
    _require_ready_manifest(manifest)
    _require_unreal()
    if args.dry_run_capture:
        assert args.capture_report is not None
        return _execute_dry_run_capture(manifest, args.capture_report)
    _require_absent_output_assets()
    assert args.capture_report is not None
    return _execute_bake(args.manifest.resolve(), manifest, args.capture_report)


if __name__ == "__main__":
    try:
        print(json.dumps(main(sys.argv[1:]), ensure_ascii=False, sort_keys=True))
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1) from exc
