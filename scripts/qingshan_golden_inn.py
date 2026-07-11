"""Deterministic Gate 1 contract helpers for the Qingshan golden inn.

This module deliberately operates on JSON and files only.  Blender generation is
kept in a separate layer so the contract and evidence checks can run in ordinary
Python unit tests.
"""

from __future__ import annotations

import hashlib
import json
import math
import os
import struct
import zlib
from collections.abc import Mapping
from pathlib import Path
from typing import Any


class GoldenInnError(ValueError):
    """Raised when the golden inn contract or Gate 1 evidence is invalid."""


ASSET_ID = "BLD_QS_M_A_INN"
VERSIONS = ("v001", "v002", "v003")
REQUIRED_VIEWS = ("hero_3q", "front", "player_camera")

_TARGET_DIMENSIONS_M = (4.8, 5.6, 5.2)
_OUTPUT_ROOT = "assets/BLD_QS_M_A_INN/source/blender"
_CATALOG_ROOT = Path("SourceAssets/TownPCG/QingshanEnvironment")
_REQUIRED_MODULES = ("BODY", "ROOF_A", "DOOR_A", "WINDOW_A", "EAVE_A")
_GROUND_TOLERANCE_M = 0.001
# Gate 1 permits five centimeters of dimensional drift but only one centimeter
# of XY pivot drift from the contract's bottom-center origin.
_BOUNDS_TOLERANCE_M = 0.05
_PIVOT_CENTER_TOLERANCE_M = 0.01
_APPROVED_QUAD_RANGE = (30_000, 35_000)
_PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def _is_finite_number(value: Any) -> bool:
    if type(value) not in (int, float):
        return False
    try:
        return math.isfinite(float(value))
    except (OverflowError, TypeError, ValueError):
        return False


