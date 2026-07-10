"""Host-safe loader and validator for the Qingshan town PCG contract."""

from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Any, Mapping


__all__ = ("CONFIG_PATH", "load_config", "validate_config")

PROJECT_ROOT = Path(__file__).resolve().parents[2]
CONFIG_PATH = PROJECT_ROOT / "Config" / "GameXXK" / "TownPCG" / "QingshanTownVerticalSlice.json"


def _require_package_path(data: Mapping[str, Any], field: str) -> None:
    value = data.get(field)
    if (
        not isinstance(value, str)
        or value != value.strip()
        or not value.startswith("/Game/")
        or any(character.isspace() for character in value)
        or "." in value
    ):
        raise ValueError(
            f"{field} must be a valid Unreal package path beginning /Game/ "
            "with no whitespace or object suffix"
        )


def _require_integer(data: Mapping[str, Any], field: str) -> int:
    value = data.get(field)
    if isinstance(value, bool):
        raise ValueError(f"{field} must be an integer, not bool")
    if not isinstance(value, int):
        raise ValueError(f"{field} must be an integer")
    return value


def _is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def validate_config(data: Mapping[str, Any]) -> None:
    """Raise ``ValueError`` when *data* violates the deterministic contract."""

    if not isinstance(data, Mapping):
        raise ValueError("config must be an object")
    if data.get("schema_version") != 1 or isinstance(data.get("schema_version"), bool):
        raise ValueError("schema_version must be 1")

    package_path_fields = (
        "source_map",
        "prototype_map",
        "asset_root",
        "graph_path",
        "building_mesh",
    )
    for field in package_path_fields:
        _require_package_path(data, field)

    north_gate_label = data.get("north_gate_label")
    if (
        not isinstance(north_gate_label, str)
        or not north_gate_label.strip()
        or north_gate_label != north_gate_label.strip()
    ):
        raise ValueError("north_gate_label must be a nonblank trimmed string")

    seed = _require_integer(data, "seed")
    building_limit = _require_integer(data, "building_limit")
    building_hard_cap = _require_integer(data, "building_hard_cap")
    dimensions = {
        field: _require_integer(data, field)
        for field in (
            "main_road_width_cm",
            "river_width_cm",
            "bridge_width_cm",
            "road_setback_cm",
        )
    }
    instance_limits = {
        field: _require_integer(data, field)
        for field in ("tree_instance_limit", "prop_instance_limit")
    }

    if seed < 0:
        raise ValueError("seed must be at least 0")

    if building_hard_cap < 1 or not 1 <= building_limit <= building_hard_cap:
        raise ValueError("building_limit must be between 1 and building_hard_cap")
    for field, value in dimensions.items():
        if value <= 0:
            raise ValueError(f"{field} must be greater than 0")
    for field, value in instance_limits.items():
        if value < 0:
            raise ValueError(f"{field} must be at least 0")

    if data.get("runtime_generation") is not False:
        raise ValueError("runtime_generation must be false")

    points = data.get("building_points")
    if not isinstance(points, list):
        raise ValueError("building_points must be a list")
    if len(points) > building_hard_cap:
        raise ValueError("building_points count exceeds building_hard_cap")
    if len(points) != building_limit:
        raise ValueError("building_points count must equal building_limit")

    seen_locations = set()
    for index, point in enumerate(points):
        if not isinstance(point, Mapping):
            raise ValueError(f"building_points[{index}] must be an object")

        location = point.get("location_cm")
        if (
            not isinstance(location, (list, tuple))
            or len(location) != 3
            or not all(_is_number(component) for component in location)
        ):
            raise ValueError(f"building_points[{index}].location_cm must be a three-number vector")
        if not all(math.isfinite(component) for component in location):
            raise ValueError(
                f"building_points[{index}].location_cm must contain only finite numbers"
            )
        location_key = tuple(location)
        if location_key in seen_locations:
            raise ValueError(f"duplicate building point at location_cm {list(location_key)}")
        seen_locations.add(location_key)

        side = point.get("side")
        if side not in ("left", "right"):
            raise ValueError(f"building_points[{index}].side must be left or right")

        yaw = point.get("yaw_degrees")
        if not _is_number(yaw):
            raise ValueError(f"building_points[{index}].yaw_degrees must be a number")
        if not math.isfinite(yaw):
            raise ValueError(f"building_points[{index}].yaw_degrees must be finite")
        expected_yaw = 0 if side == "left" else 180
        if yaw != expected_yaw:
            raise ValueError(
                f"building_points[{index}] {side} point yaw_degrees must be {expected_yaw}"
            )


def load_config(path: str | Path = CONFIG_PATH) -> dict[str, Any]:
    """Load and validate a Qingshan town PCG JSON configuration file."""

    config_path = Path(path)
    with config_path.open("r", encoding="utf-8") as config_file:
        data = json.load(config_file, parse_constant=_reject_json_numeric_constant)
    validate_config(data)
    return data


def _reject_json_numeric_constant(constant: str) -> None:
    raise ValueError(f"non-standard JSON numeric constant {constant} is not allowed")
