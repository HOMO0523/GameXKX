"""Pure host-side helpers for deterministic Qingshan whitebox evidence."""

from __future__ import annotations

from collections.abc import Mapping, Sequence
import hashlib
import json
import math
import random
import re
from typing import Any


__all__ = ("canonical_layout_hash", "generate_seeded_proxy_transforms")

_MAX_FOLIAGE_ATTEMPTS = 20_000
_MAX_FOLIAGE_COUNT = 20_000
_MAX_MOUNTAIN_COUNT = 10_000
_TRANSFORM_FIELDS = ("location_cm", "rotation_degrees", "scale")
_SIDES = ("NORTH", "EAST", "SOUTH", "WEST")
_STABLE_ID = re.compile(r"^[A-Za-z][A-Za-z0-9_]*$")


def _finite_number(value: Any, field: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        kind = "bool" if isinstance(value, bool) else type(value).__name__
        raise ValueError(f"{field} must be a finite number, not {kind}")
    try:
        finite = math.isfinite(value)
    except (OverflowError, TypeError, ValueError):
        finite = False
    if not finite:
        raise ValueError(f"{field} must be finite")
    return float(value)


def _integer(value: Any, field: str, minimum: int = 0) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise ValueError(f"{field} must be an integer, not bool" if isinstance(value, bool)
                         else f"{field} must be an integer")
    if value < minimum:
        raise ValueError(f"{field} must be at least {minimum}")
    return value


def _mapping(value: Any, field: str) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise ValueError(f"{field} must be an object")
    return value


def _list(value: Any, field: str) -> Sequence[Any]:
    if isinstance(value, (str, bytes)) or not isinstance(value, Sequence):
        raise ValueError(f"{field} must be a list")
    return value


def _vector(value: Any, field: str, length: int) -> tuple[float, ...]:
    items = _list(value, field)
    if len(items) != length:
        raise ValueError(f"{field} must contain exactly {length} numbers")
    return tuple(_finite_number(item, f"{field}[{index}]") for index, item in enumerate(items))


def _ordered_range(value: Any, field: str, *, nonnegative: bool = False) -> tuple[float, float]:
    low, high = _vector(value, field, 2)
    threshold = 0.0 if nonnegative else math.nextafter(0.0, math.inf)
    if low < threshold or high <= low:
        qualifier = "nonnegative and ordered" if nonnegative else "positive and ordered"
        raise ValueError(f"{field} must be {qualifier} [minimum, maximum]")
    return low, high


def _bounds(value: Any, field: str) -> tuple[float, float, float, float]:
    x_min, x_max, y_min, y_max = _vector(value, field, 4)
    if x_min >= x_max or y_min >= y_max:
        raise ValueError(f"{field} must be [min_x, max_x, min_y, max_y]")
    if not math.isfinite(x_max - x_min) or not math.isfinite(y_max - y_min):
        raise ValueError(f"{field} span must be finite")
    return x_min, x_max, y_min, y_max


def _source_index(config: Mapping[str, Any], field: str) -> dict[str, Mapping[str, Any]]:
    result: dict[str, Mapping[str, Any]] = {}
    for index, raw in enumerate(_list(config.get(field), field)):
        item = _mapping(raw, f"{field}[{index}]")
        stable_id = item.get("id")
        if not isinstance(stable_id, str) or not stable_id.strip():
            raise ValueError(f"{field}[{index}].id must be a nonblank string")
        if stable_id in result:
            raise ValueError(f"{field} contains duplicate id {stable_id}")
        result[stable_id] = item
    return result


def _distance_to_segment(point: tuple[float, float], start: Sequence[Any], end: Sequence[Any]) -> float:
    ax, ay, _ = _vector(start, "spline point", 3)
    bx, by, _ = _vector(end, "spline point", 3)
    dx, dy = bx - ax, by - ay
    if dx == 0.0 and dy == 0.0:
        return math.hypot(point[0] - ax, point[1] - ay)
    t = ((point[0] - ax) * dx + (point[1] - ay) * dy) / (dx * dx + dy * dy)
    t = max(0.0, min(1.0, t))
    return math.hypot(point[0] - (ax + t * dx), point[1] - (ay + t * dy))


def _inside_rotated_footprint(
    point: tuple[float, float], plot: Mapping[str, Any], margin: float, field: str
) -> bool:
    location = _vector(plot.get("location_cm"), f"{field}.location_cm", 3)
    size = _vector(plot.get("size_cm"), f"{field}.size_cm", 3)
    yaw = math.radians(-_finite_number(plot.get("yaw_degrees"), f"{field}.yaw_degrees"))
    dx, dy = point[0] - location[0], point[1] - location[1]
    local_x = dx * math.cos(yaw) - dy * math.sin(yaw)
    local_y = dx * math.sin(yaw) + dy * math.cos(yaw)
    return abs(local_x) <= size[0] / 2.0 + margin and abs(local_y) <= size[1] / 2.0 + margin


def _validate_geometry_sources(config: Mapping[str, Any]):
    raw_sources = {
        "anchor_circle": _source_index(config, "fixed_anchors"),
        "road_corridor": _source_index(config, "road_splines"),
        "river_corridor": _source_index(config, "river_splines"),
        "building_footprint": _source_index(config, "building_plots"),
    }
    sources = {kind: {} for kind in raw_sources}
    for stable_id, source in raw_sources["anchor_circle"].items():
        location = _vector(source.get("location_cm"), f"{stable_id}.location_cm", 3)
        radius = _finite_number(
            source.get("protected_radius_cm"), f"{stable_id}.protected_radius_cm"
        )
        if radius <= 0:
            raise ValueError(f"{stable_id}.protected_radius_cm must be positive")
        _finite_number(source.get("yaw_degrees"), f"{stable_id}.yaw_degrees")
        sources["anchor_circle"][stable_id] = (location, radius)

    for kind in ("road_corridor", "river_corridor"):
        for stable_id, source in raw_sources[kind].items():
            width = _finite_number(source.get("width_cm"), f"{stable_id}.width_cm")
            if width <= 0:
                raise ValueError(f"{stable_id}.width_cm must be positive")
            raw_points = _list(source.get("points_cm"), f"{stable_id}.points_cm")
            if len(raw_points) < 2:
                raise ValueError(f"{stable_id}.points_cm must contain at least two points")
            points = tuple(
                _vector(point, f"{stable_id}.points_cm[{index}]", 3)
                for index, point in enumerate(raw_points)
            )
            sources[kind][stable_id] = (points, width)

    for stable_id, source in raw_sources["building_footprint"].items():
        location = _vector(source.get("location_cm"), f"{stable_id}.location_cm", 3)
        size = _vector(source.get("size_cm"), f"{stable_id}.size_cm", 3)
        if any(component <= 0 for component in size):
            raise ValueError(f"{stable_id}.size_cm must be positive")
        yaw = _finite_number(source.get("yaw_degrees"), f"{stable_id}.yaw_degrees")
        sources["building_footprint"][stable_id] = (location, size, yaw)
    return sources


def _compile_exclusions(config: Mapping[str, Any], sources):
    compiled = []
    for index, raw in enumerate(_list(config.get("exclusion_zones"), "exclusion_zones")):
        field = f"exclusion_zones[{index}]"
        exclusion = _mapping(raw, field)
        kind = exclusion.get("kind")
        if kind not in sources:
            raise ValueError(f"{field}.kind is invalid")
        source_id = exclusion.get("source_id")
        if not isinstance(source_id, str) or source_id not in sources[kind]:
            raise ValueError(f"{field}.source_id does not resolve for {kind}")
        margin = _finite_number(exclusion.get("margin_cm"), f"{field}.margin_cm")
        if margin < 0:
            raise ValueError(f"{field}.margin_cm must be nonnegative")
        source = sources[kind][source_id]

        if kind == "anchor_circle":
            center, radius = source
            compiled.append(lambda point, c=center, r=radius + margin: math.dist(point, c[:2]) <= r)
        elif kind in ("road_corridor", "river_corridor"):
            points, width = source
            segments = tuple(zip(points, points[1:]))
            radius = width / 2.0 + margin
            compiled.append(
                lambda point, s=segments, r=radius: min(
                    _distance_to_segment(point, start, end) for start, end in s
                ) <= r
            )
        else:
            location, size, yaw = source
            compiled.append(
                lambda point, p={"location_cm": location, "size_cm": size, "yaw_degrees": yaw},
                m=margin, f=source_id: _inside_rotated_footprint(point, p, m, f)
            )
    return compiled


def _make_transform(stable_id: str, location, rotation, scale) -> dict[str, Any]:
    if not _STABLE_ID.fullmatch(stable_id):
        raise ValueError(f"generated transform id {stable_id!r} is not stable")
    return {
        "id": stable_id,
        "location_cm": list(_vector(location, f"{stable_id}.location_cm", 3)),
        "rotation_degrees": list(_vector(rotation, f"{stable_id}.rotation_degrees", 3)),
        "scale": list(_vector(scale, f"{stable_id}.scale", 3)),
    }


def generate_seeded_proxy_transforms(config: Mapping[str, Any]) -> dict[str, list[dict[str, Any]]]:
    """Expand proxy-generation metadata without touching global RNG state."""
    config = _mapping(config, "config")
    seed = _integer(config.get("seed"), "seed")
    playable = _bounds(config.get("playable_bounds_cm"), "playable_bounds_cm")
    world = _bounds(config.get("world_bounds_cm"), "world_bounds_cm")
    if not (world[0] <= playable[0] < playable[1] <= world[1]
            and world[2] <= playable[2] < playable[3] <= world[3]):
        raise ValueError("playable_bounds_cm must be inside world_bounds_cm")

    proxy = _mapping(config.get("proxy_generation"), "proxy_generation")
    foliage_config = _mapping(proxy.get("foliage"), "proxy_generation.foliage")
    mountain_config = _mapping(proxy.get("mountains"), "proxy_generation.mountains")
    foliage_count = _integer(foliage_config.get("count"), "proxy_generation.foliage.count")
    mountain_count = _integer(mountain_config.get("count"), "proxy_generation.mountains.count")
    if foliage_count > _MAX_FOLIAGE_COUNT:
        raise ValueError(
            f"proxy_generation.foliage.count exceeds hard maximum {_MAX_FOLIAGE_COUNT}"
        )
    if mountain_count > _MAX_MOUNTAIN_COUNT:
        raise ValueError(
            f"proxy_generation.mountains.count exceeds hard maximum {_MAX_MOUNTAIN_COUNT}"
        )
    spawn_caps = _mapping(config.get("spawn_caps"), "spawn_caps")
    foliage_cap = _integer(spawn_caps.get("foliage"), "spawn_caps.foliage")
    mountain_cap = _integer(spawn_caps.get("mountains"), "spawn_caps.mountains")
    if foliage_count > foliage_cap:
        raise ValueError("proxy_generation.foliage.count must not exceed spawn_caps.foliage")
    if mountain_count > mountain_cap:
        raise ValueError("proxy_generation.mountains.count must not exceed spawn_caps.mountains")
    foliage_scale = _ordered_range(
        foliage_config.get("scale_range"), "proxy_generation.foliage.scale_range"
    )
    foliage_exclusion_margin = _finite_number(
        foliage_config.get("exclusion_margin_cm"),
        "proxy_generation.foliage.exclusion_margin_cm",
    )
    if foliage_exclusion_margin < 0:
        raise ValueError("proxy_generation.foliage.exclusion_margin_cm must be nonnegative")
    mountain_scale_xy = _ordered_range(
        mountain_config.get("scale_xy_range"), "proxy_generation.mountains.scale_xy_range"
    )
    mountain_scale_z = _ordered_range(
        mountain_config.get("scale_z_range"), "proxy_generation.mountains.scale_z_range"
    )
    perimeter = _ordered_range(
        mountain_config.get("perimeter_band_cm"),
        "proxy_generation.mountains.perimeter_band_cm",
        nonnegative=True,
    )
    if mountain_count and perimeter[0] <= 0:
        raise ValueError("proxy_generation.mountains inner perimeter must be greater than zero")
    if mountain_count and mountain_count < 16:
        raise ValueError("proxy_generation.mountains.count must be at least 16 to cover four sides")

    sources = _validate_geometry_sources(config)
    exclusions = _compile_exclusions(config, sources)
    rng = random.Random(seed)
    foliage = []
    attempts = 0
    while len(foliage) < foliage_count and attempts < _MAX_FOLIAGE_ATTEMPTS:
        attempts += 1
        point = (rng.uniform(playable[0], playable[1]), rng.uniform(playable[2], playable[3]))
        if any(reject(point) for reject in exclusions):
            continue
        uniform_scale = rng.uniform(*foliage_scale)
        foliage.append(_make_transform(
            f"FOLIAGE_{len(foliage) + 1:03d}",
            (point[0], point[1], 0.0),
            (0.0, 0.0, rng.uniform(0.0, 360.0)),
            (uniform_scale, uniform_scale, uniform_scale),
        ))
    if len(foliage) != foliage_count:
        raise RuntimeError(
            "Unable to fill foliage proxies within 20000 attempts: "
            f"requested={foliage_count}, accepted={len(foliage)}"
        )

    available_offsets = {
        "NORTH": world[3] - playable[3],
        "EAST": world[1] - playable[1],
        "SOUTH": playable[2] - world[2],
        "WEST": playable[0] - world[0],
    }
    for side, available in available_offsets.items():
        if mountain_count and available <= perimeter[0]:
            raise ValueError(
                "proxy_generation.mountains.perimeter_band_cm cannot fit "
                f"inside world_bounds_cm on {side.lower()} side"
            )

    side_counts = {side: 4 for side in _SIDES} if mountain_count else {side: 0 for side in _SIDES}
    for index in range(max(0, mountain_count - 16)):
        side_counts[_SIDES[index % len(_SIDES)]] += 1

    mountains = []
    for side in _SIDES:
        max_offset = min(perimeter[1], available_offsets[side])
        for side_index in range(1, side_counts[side] + 1):
            offset = rng.uniform(perimeter[0], max_offset)
            if side == "NORTH":
                location = (rng.uniform(playable[0], playable[1]), playable[3] + offset, 0.0)
            elif side == "SOUTH":
                location = (rng.uniform(playable[0], playable[1]), playable[2] - offset, 0.0)
            elif side == "EAST":
                location = (playable[1] + offset, rng.uniform(playable[2], playable[3]), 0.0)
            else:
                location = (playable[0] - offset, rng.uniform(playable[2], playable[3]), 0.0)
            scale_xy = rng.uniform(*mountain_scale_xy)
            mountains.append(_make_transform(
                f"MOUNTAIN_{side}_{side_index:03d}",
                location,
                (0.0, 0.0, rng.uniform(0.0, 360.0)),
                (scale_xy, scale_xy, rng.uniform(*mountain_scale_z)),
            ))

    return {"foliage": foliage, "mountains": mountains}


def _canonicalize(value: Any, decimals: int, field: str, transform_numeric: bool = False):
    if isinstance(value, Mapping):
        if any(not isinstance(key, str) for key in value):
            raise ValueError(f"{field} keys must be strings")
        present_transform_fields = [name for name in _TRANSFORM_FIELDS if name in value]
        if present_transform_fields:
            stable_id = value.get("id")
            if not isinstance(stable_id, str) or not _STABLE_ID.fullmatch(stable_id):
                raise ValueError(f"{field}.id must be a nonblank stable string")
            missing = [name for name in _TRANSFORM_FIELDS if name not in value]
            if missing:
                raise ValueError(f"{field} transform is missing {', '.join(missing)}")
            for name in _TRANSFORM_FIELDS:
                vector = value[name]
                if (isinstance(vector, (str, bytes)) or not isinstance(vector, Sequence)
                        or len(vector) != 3):
                    raise ValueError(f"{field}.{name} must contain exactly 3 numbers")
                for index, component in enumerate(vector):
                    _finite_number(component, f"{field}.{name}[{index}]")
        result = {}
        for key in sorted(value):
            item = value[key]
            if key == "id" and (not isinstance(item, str) or not item.strip()):
                raise ValueError(f"{field}.id must be a nonblank string")
            result[key] = _canonicalize(
                item, decimals, f"{field}.{key}", transform_numeric or key in _TRANSFORM_FIELDS
            )
        return result
    if isinstance(value, Sequence) and not isinstance(value, (str, bytes)):
        items = [_canonicalize(item, decimals, f"{field}[{index}]", transform_numeric)
                 for index, item in enumerate(value)]
        if items and all(isinstance(item, dict) and isinstance(item.get("id"), str) for item in items):
            ids = [item["id"] for item in items]
            if len(ids) != len(set(ids)):
                raise ValueError(f"{field} contains duplicate stable id")
            items.sort(key=lambda item: item["id"])
        return items
    if isinstance(value, bool):
        raise ValueError(f"{field} must not contain bool")
    if isinstance(value, (int, float)):
        if transform_numeric:
            number = _finite_number(value, field)
            number = round(number, decimals)
            if number == 0:
                number = 0.0
            return number
        if isinstance(value, int):
            return value
        return _finite_number(value, field)
    if value is None or isinstance(value, str):
        return value
    raise ValueError(f"{field} contains unsupported {type(value).__name__}")


def canonical_layout_hash(payload: Mapping[str, Any], decimals: int = 3) -> dict[str, Any]:
    """Return canonical payload evidence and a stable SHA-256 digest."""
    if isinstance(decimals, bool) or not isinstance(decimals, int) or not 0 <= decimals <= 15:
        raise ValueError("decimals must be an integer between 0 and 15")
    payload = _mapping(payload, "payload")
    canonical_payload = _canonicalize(payload, decimals, "payload")
    canonical_json = json.dumps(
        canonical_payload, sort_keys=True, separators=(",", ":"), allow_nan=False
    )
    counts = {
        key: len(value)
        for key, value in canonical_payload.items()
        if isinstance(value, list)
    }
    return {
        "decimal_places": decimals,
        "quantization_step": 10.0 ** -decimals,
        "counts": counts,
        "canonical_payload": canonical_payload,
        "canonical_json": canonical_json,
        "sha256": hashlib.sha256(canonical_json.encode("utf-8")).hexdigest(),
    }