def _require_mapping(value: Any, field: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise GoldenInnError(f"{field} must be a mapping")
    return value


def _path_value(value: Any, field: str) -> Path:
    try:
        path = Path(value)
    except (OSError, RuntimeError, TypeError, ValueError, OverflowError) as exc:
        raise GoldenInnError(f"{field} must be a valid path-like value") from exc
    if "\0" in str(path):
        raise GoldenInnError(f"{field} must not contain a null character")
    return path


def _resolved_path(path: Path, field: str) -> Path:
    try:
        return path.resolve()
    except (OSError, RuntimeError, ValueError) as exc:
        raise GoldenInnError(f"{field} could not be resolved: {exc}") from exc


def _native_path_parts(parts: tuple[str, ...]) -> tuple[str, ...]:
    return tuple(os.path.normcase(part) for part in parts)


def _numeric_range(
    mapping: Mapping[str, Any], field: str, *, exact: tuple[float, float] | None = None
) -> tuple[float, float]:
    value = mapping.get(field)
    if (
        not isinstance(value, list)
        or len(value) != 2
        or not all(_is_finite_number(item) for item in value)
        or value[0] >= value[1]
    ):
        raise GoldenInnError(f"{field} must be a two-value increasing finite numeric range")
    result = (float(value[0]), float(value[1]))
    if exact is not None and result != exact:
        raise GoldenInnError(f"{field} must be exactly {list(exact)}")
    return result


def _validate_contract_parts(
    asset: Mapping[str, Any], golden: Mapping[str, Any]
) -> None:
    if asset.get("asset_id") != ASSET_ID:
        raise GoldenInnError(f"asset_id must be exactly {ASSET_ID}")

    dimensions = asset.get("target_dimensions_m")
    if (
        not isinstance(dimensions, list)
        or len(dimensions) != 3
        or not all(_is_finite_number(value) for value in dimensions)
        or tuple(float(value) for value in dimensions) != _TARGET_DIMENSIONS_M
    ):
        raise GoldenInnError(f"target_dimensions_m must be exactly {list(_TARGET_DIMENSIONS_M)}")

    for field, allowed in (
        ("max_window_divisions", (2, 3)),
        ("max_capital_layers", (1, 2)),
    ):
        value = golden.get(field)
        if type(value) is not int or value not in allowed:
            raise GoldenInnError(f"{field} must be one of {list(allowed)}")

    if golden.get("workflow") != "golden_sample_gate1":
        raise GoldenInnError("workflow must be exactly golden_sample_gate1")

    _numeric_range(golden, "deformation_percent_range", exact=(5.0, 10.0))
    _numeric_range(golden, "column_lean_degrees_range")
    _numeric_range(golden, "window_frame_ratio_range")

    views = golden.get("required_render_views")
    if not isinstance(views, list) or tuple(views) != REQUIRED_VIEWS:
        raise GoldenInnError(f"required_render_views must be exactly {list(REQUIRED_VIEWS)}")

    blender_version = golden.get("blender_min_version")
    if (
        not isinstance(blender_version, list)
        or len(blender_version) != 3
        or any(type(part) is not int or part < 0 for part in blender_version)
        or tuple(blender_version) < (4, 2, 0)
    ):
        raise GoldenInnError("blender_min_version must be a three-integer list at least [4, 2, 0]")

    ground_z = golden.get("ground_contact_plane_z_m")
    if not _is_finite_number(ground_z) or float(ground_z) != 0.0:
        raise GoldenInnError("ground_contact_plane_z_m must be finite and exactly 0.0")

    if golden.get("output_root") != _OUTPUT_ROOT:
        raise GoldenInnError(f"output_root must be exactly {_OUTPUT_ROOT}")


def load_golden_contract(path: Path) -> dict:
    """Load and validate the committed golden-inn asset contract."""

    contract_path = _path_value(path, "contract path")
    try:
        raw = contract_path.read_text(encoding="utf-8")
    except UnicodeDecodeError as exc:
        raise GoldenInnError(f"contract JSON is invalid: {contract_path}: {exc}") from exc
    except (OSError, ValueError) as exc:
        raise GoldenInnError(f"contract file could not be read: {contract_path}: {exc}") from exc
    try:
        payload = json.loads(raw)
    except json.JSONDecodeError as exc:
        raise GoldenInnError(f"contract JSON is invalid: {contract_path}: {exc}") from exc

    asset = _require_mapping(payload, "asset")
    building = _require_mapping(asset.get("building"), "building")
    golden = _require_mapping(building.get("golden_contract"), "building.golden_contract")
    _validate_contract_parts(asset, golden)
    return {"asset": asset, "golden": golden}


def _validate_component_plan(plan: Mapping[str, Any], golden: Mapping[str, Any]) -> None:
    deformation_min, deformation_max = _numeric_range(golden, "deformation_percent_range")
    deformation_percent = float(plan["asymmetry"]) * 100.0
    if not deformation_min <= deformation_percent <= deformation_max:
        raise GoldenInnError("asymmetry falls outside deformation_percent_range")

    frame_min, frame_max = _numeric_range(golden, "window_frame_ratio_range")
    if not frame_min <= float(plan["window_frame_ratio"]) <= frame_max:
        raise GoldenInnError("window_frame_ratio falls outside window_frame_ratio_range")

    if plan["window_divisions"] > golden["max_window_divisions"]:
        raise GoldenInnError("window_divisions exceeds max_window_divisions")
    if plan["capital_layers"] > golden["max_capital_layers"]:
        raise GoldenInnError("capital_layers exceeds max_capital_layers")
    if float(plan["ground_z_m"]) != float(golden["ground_contact_plane_z_m"]):
        raise GoldenInnError("ground_z_m differs from ground_contact_plane_z_m")

    lean_min, lean_max = _numeric_range(golden, "column_lean_degrees_range")
    dimensions = plan["dimensions_m"]
    half_x, half_y = float(dimensions[0]) / 2.0, float(dimensions[1]) / 2.0
    lean_values: list[float] = []
    for column in plan["columns"]:
        lean = float(column["lean_deg"])
        lean_values.append(lean)
        if not lean_min <= abs(lean) <= lean_max:
            raise GoldenInnError(
                f"{column['name']} lean_deg falls outside column_lean_degrees_range"
            )
        x, y = float(column["x_m"]), float(column["y_m"])
        if abs(x) > half_x or abs(y) > half_y:
            raise GoldenInnError(f"{column['name']} position falls outside component bounds")
    if len(set(lean_values)) != len(lean_values):
        raise GoldenInnError("column lean_deg values must be distinct")


def build_component_plan(contract: Mapping[str, Any]) -> dict:
    """Build the fixed, deterministic golden-inn component plan."""

    contract_mapping = _require_mapping(contract, "contract")
    asset = _require_mapping(contract_mapping.get("asset"), "contract.asset")
    golden = _require_mapping(contract_mapping.get("golden"), "contract.golden")
    _validate_contract_parts(asset, golden)

    plan = {
        "asset_id": ASSET_ID,
        "dimensions_m": list(_TARGET_DIMENSIONS_M),
        "ground_z_m": 0.0,
        "asymmetry": 0.075,
        "facade_offset_m": 0.16,
        "roof_ridge_offset_m": -0.22,
        "window_frame_ratio": 0.10,
        "window_divisions": 2,
        "capital_layers": 1,
        "modules": list(_REQUIRED_MODULES),
        "columns": [
            {"name": "Column_FL", "x_m": -2.0, "y_m": 2.45, "lean_deg": -5.5},
            {"name": "Column_FR", "x_m": 1.86, "y_m": 2.45, "lean_deg": 4.5},
            {"name": "Column_BL", "x_m": -2.0, "y_m": -2.4, "lean_deg": -4.0},
            {"name": "Column_BR", "x_m": 1.86, "y_m": -2.4, "lean_deg": 6.0},
        ],
    }
    _validate_component_plan(plan, golden)
    return plan


def output_paths(project_root: Path, version: str) -> dict[str, Path]:
    """Return the complete, canonical Gate 1 output path set for ``version``."""

    if type(version) is not str or version not in VERSIONS:
        raise GoldenInnError(f"version must be one of {list(VERSIONS)}")

    root = _path_value(project_root, "project_root")
    base = root / _CATALOG_ROOT / Path(_OUTPUT_ROOT)
    stem = f"{ASSET_ID}__golden__{version}"
    return {
        "blend": base / f"{stem}.blend",
        "hero_3q": base / f"{ASSET_ID}__hero_3q__{version}.png",
        "front": base / f"{ASSET_ID}__front__{version}.png",
        "player_camera": base / f"{ASSET_ID}__player_camera__{version}.png",
        "silhouette_128": base / f"{ASSET_ID}__silhouette_128__{version}.png",
        "report": base / f"{ASSET_ID}__gate1_report__{version}.json",
    }


def sha256_file(path: Path) -> str:
    """Return the lowercase SHA-256 digest for a file."""

    digest = hashlib.sha256()
    file_path = _path_value(path, "hash path")
    try:
        with file_path.open("rb") as handle:
            for block in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(block)
    except (OSError, ValueError) as exc:
        raise GoldenInnError(f"cannot hash output file {path}: {exc}") from exc
    return digest.hexdigest()


def _canonicalize_paths(paths: Mapping[str, Any]) -> tuple[dict[str, Path], str]:
    required_keys = ("blend", *REQUIRED_VIEWS, "silhouette_128", "report")
    supplied_keys = set(paths)
    invalid_keys = [key for key in supplied_keys if not isinstance(key, str)]
    if invalid_keys:
        rendered = ", ".join(repr(key) for key in invalid_keys)
        raise GoldenInnError(f"paths contain unexpected non-string keys: {rendered}")
    missing = [key for key in required_keys if key not in supplied_keys]
    unexpected = sorted(supplied_keys.difference(required_keys))
    if missing:
        raise GoldenInnError(f"paths missing required keys: {', '.join(missing)}")
    if unexpected:
        raise GoldenInnError(f"paths contain unexpected keys: {', '.join(unexpected)}")

    normalized = {key: _path_value(paths[key], f"paths.{key}") for key in required_keys}
    version = next(
        (
            candidate
            for candidate in VERSIONS
            if normalized["blend"].name == f"{ASSET_ID}__golden__{candidate}.blend"
        ),
        None,
    )
    if version is None:
        raise GoldenInnError("blend output path has an invalid version or filename")

    expected_names = {
        "blend": f"{ASSET_ID}__golden__{version}.blend",
        **{
            view: f"{ASSET_ID}__{view}__{version}.png"
            for view in REQUIRED_VIEWS
        },
        "silhouette_128": f"{ASSET_ID}__silhouette_128__{version}.png",
        "report": f"{ASSET_ID}__gate1_report__{version}.json",
    }
    expected_suffix = (*_CATALOG_ROOT.parts, *Path(_OUTPUT_ROOT).parts)
    try:
        lexical_parent = normalized["blend"].parent.absolute()
    except (OSError, RuntimeError, ValueError) as exc:
        raise GoldenInnError(f"paths.blend parent could not be made absolute: {exc}") from exc
    if _native_path_parts(lexical_parent.parts[-len(expected_suffix) :]) != _native_path_parts(
        expected_suffix
    ):
        raise GoldenInnError(f"outputs must stay under canonical output_root {_OUTPUT_ROOT}")
    project_root = lexical_parent.parents[len(expected_suffix) - 1]
    resolved_project_root = _resolved_path(project_root, "project_root")
    parent = _resolved_path(lexical_parent, "paths.blend parent")
    if _native_path_parts(parent.parts[-len(expected_suffix) :]) != _native_path_parts(
        expected_suffix
    ):
        raise GoldenInnError(f"outputs must stay under canonical output_root {_OUTPUT_ROOT}")
    try:
        parent.relative_to(resolved_project_root)
    except ValueError as exc:
        raise GoldenInnError("canonical output_root escapes resolved project_root") from exc
    for key, path in normalized.items():
        if path.name != expected_names[key] or _resolved_path(path.parent, f"paths.{key} parent") != parent:
            raise GoldenInnError(f"{key} output path is not canonical for version {version}")
        resolved_file = _resolved_path(path, f"paths.{key}")
        if resolved_file.parent != parent or resolved_file.name != expected_names[key]:
            raise GoldenInnError(f"{key} artifact escapes canonical output_root {_OUTPUT_ROOT}")
    return normalized, version


def _read_artifact_bytes(path: Path, key: str) -> bytes:
    try:
        data = path.read_bytes()
    except (OSError, ValueError) as exc:
        raise GoldenInnError(f"{key} artifact could not be read: {exc}") from exc
    if not data:
        raise GoldenInnError(f"{key} artifact is empty")
    return data


def _validate_blend(path: Path) -> None:
    data = _read_artifact_bytes(path, "blend")
    header = data[:12]
    if not header.startswith(b"BLENDER"):
        raise GoldenInnError("blend artifact must begin with ASCII BLENDER")
    if len(header) != 12:
        raise GoldenInnError("blend artifact requires a full 12-byte header")
    if header[7:8] not in (b"_", b"-"):
        raise GoldenInnError("blend header pointer marker must be _ or -")
    if header[8:9] not in (b"v", b"V"):
        raise GoldenInnError("blend header endian marker must be v or V")
    if any(byte < ord("0") or byte > ord("9") for byte in header[9:12]):
        raise GoldenInnError("blend header version must contain three ASCII digits")
    if int(header[9:12]) < 402:
        raise GoldenInnError("blend header version must be Blender 4.2 or newer")
    if len(data) == 12:
        raise GoldenInnError("blend block framing is missing after the header")

    pointer_size = 8 if header[7:8] == b"-" else 4
    endian = "<" if header[8:9] == b"v" else ">"
    pointer_code = "Q" if pointer_size == 8 else "I"
    bhead_format = f"{endian}4si{pointer_code}ii"
    bhead_size = struct.calcsize(bhead_format)
    offset = 12
    dna_count = 0
    endb_count = 0
    while offset < len(data):
        if len(data) - offset < bhead_size:
            raise GoldenInnError("blend block framing is truncated")
        try:
            code, payload_length, _old_pointer, _sdna_index, _count = struct.unpack_from(
                bhead_format, data, offset
            )
        except struct.error as exc:
            raise GoldenInnError(f"blend block framing is malformed: {exc}") from exc
        if payload_length < 0:
            raise GoldenInnError("blend block payload length cannot be negative")
        payload_start = offset + bhead_size
        payload_end = payload_start + payload_length
        if payload_end < payload_start or payload_end > len(data):
            raise GoldenInnError("blend block framing is truncated or overflowed")
        payload = data[payload_start:payload_end]

        if code == b"ENDB":
            endb_count += 1
            if payload_length != 0:
                raise GoldenInnError("blend ENDB block must have zero payload")
            if payload_end != len(data):
                raise GoldenInnError("blend ENDB block must be terminal with no trailing data")
            offset = payload_end
            break

        if code == b"DNA1":
            dna_count += 1
            if dna_count != 1:
                raise GoldenInnError("blend DNA1 block must appear exactly once")
            if not payload.startswith(b"SDNA"):
                raise GoldenInnError("blend DNA1 payload must start with SDNA")
            tag_offset = 4
            for tag in (b"NAME", b"TYPE", b"TLEN", b"STRC"):
                found = payload.find(tag, tag_offset)
                if found < 0:
                    raise GoldenInnError(
                        "blend DNA1 must contain ordered NAME, TYPE, TLEN, and STRC tags"
                    )
                tag_offset = found + len(tag)

        offset = payload_end

    if dna_count != 1:
        raise GoldenInnError("blend requires exactly one DNA1 block before ENDB")
    if endb_count != 1:
        raise GoldenInnError("blend requires exactly one terminal ENDB block")


def _validate_png(path: Path, key: str, expected_dimensions: tuple[int, int]) -> None:
    data = _read_artifact_bytes(path, key)
    if not data.startswith(_PNG_SIGNATURE):
        raise GoldenInnError(f"{key} PNG signature is invalid")

    offset = len(_PNG_SIGNATURE)
    chunk_index = 0
    ihdr_count = 0
    width = height = color_type = 0
    idat_parts: list[bytes] = []
    idat_started = False
    idat_finished = False
    seen_iend = False
    while offset < len(data):
        if len(data) - offset < 12:
            if chunk_index == 0:
                raise GoldenInnError(f"{key} IHDR is missing or malformed")
            raise GoldenInnError(f"{key} has a truncated PNG chunk")
        chunk_length = struct.unpack(">I", data[offset : offset + 4])[0]
        chunk_type = data[offset + 4 : offset + 8]
        chunk_data_start = offset + 8
        chunk_data_end = chunk_data_start + chunk_length
        chunk_end = chunk_data_end + 4
        if chunk_end > len(data):
            if chunk_index == 0:
                raise GoldenInnError(f"{key} IHDR is missing or malformed")
            raise GoldenInnError(f"{key} has a truncated PNG chunk")
        chunk_data = data[chunk_data_start:chunk_data_end]
        stored_crc = struct.unpack(">I", data[chunk_data_end:chunk_end])[0]
        actual_crc = zlib.crc32(chunk_type + chunk_data) & 0xFFFFFFFF
        if stored_crc != actual_crc:
            raise GoldenInnError(f"{key} PNG chunk CRC mismatch")

        if chunk_index == 0 and chunk_type != b"IHDR":
            raise GoldenInnError(f"{key} IHDR must be the first PNG chunk")
        if chunk_type == b"IHDR":
            ihdr_count += 1
            if ihdr_count != 1 or chunk_index != 0:
                raise GoldenInnError(f"{key} IHDR must appear exactly once and first")
            if chunk_length != 13:
                raise GoldenInnError(f"{key} IHDR is missing or malformed")
            width, height, bit_depth, color_type, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", chunk_data
            )
            if width <= 0 or height <= 0:
                raise GoldenInnError(f"{key} PNG dimensions must be positive")
            if (
                bit_depth != 8
                or color_type not in (2, 6)
                or compression != 0
                or filtering != 0
                or interlace != 0
            ):
                raise GoldenInnError(f"{key} IHDR format must be non-interlaced 8-bit RGB or RGBA")
        elif chunk_type == b"IDAT":
            if ihdr_count != 1 or seen_iend:
                raise GoldenInnError(f"{key} IDAT chunk order is invalid")
            if idat_finished:
                raise GoldenInnError(f"{key} IDAT chunks must be consecutive")
            idat_started = True
            if chunk_data:
                idat_parts.append(chunk_data)
        elif chunk_type == b"IEND":
            if chunk_length != 0:
                raise GoldenInnError(f"{key} IEND chunk must be empty")
            seen_iend = True
            if chunk_end != len(data):
                raise GoldenInnError(f"{key} IEND must be terminal")
            offset = chunk_end
            break
        elif idat_started:
            idat_finished = True

        chunk_index += 1
        offset = chunk_end

    if ihdr_count != 1:
        raise GoldenInnError(f"{key} IHDR must appear exactly once and first")
    if not idat_parts:
        raise GoldenInnError(f"{key} requires at least one non-empty IDAT chunk")
    if not seen_iend:
        raise GoldenInnError(f"{key} IEND chunk is required")
    if (width, height) != expected_dimensions:
        expected = f"{expected_dimensions[0]}x{expected_dimensions[1]}"
        raise GoldenInnError(f"{key} dimensions must be exactly {expected}")

    bytes_per_pixel = 3 if color_type == 2 else 4
    row_size = 1 + width * bytes_per_pixel
    expected_decoded_size = row_size * height
    decompressor = zlib.decompressobj()
    try:
        pixels = decompressor.decompress(
            b"".join(idat_parts), expected_decoded_size + 1
        )
    except zlib.error as exc:
        raise GoldenInnError(f"{key} IDAT compressed data is invalid: {exc}") from exc
    if (
        not decompressor.eof
        or decompressor.unused_data
        or decompressor.unconsumed_tail
        or len(pixels) > expected_decoded_size
    ):
        raise GoldenInnError(f"{key} IDAT compressed data must contain one bounded zlib stream")
    if len(pixels) != expected_decoded_size:
        raise GoldenInnError(f"{key} IDAT decoded byte count does not match IHDR")
    if any(pixels[row * row_size] > 4 for row in range(height)):
        raise GoldenInnError(f"{key} IDAT contains an invalid PNG filter type")


