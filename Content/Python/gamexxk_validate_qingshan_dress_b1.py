"""Read-only validation and canonical live manifest for Qingshan B1."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
import struct
import zlib
from typing import Any, Iterable

try:  # Host tests exercise the pure helpers without importing Unreal.
    import unreal
except ModuleNotFoundError:  # pragma: no cover - only outside Unreal Editor.
    unreal = None

from gamexxk_qingshan_dress_b1_config import (
    load_config,
    validate_protected_file_hashes,
)


PROJECT_ROOT = Path(__file__).resolve().parents[2]
B1_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1"
B1_ASSET_ROOT = "/Game/GameXXK/Environment/TownPCG/B1"
B1_GRAPH_ROOT = f"{B1_ASSET_ROOT}/Graphs/"
B1_MANAGED_TAG = "QingshanB1Managed"
QUICKROAD_OWNER_TAG = "QingshanB1QuickRoadOwned"
VALIDATION_MARKER = "GAMEXXK_QINGSHAN_B1_VALIDATION"
GRAPH_CONTRACT_PREFIX = "GAMEXXK_TOWN_PCG_CONTRACT:"

EXPECTED_COUNTS = {
    "buildings": 26,
    "props": 72,
    "static_plants": 70,
    "mountains": 24,
    "animated_plants": 30,
}
EXPECTED_GROUP_COUNTS = {
    "building": 19,
    "prop": 10,
    "static_plant": 4,
    "mountain": 1,
}
EXPECTED_ARCHETYPE_COUNT = 6
EXPECTED_ROOF_PALETTE_COUNT = 4
EXPECTED_LANDSCAPE_RESOLUTION = 505
EXPECTED_LANDSCAPE_COMPONENT_COUNT = 16
EXPECTED_CAMERA_COUNT = 4
EXPECTED_GRAPH_COUNT = 34
MAX_BUILDING_BOTTOM_ERROR_CM = 25.0
EXPECTED_CAMERA_LABELS = (
    "CAM_QS_B1_GATE_ARRIVAL",
    "CAM_QS_B1_TOWN_CORE",
    "CAM_QS_B1_MAIN_BRIDGE",
    "CAM_QS_B1_SOUTH_DOCK",
)
APPROVED_PROTECTED_FILE_HASHES = {
    "Content/GameXXK/Maps/L_QingshanInn.umap":
        "a3639b38623d00e8ad3e5a610a3e1695a47b38c1d1e6fedb8115e1e9fdf5c8a8",
    "Content/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R.umap":
        "74292340df0cea97d99e905dd193a921038326bfec2f3ce034a5e9f70bd3f107",
    "Config/GameXXK/TownPCG/QingshanWhiteboxB0R.json":
        "3f231876d0083bffe28ee555b60af1a20b0966edbde9dc4bb6f2647e920eadb1",
}


def canonical_manifest_bytes(manifest: Any) -> bytes:
    """Encode deterministic compact JSON and reject NaN/Infinity."""

    return json.dumps(
        manifest,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")


def canonical_manifest_sha256(manifest: Any) -> str:
    return hashlib.sha256(canonical_manifest_bytes(manifest)).hexdigest()


def _project_relative_manifest_path(value: str | Path) -> str:
    relative = Path(value)
    if relative.is_absolute():
        raise ValueError(f"manifest path must be project-relative: {value}")
    root = PROJECT_ROOT.resolve()
    resolved = (root / relative).resolve()
    try:
        normalized = resolved.relative_to(root)
    except ValueError as error:
        raise ValueError(f"manifest path escapes project root: {value}") from error
    return normalized.as_posix()


def _paeth(left: int, above: int, upper_left: int) -> int:
    estimate = left + above - upper_left
    left_distance = abs(estimate - left)
    above_distance = abs(estimate - above)
    upper_left_distance = abs(estimate - upper_left)
    if left_distance <= above_distance and left_distance <= upper_left_distance:
        return left
    if above_distance <= upper_left_distance:
        return above
    return upper_left


def decode_png16_grayscale(path: str | Path) -> dict[str, Any]:
    """Decode a non-interlaced 16-bit grayscale PNG using only stdlib."""

    source = Path(path)
    payload = source.read_bytes()
    if payload[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"not a PNG file: {source}")
    offset = 8
    header = None
    compressed = bytearray()
    saw_end = False
    while offset < len(payload):
        if offset + 12 > len(payload):
            raise ValueError("truncated PNG chunk")
        length = struct.unpack(">I", payload[offset:offset + 4])[0]
        chunk_type = payload[offset + 4:offset + 8]
        data_start = offset + 8
        data_end = data_start + length
        if data_end + 4 > len(payload):
            raise ValueError("truncated PNG chunk data")
        chunk = payload[data_start:data_end]
        expected_crc = struct.unpack(">I", payload[data_end:data_end + 4])[0]
        if zlib.crc32(chunk_type + chunk) & 0xFFFFFFFF != expected_crc:
            raise ValueError(f"PNG CRC mismatch for {chunk_type!r}")
        if chunk_type == b"IHDR":
            if header is not None or length != 13:
                raise ValueError("invalid PNG IHDR")
            header = struct.unpack(">IIBBBBB", chunk)
        elif chunk_type == b"IDAT":
            compressed.extend(chunk)
        elif chunk_type == b"IEND":
            saw_end = True
            break
        offset = data_end + 4
    if header is None or not compressed or not saw_end:
        raise ValueError("PNG is missing IHDR, IDAT, or IEND")
    width, height, bit_depth, color_type, compression, filter_method, interlace = header
    if (
        width <= 0 or height <= 0 or bit_depth != 16 or color_type != 0
        or compression != 0 or filter_method != 0 or interlace != 0
    ):
        raise ValueError(
            "heightmap must be non-interlaced 16-bit grayscale PNG"
        )
    bytes_per_pixel = 2
    stride = width * bytes_per_pixel
    inflated = zlib.decompress(bytes(compressed))
    expected_length = height * (stride + 1)
    if len(inflated) != expected_length:
        raise ValueError(
            f"PNG scanline size mismatch: {len(inflated)} != {expected_length}"
        )
    rows = []
    previous = bytearray(stride)
    cursor = 0
    for _row_index in range(height):
        filter_type = inflated[cursor]
        encoded = inflated[cursor + 1:cursor + 1 + stride]
        cursor += stride + 1
        decoded = bytearray(stride)
        for index, value in enumerate(encoded):
            left = decoded[index - bytes_per_pixel] if index >= bytes_per_pixel else 0
            above = previous[index]
            upper_left = previous[index - bytes_per_pixel] if index >= bytes_per_pixel else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = above
            elif filter_type == 3:
                predictor = (left + above) // 2
            elif filter_type == 4:
                predictor = _paeth(left, above, upper_left)
            else:
                raise ValueError(f"unsupported PNG filter type: {filter_type}")
            decoded[index] = (value + predictor) & 0xFF
        rows.append(bytes(decoded))
        previous = decoded
    raw_u16 = b"".join(rows)
    values = [
        int.from_bytes(raw_u16[index:index + 2], "big")
        for index in range(0, len(raw_u16), 2)
    ]
    return {
        "width": int(width),
        "height": int(height),
        "values": values,
        "u16_sha256": hashlib.sha256(raw_u16).hexdigest(),
        "png_sha256": hashlib.sha256(payload).hexdigest(),
    }


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("live B1 validation requires Unreal Editor Python")


def _reject_json_constant(value: str):
    raise ValueError(f"nonfinite JSON constant is forbidden: {value}")


def _json_object(value: Any, context: str) -> dict[str, Any]:
    if isinstance(value, str):
        try:
            value = json.loads(value, parse_constant=_reject_json_constant)
        except (json.JSONDecodeError, ValueError) as error:
            raise RuntimeError(f"{context} returned invalid JSON") from error
    if not isinstance(value, dict):
        raise RuntimeError(f"{context} must be a JSON object")
    canonical_manifest_bytes(value)
    return value


def _operation_payload(operation: str, value: Any) -> dict[str, Any]:
    payload = _json_object(str(value), operation)
    if payload.get("success") is not True:
        raise RuntimeError(f"{operation} failed: {payload}")
    return payload


def _finite(value: Any, field: str, digits: int = 6) -> float:
    number = float(value)
    if not math.isfinite(number):
        raise RuntimeError(f"{field} must be finite")
    number = round(number, digits)
    return 0.0 if number == 0.0 else number


def _circular_phase_error_frames(
        *, reference_seconds: float, observed_seconds: float,
        configured_delta_frames: int, frame_count: int,
        frames_per_second: float) -> float:
    values = (reference_seconds, observed_seconds, frames_per_second)
    if not all(math.isfinite(float(value)) for value in values):
        raise ValueError("phase inputs must be finite")
    if frame_count <= 0 or frames_per_second <= 0.0:
        raise ValueError("phase frame count and frame rate must be positive")
    duration_seconds = float(frame_count) / float(frames_per_second)
    live_delta_frames = (
        (float(observed_seconds) - float(reference_seconds)) % duration_seconds
    ) * float(frames_per_second)
    configured = int(configured_delta_frames) % int(frame_count)
    signed_error = (
        (live_delta_frames - configured + frame_count * 0.5) % frame_count
        - frame_count * 0.5
    )
    return abs(float(signed_error))


def _vector_payload(value, field: str = "vector") -> list[float]:
    return [
        _finite(value.x, f"{field}.x"),
        _finite(value.y, f"{field}.y"),
        _finite(value.z, f"{field}.z"),
    ]


def _rotator_payload(value, field: str = "rotation") -> list[float]:
    return [
        _finite(value.pitch, f"{field}.pitch"),
        _finite(value.yaw, f"{field}.yaw"),
        _finite(value.roll, f"{field}.roll"),
    ]


def _transform_payload(value, field: str = "transform") -> dict[str, Any]:
    rotation = value.rotation.rotator() if hasattr(value.rotation, "rotator") else value.rotation
    return {
        "location": _vector_payload(value.translation, f"{field}.location"),
        "rotation": _rotator_payload(rotation, f"{field}.rotation"),
        "scale": _vector_payload(value.scale3d, f"{field}.scale"),
    }


def _package_path(value: Any) -> str:
    text = str(value or "")
    return text.split(".", 1)[0]


def _object_path(value: Any) -> str:
    if value is None:
        return ""
    getter = getattr(value, "get_path_name", None)
    if callable(getter):
        return _package_path(getter())
    return _package_path(value)


def _class_path(value: Any) -> str:
    object_class = value.get_class()
    return str(object_class.get_path_name())


def _read_property(value: Any, names: str | Iterable[str], *, required: bool = True):
    names = (names,) if isinstance(names, str) else tuple(names)
    for name in names:
        try:
            return value.get_editor_property(name)
        except Exception:
            candidate = getattr(value, name, None)
            if candidate is not None:
                return candidate
    if required:
        raise RuntimeError(f"could not read {names!r} from {value}")
    return None


def _actor_tags(actor) -> list[str]:
    return sorted(str(tag) for tag in _read_property(actor, "tags"))


def _component_tags(component) -> list[str]:
    return sorted(str(tag) for tag in _read_property(component, "component_tags"))


def _all_actors() -> list[Any]:
    return list(unreal.EditorLevelLibrary.get_all_level_actors())


def _actor_label(actor) -> str:
    return str(actor.get_actor_label())


def _find_unique_actor(label: str, expected_class=None):
    matches = [actor for actor in _all_actors() if _actor_label(actor) == label]
    if len(matches) != 1:
        raise RuntimeError(f"expected exactly one actor {label!r}, got {len(matches)}")
    actor = matches[0]
    if expected_class is not None and not isinstance(actor, expected_class):
        raise RuntimeError(f"actor {label!r} has class {_class_path(actor)}")
    return actor


def _read_only_b1_world_guard() -> dict[str, str]:
    world = unreal.EditorLevelLibrary.get_editor_world()
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    level = subsystem.get_current_level() if subsystem is not None else None
    world_package = _package_path(world.get_outermost().get_path_name()) if world else ""
    level_package = _package_path(level.get_outermost().get_path_name()) if level else ""
    if world_package != B1_MAP or level_package != B1_MAP:
        raise RuntimeError(
            f"validation requires exact B1 world/current level: "
            f"{world_package}/{level_package}"
        )
    return {"world_package": world_package, "current_level_package": level_package}


def _angular_difference(first: float, second: float) -> float:
    return abs((float(first) - float(second) + 180.0) % 360.0 - 180.0)


def _assert_vector_close(actual, expected, field: str, tolerance: float = 0.1) -> None:
    pairs = zip(_vector_payload(actual, field), _vector_payload(expected, field))
    if any(abs(first - second) > tolerance for first, second in pairs):
        raise RuntimeError(f"{field} differs from approved transform")


def _assert_rotator_close(actual, expected, field: str, tolerance: float = 0.1) -> None:
    if any(
        _angular_difference(first, second) > tolerance
        for first, second in zip(
            (actual.pitch, actual.yaw, actual.roll),
            (expected.pitch, expected.yaw, expected.roll),
        )
    ):
        raise RuntimeError(f"{field} differs from approved rotation")


def _enum_name(value: Any) -> str:
    return str(value).rsplit(".", 1)[-1].lower()


def _sorted_payloads(values: Iterable[dict[str, Any]]) -> list[dict[str, Any]]:
    return sorted(values, key=canonical_manifest_bytes)


def _assembly_module():
    import gamexxk_assemble_qingshan_dress_b1 as assembly
    return assembly


def _protected_file_manifest(config: dict[str, Any]) -> dict[str, str]:
    if config["protected_files"] != APPROVED_PROTECTED_FILE_HASHES:
        raise RuntimeError("protected file contract differs from the three approved hashes")
    observed = validate_protected_file_hashes(config["protected_files"], PROJECT_ROOT)
    if observed != APPROVED_PROTECTED_FILE_HASHES:
        raise RuntimeError("protected raw hashes differ from approved values")
    return dict(sorted(observed.items()))


def _quickroad_manifest(config: dict[str, Any], anchor_transform) -> dict[str, Any]:
    assembly = _assembly_module()
    landscape = config["landscape"]
    raw = _operation_payload(
        "audit_infrastructure",
        unreal.Quick_RoadAutomationLibrary.audit_infrastructure(
            landscape["label"],
            unreal.Name(landscape["edit_layer"]),
            unreal.Name(config["quickroad"]["combined_tag_expression"]),
        ),
    )
    center = assembly.local_to_world(anchor_transform, landscape["center_local_cm"])
    verified = assembly._validate_quickroad_audit(
        raw, config, center, require_active=False)
    source_records = []
    for record in raw["source_records"]:
        source_records.append({
            "road_tag": str(record["road_tag"]),
            "spline_count": int(record["spline_count"]),
            "road_widths_cm": [int(value) for value in record["road_widths_cm"]],
            "network_material_paths": sorted(
                _package_path(path) for path in record["network_material_paths"]
            ),
        })
    base_height_digest = str(raw.get("base_height_u16_sha256", ""))
    merged_height_digest = str(raw.get("merged_height_u16_sha256", ""))
    for field, digest in (
        ("base_height_u16_sha256", base_height_digest),
        ("merged_height_u16_sha256", merged_height_digest),
    ):
        if len(digest) != 64 or any(
                character not in "0123456789abcdef" for character in digest):
            raise RuntimeError(f"QuickRoad audit {field} is invalid")
    height_bounds = [int(value) for value in raw.get("height_bounds", [])]
    if len(height_bounds) != 4:
        raise RuntimeError("QuickRoad audit height_bounds is invalid")
    heightmap_source_path = _project_relative_manifest_path(
        landscape["heightmap_source"])
    expected_heightmap_source = (PROJECT_ROOT / heightmap_source_path).resolve()
    live_heightmap_source = Path(str(raw["heightmap_source_path"])).resolve()
    if str(live_heightmap_source).casefold() != str(expected_heightmap_source).casefold():
        raise RuntimeError("QuickRoad audit heightmap source path mismatch")
    return {
        "source_records": sorted(source_records, key=lambda item: item["road_tag"]),
        "network_count": int(raw["network_count"]),
        "road_triangle_count": int(raw["road_triangle_count"]),
        "intersection_patch_count": int(raw["intersection_patch_count"]),
        "network_material_paths": sorted(
            _package_path(path) for path in raw["network_material_paths"]
        ),
        "edit_layer": str(raw["edit_layer"]),
        "edit_layer_active": bool(raw["edit_layer_active"]),
        "edit_layer_visible": bool(raw["edit_layer_visible"]),
        "edit_layer_locked": bool(raw["edit_layer_locked"]),
        "non_neutral_sample_count": int(raw["non_neutral_sample_count"]),
        "edit_layer_delta_sha256": str(raw["edit_layer_delta_sha256"]),
        "base_height_u16_sha256": base_height_digest,
        "merged_height_u16_sha256": merged_height_digest,
        "landscape_component_count": int(raw["landscape_component_count"]),
        "component_size_quads": int(raw["component_size_quads"]),
        "num_subsections": int(raw["num_subsections"]),
        "subsection_size_quads": int(raw["subsection_size_quads"]),
        "heightmap_source_path": heightmap_source_path,
        "height_bounds": height_bounds,
        "edge_actor_labels": list(raw["edge_actor_labels"]),
        "edge_spline_count": int(raw["edge_spline_count"]),
        "edge_geometry_sha256": str(raw["edge_geometry_sha256"]),
        "reconstructed_grid_center_world": [
            _finite(value, "quickroad.grid_center")
            for value in raw["reconstructed_grid_center_world"]
        ],
        "quickroad_status": verified["quickroad_status"],
    }


def _graph_contract(status: dict[str, Any], group_key: str) -> dict[str, Any]:
    description = str(status.get("graph_contract", ""))
    if not description.startswith(GRAPH_CONTRACT_PREFIX):
        raise RuntimeError(f"{group_key} has no advanced PCG graph contract")
    return _json_object(description[len(GRAPH_CONTRACT_PREFIX):], f"{group_key}.graph_contract")


def _pcg_component(volume):
    component = _read_property(volume, "pcg_component", required=False)
    if component is None and hasattr(unreal, "PCGComponent"):
        component = volume.get_component_by_class(unreal.PCGComponent)
    if component is None:
        raise RuntimeError(f"PCG volume {_actor_label(volume)} has no PCGComponent")
    return component


def _live_graph_path(volume, expected_path: str, status_contract: str) -> str:
    component = _pcg_component(volume)
    graph = None
    getter = getattr(component, "get_graph", None)
    if callable(getter):
        graph = getter()
    if graph is None:
        graph_instance = _read_property(component, ("graph_instance", "graph"), required=False)
        nested_getter = getattr(graph_instance, "get_graph", None)
        graph = nested_getter() if callable(nested_getter) else graph_instance
    if graph is None:
        graph = unreal.EditorAssetLibrary.load_asset(expected_path)
        description = str(_read_property(graph, "description")) if graph is not None else ""
        if description != status_contract:
            raise RuntimeError(f"cannot resolve live graph path for {_actor_label(volume)}")
    path = _object_path(graph)
    if path != expected_path or not path.startswith(B1_GRAPH_ROOT):
        raise RuntimeError(f"live PCG graph path mismatch: {path} != {expected_path}")
    return path


def _trace_ground(world_location, actors_to_ignore: list[Any]):
    world = unreal.EditorLevelLibrary.get_editor_world()
    start = unreal.Vector(world_location.x, world_location.y, world_location.z + 50000.0)
    end = unreal.Vector(world_location.x, world_location.y, world_location.z - 50000.0)
    result = unreal.SystemLibrary.line_trace_single(
        world,
        start,
        end,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        True,
        actors_to_ignore,
        unreal.DrawDebugTrace.NONE,
        True,
    )
    if result is None:
        raise RuntimeError(f"ground trace missed at {_vector_payload(world_location)}")
    try:
        payload = result.to_dict()
    except Exception as error:
        raise RuntimeError("ground HitResult does not expose UE 5.8 to_dict") from error
    if not isinstance(payload, dict) or "impact_point" not in payload:
        raise RuntimeError(f"ground HitResult omitted impact_point: {payload!r}")
    return payload["impact_point"]


def _component_material_paths(component) -> list[str]:
    count = int(component.get_num_materials())
    result = []
    for index in range(count):
        material = component.get_material(index)
        path = _object_path(material)
        if not path:
            raise RuntimeError(f"generated component has an empty material slot: {component}")
        result.append(path)
    return result


def _primitive_cull_end(component, field: str) -> float:
    values = []
    for property_name in ("cached_max_draw_distance", "ld_max_draw_distance"):
        value = _read_property(component, property_name, required=False)
        if value is not None:
            values.append(_finite(value, f"{field}.{property_name}"))
    if not values:
        raise RuntimeError(f"{field} exposes no primitive cull distance")
    return max(values)


def _pcg_group_components(
        group: dict[str, Any], volume, expected_material_paths: list[str],
        actors_to_ignore: list[Any]) -> tuple[list[dict[str, Any]], int, float]:
    components = list(volume.get_components_by_class(unreal.InstancedStaticMeshComponent))
    if not components:
        raise RuntimeError(f"{group['key']} has no generated ISM/HISM component")
    expected_collision = (
        unreal.CollisionEnabled.QUERY_AND_PHYSICS
        if group["collision_enabled"] else unreal.CollisionEnabled.NO_COLLISION
    )
    expected_start = int(float(group["cull_end_cm"]) * 0.7)
    expected_end = int(float(group["cull_end_cm"]))
    result = []
    total_instances = 0
    maximum_bottom_error = 0.0
    for component in components:
        mesh = _read_property(component, "static_mesh")
        mesh_path = _object_path(mesh)
        if mesh_path != group["mesh_path"]:
            raise RuntimeError(f"{group['key']} live mesh mismatch: {mesh_path}")
        material_paths = _component_material_paths(component)
        if material_paths != expected_material_paths:
            raise RuntimeError(
                f"{group['key']} live material mismatch: {material_paths}"
            )
        collision = component.get_collision_enabled()
        if collision != expected_collision:
            raise RuntimeError(f"{group['key']} collision policy mismatch")
        cull_start = int(_read_property(
            component, ("instance_start_cull_distance", "start_cull_distance")))
        cull_end = int(_read_property(
            component, ("instance_end_cull_distance", "end_cull_distance")))
        if (cull_start, cull_end) != (expected_start, expected_end):
            raise RuntimeError(
                f"{group['key']} cull mismatch: {(cull_start, cull_end)}"
            )
        instance_count = int(component.get_instance_count())
        transforms = []
        bounds = mesh.get_bounds()
        local_bottom_z = float(bounds.origin.z) - float(bounds.box_extent.z)
        for index in range(instance_count):
            transform = component.get_instance_transform(index, world_space=True)
            if transform is None:
                raise RuntimeError(
                    f"{group['key']} instance transform {index} is unavailable"
                )
            transforms.append(_transform_payload(transform, f"{group['key']}.instance"))
            if group["category"] == "building":
                bottom = unreal.MathLibrary.transform_location(
                    transform, unreal.Vector(0.0, 0.0, local_bottom_z))
                ground = _trace_ground(bottom, actors_to_ignore)
                maximum_bottom_error = max(
                    maximum_bottom_error, abs(float(bottom.z) - float(ground.z)))
        total_instances += instance_count
        result.append({
            "mesh_path": mesh_path,
            "material_paths": material_paths,
            "collision_enabled": _enum_name(collision),
            "cull_distances_cm": [cull_start, cull_end],
            "generated_component_tags": _component_tags(component),
            "instance_count": instance_count,
            "transforms": _sorted_payloads(transforms),
        })
    return _sorted_payloads(result), total_instances, maximum_bottom_error


def _pcg_manifest(
        config: dict[str, Any], quickroad: dict[str, Any]) -> tuple[dict[str, Any], float]:
    assembly = _assembly_module()
    groups = assembly.build_pcg_group_specs(config)
    if len(groups) != EXPECTED_GRAPH_COUNT:
        raise RuntimeError(f"expected 34 PCG graph groups, got {len(groups)}")
    volumes = {
        group["key"]: _find_unique_actor(group["volume_label"], unreal.PCGVolume)
        for group in groups
    }
    ignore = list(volumes.values())
    category_counts = {key: 0 for key in EXPECTED_GROUP_COUNTS}
    population_counts = {
        "buildings": 0, "props": 0, "static_plants": 0, "mountains": 0,
    }
    population_key = {
        "building": "buildings",
        "prop": "props",
        "static_plant": "static_plants",
        "mountain": "mountains",
    }
    graph_records = []
    maximum_bottom_error = 0.0
    live_archetypes = set()
    live_palettes = set()
    live_roof_materials = set()
    for group in groups:
        volume = volumes[group["key"]]
        if B1_MANAGED_TAG not in _actor_tags(volume):
            raise RuntimeError(f"PCG volume lacks B1 ownership: {group['volume_label']}")
        status = _operation_payload(
            "get_town_pcg_status",
            unreal.GameXXKTownPCGAutomationLibrary.get_town_pcg_status(
                group["volume_label"]),
        )
        if status.get("generating") is True or status.get("generated") is not True:
            raise RuntimeError(f"PCG group is not complete: {group['key']}")
        contract = _graph_contract(status, group["key"])
        status_contract = str(status["graph_contract"])
        graph_path = _live_graph_path(volume, group["graph_path"], status_contract)
        mesh_asset = unreal.EditorAssetLibrary.load_asset(group["mesh_path"])
        if mesh_asset is None:
            raise RuntimeError(f"missing live PCG mesh: {group['mesh_path']}")
        expected_material_paths = assembly._mesh_material_override_paths(
            mesh_asset, group["slot_materials"])
        expected_material_paths = [_package_path(path) for path in expected_material_paths]
        expected_contract = {
            "static_mesh_path": group["mesh_path"],
            "point_count": len(group["records"]),
            "base_seed": int(group["base_seed"]),
            "road_edge_geometry_digest": quickroad["edge_geometry_sha256"],
            "consumed_road_edge_actor_labels": sorted(quickroad["edge_actor_labels"]),
            "material_override_paths": expected_material_paths,
        }
        for field, expected in expected_contract.items():
            if contract.get(field) != expected:
                raise RuntimeError(
                    f"{group['key']} contract {field} mismatch: "
                    f"{contract.get(field)!r} != {expected!r}"
                )
        point_seed_sha256 = str(contract.get("point_seed_sha256", ""))
        if len(point_seed_sha256) != 64 or any(
                character not in "0123456789abcdef" for character in point_seed_sha256):
            raise RuntimeError(f"{group['key']} point seed digest is invalid")
        if int(status.get("component_seed", -1)) != int(group["component_seed"]):
            raise RuntimeError(f"{group['key']} component seed mismatch")
        component_records, instance_count, bottom_error = _pcg_group_components(
            group, volume, expected_material_paths, ignore)
        if int(status.get("generated_component_count", -1)) != len(component_records):
            raise RuntimeError(f"{group['key']} generated component count mismatch")
        if instance_count != len(group["records"]):
            raise RuntimeError(f"{group['key']} instance count mismatch")
        maximum_bottom_error = max(maximum_bottom_error, bottom_error)
        category_counts[group["category"]] += 1
        population_counts[population_key[group["category"]]] += instance_count
        if group["category"] == "building":
            archetypes = {record["archetype_id"] for record in group["records"]}
            palettes = {record["roof_palette"] for record in group["records"]}
            live_archetypes.update(archetypes)
            live_palettes.update(palettes)
            live_roof_materials.update(
                path for path in expected_material_paths
                if path in config["building_materials"]["roof_palette_materials"].values()
            )
        graph_records.append({
            "key": group["key"],
            "category": group["category"],
            "volume_label": group["volume_label"],
            "graph_path": graph_path,
            "base_seed": int(contract["base_seed"]),
            "component_seed": int(status["component_seed"]),
            "point_seed_sha256": point_seed_sha256,
            "point_count": int(contract["point_count"]),
            "generated_component_count": int(status["generated_component_count"]),
            "instance_count": instance_count,
            "consumed_edge_labels": list(contract["consumed_road_edge_actor_labels"]),
            "consumed_edge_digest": str(contract["road_edge_geometry_digest"]),
            "mesh_path": str(contract["static_mesh_path"]),
            "material_paths": list(contract["material_override_paths"]),
            "collision_enabled": bool(group["collision_enabled"]),
            "cull_distances_cm": [
                int(float(group["cull_end_cm"]) * 0.7),
                int(float(group["cull_end_cm"])),
            ],
            "volume_transform": _transform_payload(
                volume.get_actor_transform(), f"{group['key']}.volume"),
            "components": component_records,
        })
    if category_counts != EXPECTED_GROUP_COUNTS:
        raise RuntimeError(f"PCG group counts mismatch: {category_counts}")
    if population_counts != {key: EXPECTED_COUNTS[key] for key in population_counts}:
        raise RuntimeError(f"PCG population counts mismatch: {population_counts}")
    expected_roofs = set(config["building_materials"]["roof_palette_materials"].values())
    if (
        len(live_archetypes) != EXPECTED_ARCHETYPE_COUNT
        or len(live_palettes) != EXPECTED_ROOF_PALETTE_COUNT
        or live_roof_materials != expected_roofs
    ):
        raise RuntimeError("live building archetype/roof palette coverage mismatch")
    if maximum_bottom_error > MAX_BUILDING_BOTTOM_ERROR_CM:
        raise RuntimeError(
            f"building_bottom_error_cm exceeds {MAX_BUILDING_BOTTOM_ERROR_CM}: "
            f"{maximum_bottom_error}"
        )
    return {
        "group_count": len(graph_records),
        "group_counts": category_counts,
        "population_counts": population_counts,
        "archetypes": sorted(live_archetypes),
        "roof_palettes": sorted(live_palettes),
        "live_roof_material_paths": sorted(live_roof_materials),
        "graphs": sorted(graph_records, key=lambda item: item["key"]),
    }, maximum_bottom_error


def _animated_plant_phase_snapshot(records, assembly):
    expected_labels = {f"QS_B1_{record['id']}" for record in records}
    all_actors = _all_actors()
    live_labels = set()
    matches = {label: [] for label in expected_labels}
    for actor in all_actors:
        label = _actor_label(actor)
        tags = _actor_tags(actor)
        if B1_MANAGED_TAG in tags and "QingshanB1AnimatedPlant" in tags:
            live_labels.add(label)
        if label in matches:
            matches[label].append(actor)
    if live_labels != expected_labels:
        raise RuntimeError("live PaperFlipbookActor labels mismatch")
    actor_components = {}
    for label in sorted(expected_labels):
        actors = matches[label]
        if len(actors) != 1 or not isinstance(actors[0], unreal.PaperFlipbookActor):
            raise RuntimeError(
                f"animated plant label must resolve to one PaperFlipbookActor: {label}"
            )
        actor = actors[0]
        actor_components[label] = (actor, assembly._flipbook_component(actor))
    phase_snapshot = {
        label: float(component.get_playback_position())
        for label, (_actor, component) in actor_components.items()
    }
    return actor_components, phase_snapshot


def _animated_plant_manifest(config: dict[str, Any], anchor_transform) -> dict[str, Any]:
    assembly = _assembly_module()
    records = sorted(
        (
            record for record in config["vegetation_records"]
            if record["render_mode"] == "animated_flipbook"
        ),
        key=lambda item: item["id"],
    )
    if len(records) != EXPECTED_COUNTS["animated_plants"]:
        raise RuntimeError("animated plant config count mismatch")
    actor_components, phase_snapshot = _animated_plant_phase_snapshot(
        records, assembly)
    plants = []
    for record in records:
        label = f"QS_B1_{record['id']}"
        actor, component = actor_components[label]
        flipbook = component.get_flipbook()
        if _object_path(flipbook) != config["asset_catalog"]["plant_flipbook"]:
            raise RuntimeError(f"{label} does not use FB_QS_B1_Plant_Sway")
        collision = component.get_collision_enabled()
        if collision != unreal.CollisionEnabled.NO_COLLISION:
            raise RuntimeError(f"{label} Paper2D collision must be disabled")
        cull = _primitive_cull_end(component, f"{label}.cull")
        if abs(cull - 12000.0) > 0.1:
            raise RuntimeError(f"{label} Paper2D cull distance mismatch")
        expected_xy = assembly.local_to_world(anchor_transform, record["location_cm"])
        actual_location = actor.get_actor_location()
        if math.hypot(
                float(actual_location.x) - float(expected_xy.x),
                float(actual_location.y) - float(expected_xy.y)) > 0.1:
            raise RuntimeError(f"{label} XY transform drift")
        expected_yaw = assembly._anchor_local_yaw(
            anchor_transform, float(record["yaw_degrees"]))
        if _angular_difference(actor.get_actor_rotation().yaw, expected_yaw) > 0.1:
            raise RuntimeError(f"{label} yaw drift")
        scale = actor.get_actor_scale3d()
        if any(abs(float(value) - float(record["scale"])) > 0.001 for value in (
                scale.x, scale.y, scale.z)):
            raise RuntimeError(f"{label} scale drift")
        looping_getter = getattr(component, "is_looping", None)
        playing_getter = getattr(component, "is_playing", None)
        if callable(looping_getter) and not looping_getter():
            raise RuntimeError(f"{label} is not looping")
        if callable(playing_getter) and not playing_getter():
            raise RuntimeError(f"{label} is not playing")
        play_rate = _finite(component.get_play_rate(), f"{label}.play_rate")
        if abs(play_rate - 1.0) > 1.0e-6:
            raise RuntimeError(f"{label} play rate must be exactly 1.0")
        if component.is_reversing():
            raise RuntimeError(f"{label} must play forward")
        frame_count = int(flipbook.get_num_frames())
        frames_per_second = _finite(
            component.get_flipbook_framerate(), f"{label}.frames_per_second")
        if frame_count != 6 or abs(frames_per_second - 5.0) > 1.0e-6:
            raise RuntimeError(f"{label} flipbook timing contract mismatch")
        observed_seconds = float(phase_snapshot[label])
        if not math.isfinite(observed_seconds):
            raise RuntimeError(f"{label} live flipbook phase is nonfinite")
        plants.append({
            "label": label,
            "transform": _transform_payload(actor.get_actor_transform(), label),
            "flipbook_path": _object_path(flipbook),
            "configured_start_frame": int(record["start_frame"]),
            "frame_count": frame_count,
            "frames_per_second": frames_per_second,
            "looping": True,
            "playing": True,
            "_observed_seconds": observed_seconds,
            "collision_enabled": _enum_name(collision),
            "cull_distances_cm": [0, int(cull)],
        })
    reference = plants[0]
    reference_seconds = float(reference["_observed_seconds"])
    reference_configured = int(reference["configured_start_frame"])
    for plant in plants:
        configured_delta = (
            int(plant["configured_start_frame"]) - reference_configured
        ) % int(plant["frame_count"])
        observed_seconds = float(plant.pop("_observed_seconds"))
        phase_error_frames = _circular_phase_error_frames(
            reference_seconds=reference_seconds,
            observed_seconds=observed_seconds,
            configured_delta_frames=configured_delta,
            frame_count=int(plant["frame_count"]),
            frames_per_second=float(plant["frames_per_second"]),
        )
        if phase_error_frames > 0.25:
            raise RuntimeError(
                f"{plant['label']} live Paper2D relative phase mismatch: "
                f"error={phase_error_frames} frames, configured={configured_delta}"
            )
        # Canonicalize only the verified configured phase class; live float time is transient.
        plant["live_relative_phase_delta"] = configured_delta
    return {"count": len(plants), "actors": plants}


def _camera_manifest(config: dict[str, Any], anchor_transform) -> dict[str, Any]:
    assembly = _assembly_module()
    records = {record["id"]: record for record in config["cameras"]}
    if set(records) != set(EXPECTED_CAMERA_LABELS):
        raise RuntimeError("camera config labels mismatch")
    cameras = []
    for label in EXPECTED_CAMERA_LABELS:
        record = records[label]
        actor = _find_unique_actor(label, unreal.CameraActor)
        if B1_MANAGED_TAG not in _actor_tags(actor):
            raise RuntimeError(f"review camera lacks B1 ownership: {label}")
        location = assembly.local_to_world(anchor_transform, record["location_cm"])
        target = assembly.local_to_world(anchor_transform, record["target_cm"])
        rotation = unreal.MathLibrary.find_look_at_rotation(location, target)
        _assert_vector_close(actor.get_actor_location(), location, f"{label}.location")
        _assert_rotator_close(actor.get_actor_rotation(), rotation, f"{label}.rotation")
        component = assembly._camera_component(actor)
        fov = _finite(_read_property(component, "field_of_view"), f"{label}.fov")
        if abs(fov - float(record["fov_degrees"])) > 0.01:
            raise RuntimeError(f"{label} field of view mismatch")
        cameras.append({
            "label": label,
            "transform": _transform_payload(actor.get_actor_transform(), label),
            "target_world": _vector_payload(target, f"{label}.target"),
            "field_of_view": fov,
        })
    if len(cameras) != EXPECTED_CAMERA_COUNT:
        raise RuntimeError("camera count mismatch")
    return {"count": len(cameras), "actors": cameras}


def _actor_bounds(actor) -> tuple[Any, Any]:
    result = actor.get_actor_bounds(False)
    if not isinstance(result, tuple) or len(result) != 2:
        raise RuntimeError(f"could not read bounds for {_actor_label(actor)}")
    return result


def _semantic_mesh_record(
        actor, *, collision_enabled: bool, cull_end_cm: int) -> dict[str, Any]:
    components = list(actor.get_components_by_class(unreal.StaticMeshComponent))
    if len(components) != 1:
        raise RuntimeError(
            f"semantic actor {_actor_label(actor)} must own exactly one StaticMeshComponent"
        )
    component = components[0]
    expected_collision = (
        unreal.CollisionEnabled.QUERY_AND_PHYSICS
        if collision_enabled else unreal.CollisionEnabled.NO_COLLISION
    )
    collision = component.get_collision_enabled()
    if collision != expected_collision:
        raise RuntimeError(
            f"semantic collision mismatch for {_actor_label(actor)}: "
            f"actual={collision!r}/{_enum_name(collision)}, "
            f"expected={expected_collision!r}/{_enum_name(expected_collision)}, "
            f"profile={component.get_collision_profile_name()}"
        )
    cull = _primitive_cull_end(component, f"{_actor_label(actor)}.cull")
    if abs(cull - float(cull_end_cm)) > 0.1:
        raise RuntimeError(f"semantic cull mismatch for {_actor_label(actor)}: {cull}")
    mesh = _read_property(component, "static_mesh")
    return {
        "label": _actor_label(actor),
        "class": _class_path(actor),
        "tags": _actor_tags(actor),
        "transform": _transform_payload(
            actor.get_actor_transform(), _actor_label(actor)),
        "mesh_path": _object_path(mesh),
        "material_paths": _component_material_paths(component),
        "collision_enabled": _enum_name(collision),
        "cull_distances_cm": [0, int(cull)],
    }


def _bridge_river_manifest(config: dict[str, Any]) -> dict[str, Any]:
    bridge_labels = {
        "QS_B1_Bridge_Deck",
        "QS_B1_Bridge_Rail_L", "QS_B1_Bridge_Rail_R",
        *{
            f"QS_B1_Bridge_Post_{side}_{rail}"
            for side in ("W", "E") for rail in ("L", "R")
        },
        *{
            f"QS_B1_Bridge_ApproachStone_{side}_{rail}"
            for side in ("W", "E") for rail in ("L", "R")
        },
    }
    river_labels = {
        "QS_B1_River_Centerline",
        *{
            f"QS_B1_River_Segment_{index:03d}"
            for index in range(len(config["river_splines"][0]["points_cm"]) - 1)
        },
    }
    dock_labels = {"QS_B1_Dock_Deck", "QS_B1_Dock_Rail_L", "QS_B1_Dock_Rail_R"}
    if bridge_labels & river_labels or bridge_labels & dock_labels or river_labels & dock_labels:
        raise RuntimeError("bridge/river/dock label sets overlap")
    bridge = [_find_unique_actor(label) for label in sorted(bridge_labels)]
    river = [_find_unique_actor(label) for label in sorted(river_labels)]
    dock = [_find_unique_actor(label) for label in sorted(dock_labels)]
    deck = _find_unique_actor("QS_B1_Bridge_Deck")
    river_segments = [
        actor for actor in river if _actor_label(actor).startswith("QS_B1_River_Segment_")
    ]
    deck_location = deck.get_actor_location()
    deck_origin, deck_extent = _actor_bounds(deck)
    crossing_candidates = []
    for actor in river_segments:
        river_origin, river_extent = _actor_bounds(actor)
        if (
            abs(float(deck_location.x) - float(river_origin.x))
            <= float(river_extent.x) + 1.0
            and abs(float(deck_location.y) - float(river_origin.y))
            <= float(river_extent.y) + 1.0
        ):
            crossing_candidates.append((
                float(river_origin.z) + float(river_extent.z),
                actor,
                river_origin,
                river_extent,
            ))
    if not crossing_candidates:
        raise RuntimeError("bridge deck does not overlap any live river segment")
    river_top_z, crossing, river_origin, river_extent = max(
        crossing_candidates, key=lambda item: item[0])
    vertical_separation = (
        float(deck_origin.z) - float(deck_extent.z)
        - river_top_z
    )
    if vertical_separation <= 0.0:
        raise RuntimeError(
            f"bridge deck is not vertically separated from river: {vertical_separation}"
        )
    bridge_records = [
        _semantic_mesh_record(
            actor,
            collision_enabled=True,
            cull_end_cm=(
                22000 if _actor_label(actor).startswith(
                    "QS_B1_Bridge_ApproachStone_") else 30000
            ),
        )
        for actor in bridge
    ]
    river_records = []
    for actor in river:
        if _actor_label(actor) == "QS_B1_River_Centerline":
            river_records.append({
                "label": _actor_label(actor),
                "class": _class_path(actor),
                "tags": _actor_tags(actor),
                "transform": _transform_payload(
                    actor.get_actor_transform(), _actor_label(actor)),
                "collision_enabled": "no_collision",
            })
        else:
            river_records.append(_semantic_mesh_record(
                actor, collision_enabled=False, cull_end_cm=50000))
    dock_records = [
        _semantic_mesh_record(actor, collision_enabled=True, cull_end_cm=30000)
        for actor in dock
    ]
    return {
        "bridge": bridge_records,
        "river": river_records,
        "dock": dock_records,
        "crossing_river_label": _actor_label(crossing),
        "crossing_candidate_labels": sorted(
            _actor_label(item[1]) for item in crossing_candidates),
        "vertical_separation_cm": _finite(vertical_separation, "bridge_river.separation"),
    }


def _landscape_topology(landscape, quickroad: dict[str, Any]) -> dict[str, Any]:
    components = list(landscape.get_components_by_class(unreal.LandscapeComponent))
    component_size = int(quickroad["component_size_quads"])
    num_subsections = int(quickroad["num_subsections"])
    subsection_size = int(quickroad["subsection_size_quads"])
    height_bounds = [int(value) for value in quickroad["height_bounds"]]
    if len(height_bounds) != 4:
        raise RuntimeError("live Landscape height bounds are invalid")
    min_x, min_y, max_x, max_y = height_bounds
    if component_size <= 0 or max_x < min_x or max_y < min_y:
        raise RuntimeError("live Landscape topology values are invalid")
    bases = []
    for component in components:
        getter = getattr(component, "get_section_base", None)
        if callable(getter):
            base = getter()
            bases.append((int(base.x), int(base.y)))
        else:
            bases.append((
                int(_read_property(component, "section_base_x")),
                int(_read_property(component, "section_base_y")),
            ))
    if not bases:
        raise RuntimeError("Landscape has no components")
    if len(components) != int(quickroad["landscape_component_count"]):
        raise RuntimeError("live Landscape component count differs from QuickRoad audit")
    if (
        min(x for x, _ in bases) != min_x
        or min(y for _, y in bases) != min_y
        or max(x for x, _ in bases) + component_size != max_x
        or max(y for _, y in bases) + component_size != max_y
    ):
        raise RuntimeError("live Landscape component bases differ from audited height bounds")
    resolution_x = max_x - min_x + 1
    resolution_y = max_y - min_y + 1
    return {
        "resolution": [resolution_x, resolution_y],
        "component_count": len(components),
        "component_size_quads": component_size,
        "num_subsections": num_subsections,
        "subsection_size_quads": subsection_size,
        "height_bounds": height_bounds,
        "component_section_bases": [[x, y] for x, y in sorted(bases)],
    }


def _heightmap_manifest(config: dict[str, Any]) -> dict[str, Any]:
    landscape_config = config["landscape"]
    source = (PROJECT_ROOT / landscape_config["heightmap_source"]).resolve()
    decoded = decode_png16_grayscale(source)
    if [decoded["width"], decoded["height"]] != [
            EXPECTED_LANDSCAPE_RESOLUTION, EXPECTED_LANDSCAPE_RESOLUTION]:
        raise RuntimeError("base Landscape heightmap is not 505 x 505")
    samples_path = source.with_suffix(".samples.json")
    samples = _json_object(samples_path.read_text(encoding="utf-8"), "heightmap samples")
    if samples.get("png_sha256") != decoded["png_sha256"]:
        raise RuntimeError("heightmap PNG hash differs from approved sample metadata")
    scale_z = float(landscape_config["scale_cm"][2])
    source_little_endian = b"".join(
        int(value).to_bytes(2, "little") for value in decoded["values"]
    )
    decoded_samples = {}
    for name, sample in sorted(samples["named_samples"].items()):
        x, y = map(int, sample["pixel_xy"])
        raw = decoded["values"][y * decoded["width"] + x]
        elevation = (raw - 32768) * scale_z / 128.0
        if raw != int(sample["raw"]) or abs(
                elevation - float(sample["decoded_elevation_cm"])) > 1.0e-7:
            raise RuntimeError(f"heightmap decoded sample mismatch: {name}")
        decoded_samples[name] = {
            "pixel_xy": [x, y],
            "raw_u16": raw,
            "decoded_elevation_cm": _finite(elevation, f"heightmap.{name}"),
        }
    return {
        "source_png": landscape_config["heightmap_source"],
        "source_png_sha256": decoded["png_sha256"],
        "source_base_height_u16_big_endian_sha256": decoded["u16_sha256"],
        "source_base_height_u16_sha256": hashlib.sha256(
            source_little_endian).hexdigest(),
        "decoded_sample_elevations_cm": {
            name: value["decoded_elevation_cm"]
            for name, value in decoded_samples.items()
        },
        "decoded_samples": decoded_samples,
    }


def _landscape_manifest(
        config: dict[str, Any], anchor_transform, quickroad: dict[str, Any],
        building_bottom_error_cm: float) -> dict[str, Any]:
    assembly = _assembly_module()
    landscape_config = config["landscape"]
    landscape = _find_unique_actor(landscape_config["label"], unreal.Landscape)
    topology = _landscape_topology(landscape, quickroad)
    if topology["resolution"] != [
            EXPECTED_LANDSCAPE_RESOLUTION, EXPECTED_LANDSCAPE_RESOLUTION]:
        raise RuntimeError(f"live Landscape resolution mismatch: {topology['resolution']}")
    if topology["component_count"] != EXPECTED_LANDSCAPE_COMPONENT_COUNT:
        raise RuntimeError("live Landscape component count mismatch")
    if (
        topology["component_size_quads"] != 126
        or topology["num_subsections"] != 2
        or topology["subsection_size_quads"] != 63
    ):
        raise RuntimeError(f"live Landscape topology mismatch: {topology}")
    source = (
        PROJECT_ROOT
        / _project_relative_manifest_path(quickroad["heightmap_source_path"])
    ).resolve()
    expected_source = (
        PROJECT_ROOT
        / _project_relative_manifest_path(landscape_config["heightmap_source"])
    ).resolve()
    if str(source).casefold() != str(expected_source).casefold():
        raise RuntimeError("live Landscape source PNG path mismatch")
    material_path = _object_path(_read_property(landscape, "landscape_material"))
    if material_path != landscape_config["ground_material"]:
        raise RuntimeError("live Landscape material mismatch")
    center = landscape.get_actor_transform().transform_location(
        unreal.Vector(
            (topology["resolution"][0] - 1) * 0.5,
            (topology["resolution"][1] - 1) * 0.5,
            0.0,
        ))
    expected_center = assembly.local_to_world(
        anchor_transform, landscape_config["center_local_cm"])
    _assert_vector_close(center, expected_center, "Landscape grid centre", 1.0)
    source_heights = _heightmap_manifest(config)
    if (
        quickroad["base_height_u16_sha256"]
        != source_heights["source_base_height_u16_sha256"]
    ):
        raise RuntimeError(
            "live Landscape base height digest mismatch: "
            f"{quickroad['base_height_u16_sha256']} != "
            f"{source_heights['source_base_height_u16_sha256']}"
        )
    return {
        **topology,
        "transform": _transform_payload(landscape.get_actor_transform(), "Landscape"),
        "reconstructed_grid_center_world": _vector_payload(center, "Landscape.center"),
        "material_path": material_path,
        "edit_layer": quickroad["edit_layer"],
        "edit_layer_active": quickroad["edit_layer_active"],
        "edit_layer_visible": quickroad["edit_layer_visible"],
        "edit_layer_locked": quickroad["edit_layer_locked"],
        "edit_layer_delta_sha256": quickroad["edit_layer_delta_sha256"],
        "base_height_u16_sha256": quickroad["base_height_u16_sha256"],
        "merged_height_u16_sha256": quickroad["merged_height_u16_sha256"],
        "building_bottom_error_cm": _finite(
            building_bottom_error_cm, "building_bottom_error_cm"),
        **source_heights,
    }


def _managed_actor_manifest() -> list[dict[str, Any]]:
    records = []
    for actor in _all_actors():
        tags = _actor_tags(actor)
        if B1_MANAGED_TAG not in tags and QUICKROAD_OWNER_TAG not in tags:
            continue
        records.append({
            "label": _actor_label(actor),
            "class": _class_path(actor),
            "tags": tags,
            "transform": _transform_payload(actor.get_actor_transform(), _actor_label(actor)),
        })
    labels = [record["label"] for record in records]
    if len(labels) != len(set(labels)):
        raise RuntimeError("managed actor labels are not unique")
    return sorted(records, key=lambda item: item["label"])


def _performance_counts() -> dict[str, int]:
    actors = _all_actors()
    component_count = 0
    actor_tick_enabled_count = 0
    component_tick_enabled_count = 0
    for actor in actors:
        actor_tick = getattr(actor, "is_actor_tick_enabled", None)
        if callable(actor_tick) and actor_tick():
            actor_tick_enabled_count += 1
        components = list(actor.get_components_by_class(unreal.ActorComponent))
        component_count += len(components)
        for component in components:
            component_tick = getattr(component, "is_component_tick_enabled", None)
            if callable(component_tick) and component_tick():
                component_tick_enabled_count += 1
    return {
        "actor_count": len(actors),
        "component_count": component_count,
        "actor_tick_enabled_count": actor_tick_enabled_count,
        "component_tick_enabled_count": component_tick_enabled_count,
        "tick_enabled_count": actor_tick_enabled_count + component_tick_enabled_count,
    }


def build_live_manifest() -> dict[str, Any]:
    _require_unreal()
    world = _read_only_b1_world_guard()
    config = load_config()
    protected_file_hashes = _protected_file_manifest(config)
    assembly = _assembly_module()
    gate = _find_unique_actor("QingshanInn_TownExit")
    anchor_transform = assembly._validate_north_gate_anchor(gate)
    quickroad = _quickroad_manifest(config, anchor_transform)
    pcg, building_bottom_error_cm = _pcg_manifest(config, quickroad)
    animated_plants = _animated_plant_manifest(config, anchor_transform)
    population_counts = dict(pcg["population_counts"])
    population_counts["animated_plants"] = animated_plants["count"]
    if population_counts != EXPECTED_COUNTS:
        raise RuntimeError(f"live population counts mismatch: {population_counts}")
    manifest = {
        "schema_version": 1,
        "map": world,
        "protected_file_hashes": protected_file_hashes,
        "population_counts": population_counts,
        "pcg": pcg,
        "animated_plants": animated_plants,
        "quickroad": quickroad,
        "cameras": _camera_manifest(config, anchor_transform),
        "bridge_river": _bridge_river_manifest(config),
        "landscape": _landscape_manifest(
            config, anchor_transform, quickroad, building_bottom_error_cm),
        "managed_actors": _managed_actor_manifest(),
        "performance": _performance_counts(),
    }
    canonical_manifest_bytes(manifest)
    return manifest


def validate_live_b1() -> dict[str, Any]:
    manifest = build_live_manifest()
    live_scene_manifest_sha256 = canonical_manifest_sha256(manifest)
    return {
        "success": True,
        "error": "",
        "live_scene_manifest_sha256": live_scene_manifest_sha256,
        "manifest": manifest,
        "actor_count": manifest["performance"]["actor_count"],
        "component_count": manifest["performance"]["component_count"],
        "tick_enabled_count": manifest["performance"]["tick_enabled_count"],
    }


def main() -> None:
    try:
        result = validate_live_b1()
    except Exception as error:
        if unreal is not None:
            unreal.log_error(f"GameXXK Qingshan B1 validation failed: {error}")
        result = {
            "success": False,
            "error": str(error),
            "live_scene_manifest_sha256": "",
        }
    print(f"{VALIDATION_MARKER} {canonical_manifest_bytes(result).decode('utf-8')}")


if __name__ == "__main__":
    main()