def _validate_artifacts(paths: Mapping[str, Path]) -> None:
    _validate_blend(paths["blend"])
    for key in REQUIRED_VIEWS:
        _validate_png(paths[key], key, (1536, 1024))
    _validate_png(paths["silhouette_128"], "silhouette_128", (128, 128))


def _require_empty_list(report: Mapping[str, Any], field: str) -> None:
    value = report.get(field)
    if not isinstance(value, list):
        raise GoldenInnError(f"{field} must be a list")
    if value:
        raise GoldenInnError(f"{field} must be empty")


def _module_material_counts(report: Mapping[str, Any]) -> dict[str, int]:
    counts = _require_mapping(report.get("module_material_counts"), "module_material_counts")
    missing = [name for name in _REQUIRED_MODULES if name not in counts]
    if missing:
        raise GoldenInnError(f"missing modules: {', '.join(missing)}")
    unexpected = sorted(set(counts).difference(_REQUIRED_MODULES))
    if unexpected:
        raise GoldenInnError(f"module_material_counts has unexpected modules: {', '.join(unexpected)}")
    normalized: dict[str, int] = {}
    for name in _REQUIRED_MODULES:
        count = counts[name]
        if type(count) is not int or not 1 <= count <= 2:
            raise GoldenInnError(f"module_material_counts.{name} must be 1 or 2")
        normalized[name] = count
    return normalized


def _normalized_bounds(value: Any, ground_min_z_m: float) -> list[float]:
    bounds = _require_mapping(value, "bounds_m")
    vectors: dict[str, list[float]] = {}
    for field in ("min", "max", "size"):
        vector = bounds.get(field)
        if (
            not isinstance(vector, list)
            or len(vector) != 3
            or not all(_is_finite_number(item) for item in vector)
        ):
            raise GoldenInnError(f"bounds_m.{field} must contain three finite numbers")
        vectors[field] = [float(item) for item in vector]

    minimum, maximum, result = vectors["min"], vectors["max"], vectors["size"]
    if any(dimension <= 0.0 for dimension in result):
        raise GoldenInnError("bounds_m.size dimensions must be positive")
    for axis, (lower, upper, size) in enumerate(zip(minimum, maximum, result)):
        if upper <= lower or not math.isclose(upper - lower, size, abs_tol=1e-6):
            raise GoldenInnError(f"bounds_m size does not match min/max on axis {axis}")
    if not math.isclose(minimum[2], ground_min_z_m, abs_tol=1e-6):
        raise GoldenInnError("bounds_m.min Z must match ground_min_z_m")
    center_x = (minimum[0] + maximum[0]) / 2.0
    center_y = (minimum[1] + maximum[1]) / 2.0
    if abs(center_x) > _PIVOT_CENTER_TOLERANCE_M or abs(center_y) > _PIVOT_CENTER_TOLERANCE_M:
        raise GoldenInnError(
            f"bottom_center pivot must stay within {_PIVOT_CENTER_TOLERANCE_M} m of XY origin"
        )
    if any(
        abs(actual - target) > _BOUNDS_TOLERANCE_M
        for actual, target in zip(result, _TARGET_DIMENSIONS_M)
    ):
        raise GoldenInnError(
            f"bounds_m must remain within {_BOUNDS_TOLERANCE_M} m of {list(_TARGET_DIMENSIONS_M)}"
        )
    return result


def _verify_output_hashes(report: Mapping[str, Any], paths: Mapping[str, Path]) -> bool:
    if "output_sha256" not in report:
        raise GoldenInnError("output_sha256 is required for every output artifact")
    hashes = _require_mapping(report.get("output_sha256"), "output_sha256")
    expected_keys = {key for key in paths if key != "report"}
    missing = sorted(expected_keys.difference(hashes))
    if missing:
        raise GoldenInnError(f"output_sha256 is missing entries: {', '.join(missing)}")
    unexpected = sorted(set(hashes).difference(expected_keys))
    if unexpected:
        raise GoldenInnError(f"output_sha256 has unexpected entries: {', '.join(unexpected)}")
    for key, path in paths.items():
        if key == "report":
            continue
        supplied = hashes.get(key)
        if (
            not isinstance(supplied, str)
            or len(supplied) != 64
            or any(character not in "0123456789abcdefABCDEF" for character in supplied)
        ):
            raise GoldenInnError(f"output_sha256 entry for {key} must be a 64-character hex digest")
        actual = sha256_file(path)
        if supplied.lower() != actual:
            raise GoldenInnError(f"output_sha256 mismatch for {key}")
    return True


def verify_gate1_report(report_path: Path, paths: Mapping[str, Any]) -> dict:
    """Validate Gate 1 report facts, artifacts, and mandatory SHA-256 evidence."""

    paths_mapping = _require_mapping(paths, "paths")
    normalized_paths, expected_version = _canonicalize_paths(paths_mapping)
    normalized_report_path = _path_value(report_path, "report_path")
    if _resolved_path(normalized_report_path, "report_path") != _resolved_path(
        normalized_paths["report"], "paths.report"
    ):
        raise GoldenInnError("report_path must match paths.report")
    if not normalized_report_path.is_file():
        raise GoldenInnError(f"missing report: {normalized_report_path}")
    for key, path in normalized_paths.items():
        if key != "report" and not path.is_file():
            raise GoldenInnError(f"missing output for {key}: {path}")
    _validate_artifacts(normalized_paths)

    try:
        raw = normalized_report_path.read_text(encoding="utf-8")
        payload = json.loads(raw)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise GoldenInnError(f"report JSON is invalid: {normalized_report_path}: {exc}") from exc
    report = _require_mapping(payload, "report")

    if report.get("asset_id") != ASSET_ID:
        raise GoldenInnError(f"asset_id must be exactly {ASSET_ID}")
    if report.get("version") != expected_version:
        raise GoldenInnError(f"version must match supplied output paths: {expected_version}")
    blender_version = report.get("blender_version")
    if (
        not isinstance(blender_version, list)
        or len(blender_version) != 3
        or any(type(part) is not int or part < 0 for part in blender_version)
        or tuple(blender_version) < (4, 2, 0)
    ):
        raise GoldenInnError("blender_version must be a three-integer list at least [4, 2, 0]")
    views = report.get("render_views")
    if not isinstance(views, list) or tuple(views) != REQUIRED_VIEWS:
        raise GoldenInnError(f"render_views must be exactly {list(REQUIRED_VIEWS)}")

    ground_min = report.get("ground_min_z_m")
    if not _is_finite_number(ground_min) or abs(float(ground_min)) > _GROUND_TOLERANCE_M:
        raise GoldenInnError(
            f"ground_min_z_m must be finite and within {_GROUND_TOLERANCE_M} m of zero"
        )

    non_manifold = report.get("non_manifold_edges")
    if type(non_manifold) is not int or non_manifold != 0:
        raise GoldenInnError("non_manifold_edges must be zero")
    for field in ("object_count", "mesh_object_count"):
        value = report.get(field)
        if type(value) is not int or value <= 0:
            raise GoldenInnError(f"{field} must be a positive integer")
    if report["mesh_object_count"] > report["object_count"]:
        raise GoldenInnError("mesh_object_count cannot exceed object_count")
    faces = _require_mapping(report.get("faces"), "faces")
    face_keys = ("triangles", "quads", "ngons")
    if set(faces) != set(face_keys):
        raise GoldenInnError(f"faces must contain exactly {list(face_keys)}")
    if any(type(faces[key]) is not int or faces[key] < 0 for key in face_keys):
        raise GoldenInnError("faces counts must be non-negative integers")
    if sum(faces[key] for key in face_keys) <= 0:
        raise GoldenInnError("faces must report at least one face")
    quad_min, quad_max = _APPROVED_QUAD_RANGE
    if not quad_min <= faces["quads"] <= quad_max:
        raise GoldenInnError(f"faces.quads must be inside approved range [{quad_min}, {quad_max}]")
    if faces["triangles"] + faces["ngons"] >= faces["quads"]:
        raise GoldenInnError("faces must remain quad-dominant")
    _require_empty_list(report, "negative_scale_objects")
    _require_empty_list(report, "unapplied_transform_objects")
    _module_material_counts(report)

    capital_layers = report.get("capital_layers")
    if type(capital_layers) is not int or capital_layers != 1:
        raise GoldenInnError("capital_layers must be exactly integer 1")
    window_divisions = report.get("window_divisions")
    if type(window_divisions) is not int or window_divisions != 2:
        raise GoldenInnError("window_divisions must be exactly integer 2")

    bounds = _normalized_bounds(report.get("bounds_m"), float(ground_min))
    hashes_verified = _verify_output_hashes(report, normalized_paths)
    return {
        "ok": True,
        "asset_id": ASSET_ID,
        "version": expected_version,
        "views": list(REQUIRED_VIEWS),
        "modules": list(_REQUIRED_MODULES),
        "bounds_m": bounds,
        "sha256_verified": hashes_verified,
    }
