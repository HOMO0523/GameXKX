"""Generate the deterministic broad 505 x 505 Qingshan B1 Landscape heightmap."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import math
from pathlib import Path
from typing import Sequence

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
LOADER_PATH = (
    PROJECT_ROOT / "Content" / "Python" / "gamexxk_qingshan_dress_b1_config.py"
)
DEFAULT_HEIGHTMAP = (
    PROJECT_ROOT
    / "Content"
    / "ArtSource"
    / "Qingshan"
    / "B1"
    / "H_QS_B1_Terrain_505.png"
)
DEFAULT_SAMPLES = DEFAULT_HEIGHTMAP.with_suffix(".samples.json")

ZERO_RAW = 32768
RIVER_TRENCH_DEPTH_CM = 360.0
RIVER_TRENCH_SIGMA_CM = 900.0
NOISE_OCTAVES = (
    {"wavelength_cm": 12000.0, "amplitude_cm": 14.0},
    {"wavelength_cm": 6000.0, "amplitude_cm": 6.0},
)
NAMED_SAMPLE_POINTS = {
    "gate": (0.0, 0.0),
    "core": (-12000.0, -1800.0),
    "south": (-18500.0, -4500.0),
    "dock_bank": (-22800.0, -8300.0),
    "riverbed": (-22000.0, -9600.0),
}


def _load_module(path: Path):
    spec = importlib.util.spec_from_file_location(
        "_gamexxk_qingshan_dress_b1_config_for_heightmap", path
    )
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load B1 config module: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_b1_config(project_root: Path = PROJECT_ROOT) -> dict:
    """Load the host-validated B1 merge used by all terrain calculations."""

    root = Path(project_root).resolve()
    loader_path = root / "Content" / "Python" / "gamexxk_qingshan_dress_b1_config.py"
    module = _load_module(loader_path)
    return module.load_config(
        root / "Config" / "GameXXK" / "TownPCG" / "QingshanDressB1.json",
        project_root=root,
    )


def landscape_actor_origin_local_cm(landscape: dict) -> list[float]:
    """Convert the desired Landscape grid centre to the actor's vertex-0 origin."""

    resolution = landscape["resolution"]
    scale = landscape["scale_cm"]
    center = landscape["center_local_cm"]
    return [
        float(center[0] - (resolution[0] - 1) * scale[0] / 2.0),
        float(center[1] - (resolution[1] - 1) * scale[1] / 2.0),
        float(center[2]),
    ]


def pixel_to_local(landscape: dict, pixel_xy: Sequence[int]) -> tuple[float, float]:
    origin = landscape_actor_origin_local_cm(landscape)
    scale = landscape["scale_cm"]
    return (
        float(origin[0] + int(pixel_xy[0]) * scale[0]),
        float(origin[1] + int(pixel_xy[1]) * scale[1]),
    )


def local_to_pixel(landscape: dict, local_xy: Sequence[float]) -> tuple[int, int]:
    origin = landscape_actor_origin_local_cm(landscape)
    scale = landscape["scale_cm"]
    pixel = (
        int(round((float(local_xy[0]) - origin[0]) / scale[0])),
        int(round((float(local_xy[1]) - origin[1]) / scale[1])),
    )
    resolution = landscape["resolution"]
    if not (0 <= pixel[0] < resolution[0] and 0 <= pixel[1] < resolution[1]):
        raise ValueError(f"local point lies outside Landscape grid: {tuple(local_xy)}")
    snapped = pixel_to_local(landscape, pixel)
    tolerance = max(abs(float(scale[0])), abs(float(scale[1]))) * 1.0e-6
    if math.hypot(snapped[0] - float(local_xy[0]), snapped[1] - float(local_xy[1])) > tolerance:
        raise ValueError(f"local point does not lie on a Landscape vertex: {tuple(local_xy)}")
    return pixel


def _point_segment_distance_cm(
    point: Sequence[float], start: Sequence[float], end: Sequence[float]
) -> float:
    px, py = float(point[0]), float(point[1])
    ax, ay = float(start[0]), float(start[1])
    bx, by = float(end[0]), float(end[1])
    dx, dy = bx - ax, by - ay
    length_squared = dx * dx + dy * dy
    if length_squared <= 1.0e-12:
        return math.hypot(px - ax, py - ay)
    fraction = max(
        0.0,
        min(1.0, ((px - ax) * dx + (py - ay) * dy) / length_squared),
    )
    return math.hypot(px - (ax + fraction * dx), py - (ay + fraction * dy))


def polyline_distance_cm(point: Sequence[float], points: Sequence[Sequence[float]]) -> float:
    if len(points) < 2:
        raise ValueError("river polyline requires at least two points")
    return min(
        _point_segment_distance_cm(point, start, end)
        for start, end in zip(points, points[1:])
    )


def main_river_spline(config: dict) -> dict:
    """Return the single approved B0R main river across loader schema variants."""

    rivers = config["river_splines"]
    if isinstance(rivers, dict):
        candidates = [rivers]
    elif isinstance(rivers, list):
        candidates = rivers
    else:
        raise ValueError("river_splines must be an object or list")
    matches = [river for river in candidates if river.get("id") == "River_Main"]
    if len(matches) != 1:
        raise ValueError("B1 config must contain exactly one River_Main spline")
    return matches[0]


def _smoothstep(value: float) -> float:
    value = max(0.0, min(1.0, value))
    return value * value * (3.0 - 2.0 * value)


def _oriented_rectangle_weight(x_cm: float, y_cm: float, zone: dict) -> float:
    center_x, center_y = zone["center_cm"][:2]
    half_x = float(zone["size_cm"][0]) * 0.5
    half_y = float(zone["size_cm"][1]) * 0.5
    radians = math.radians(float(zone.get("yaw_degrees", 0.0)))
    cosine = math.cos(radians)
    sine = math.sin(radians)
    delta_x = x_cm - float(center_x)
    delta_y = y_cm - float(center_y)
    local_x = delta_x * cosine + delta_y * sine
    local_y = -delta_x * sine + delta_y * cosine
    normalized_radius = max(abs(local_x) / half_x, abs(local_y) / half_y)
    # A wide 40% edge feather keeps terraces broad and avoids point-like terrain noise.
    return _smoothstep((1.18 - normalized_radius) / 0.48)


def _noise_phase(seed: int, octave_index: int, axis: str) -> float:
    digest = hashlib.sha256(f"{seed}:terrain:{octave_index}:{axis}".encode("utf-8")).digest()
    fraction = int.from_bytes(digest[:8], "big") / float(2**64 - 1)
    return fraction * math.tau


def _low_frequency_noise_cm(seed: int, x_cm: float, y_cm: float) -> float:
    result = 0.0
    for index, octave in enumerate(NOISE_OCTAVES):
        wavelength = float(octave["wavelength_cm"])
        phase_x = _noise_phase(seed, index, "x")
        phase_y = _noise_phase(seed, index, "y")
        first = math.sin(math.tau * x_cm / wavelength + phase_x)
        second = math.cos(math.tau * y_cm / wavelength + phase_y)
        result += float(octave["amplitude_cm"]) * first * second
    return result


def terrain_elevation_cm(config: dict, x_cm: float, y_cm: float) -> float:
    """Evaluate broad B0R terraces, a Gaussian river trench, and two noise octaves."""

    zones = config["terrain_zones"]
    if not zones:
        raise ValueError("B1 config has no terrain zones")
    base_zone = next((zone for zone in zones if zone["id"] == "Terrain_Base"), zones[0])
    elevation = float(base_zone["center_cm"][2])
    for zone in zones:
        if zone is base_zone:
            continue
        weight = _oriented_rectangle_weight(float(x_cm), float(y_cm), zone)
        target = float(zone["center_cm"][2])
        elevation += (target - elevation) * weight

    river_points = main_river_spline(config)["points_cm"]
    river_distance = polyline_distance_cm((x_cm, y_cm), river_points)
    elevation -= RIVER_TRENCH_DEPTH_CM * math.exp(
        -0.5 * (river_distance / RIVER_TRENCH_SIGMA_CM) ** 2
    )
    elevation += _low_frequency_noise_cm(int(config["seed"]), float(x_cm), float(y_cm))
    return elevation


def road_support_elevation_cm(config: dict, x_cm: float, y_cm: float) -> float:
    """Return the broad terrain surface before the river trench is subtracted."""

    elevation = terrain_elevation_cm(config, x_cm, y_cm)
    river_points = main_river_spline(config)["points_cm"]
    river_distance = polyline_distance_cm((x_cm, y_cm), river_points)
    trench = RIVER_TRENCH_DEPTH_CM * math.exp(
        -0.5 * (river_distance / RIVER_TRENCH_SIGMA_CM) ** 2
    )
    return elevation + trench


def encode_height_cm(elevation_cm: float, scale_z_cm: float) -> int:
    encoded = ZERO_RAW + float(elevation_cm) * 128.0 / float(scale_z_cm)
    return max(0, min(65535, int(math.floor(encoded + 0.5))))


def decode_raw_height_cm(raw: int, scale_z_cm: float) -> float:
    return (int(raw) - ZERO_RAW) * float(scale_z_cm) / 128.0


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def generate_heightmap(heightmap_path: Path, samples_path: Path) -> dict:
    config = load_b1_config()
    landscape = config["landscape"]
    resolution = [int(value) for value in landscape["resolution"]]
    if resolution != [505, 505]:
        raise ValueError(f"B1 heightmap requires 505 x 505 vertices, got {resolution}")
    scale = [float(value) for value in landscape["scale_cm"]]
    if scale != [100.0, 100.0, 100.0]:
        raise ValueError(f"B1 heightmap requires 100 cm XYZ scale, got {scale}")
    derived_origin = landscape_actor_origin_local_cm(landscape)
    if [float(value) for value in landscape["origin_local_cm"]] != derived_origin:
        raise ValueError("B1 Landscape origin does not match centre/extent formula")

    raw_values: list[int] = []
    for y in range(resolution[1]):
        for x in range(resolution[0]):
            local_x, local_y = pixel_to_local(landscape, (x, y))
            raw_values.append(
                encode_height_cm(
                    terrain_elevation_cm(config, local_x, local_y), scale[2]
                )
            )

    heightmap_path = Path(heightmap_path).resolve()
    samples_path = Path(samples_path).resolve()
    heightmap_path.parent.mkdir(parents=True, exist_ok=True)
    samples_path.parent.mkdir(parents=True, exist_ok=True)
    image = Image.new("I;16", tuple(resolution))
    image.putdata(raw_values)
    image.save(
        heightmap_path,
        format="PNG",
        optimize=False,
        compress_level=9,
    )

    named_samples = {}
    for name, local in NAMED_SAMPLE_POINTS.items():
        pixel = local_to_pixel(landscape, local)
        raw = raw_values[pixel[1] * resolution[0] + pixel[0]]
        named_samples[name] = {
            "local_cm": [float(local[0]), float(local[1])],
            "pixel_xy": [int(pixel[0]), int(pixel[1])],
            "raw": int(raw),
            "decoded_elevation_cm": decode_raw_height_cm(raw, scale[2]),
        }

    payload = {
        "schema_version": 1,
        "resolution": resolution,
        "scale_cm": scale,
        "center_local_cm": [float(value) for value in landscape["center_local_cm"]],
        "landscape_actor_origin_local_cm": derived_origin,
        "encoding": {
            "value_type": "uint16",
            "zero_raw": ZERO_RAW,
            "scale_z_cm": scale[2],
            "units_per_cm": 128.0 / scale[2],
            "formula": "raw=clamp(round(32768+elevation_cm*128/scale_z_cm),0,65535)",
        },
        "algorithm": {
            "terrain_zone_ids": [zone["id"] for zone in config["terrain_zones"]],
            "river_spline_id": main_river_spline(config)["id"],
            "river_trench_depth_cm": RIVER_TRENCH_DEPTH_CM,
            "river_trench_sigma_cm": RIVER_TRENCH_SIGMA_CM,
            "noise_octaves": [dict(octave) for octave in NOISE_OCTAVES],
            "seed": int(config["seed"]),
        },
        "named_samples": named_samples,
        "png_sha256": _sha256_file(heightmap_path),
    }
    samples_path.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    return {
        "ok": True,
        "heightmap": str(heightmap_path),
        "samples": str(samples_path),
        "png_sha256": payload["png_sha256"],
    }


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_HEIGHTMAP)
    parser.add_argument("--samples", type=Path, default=DEFAULT_SAMPLES)
    return parser.parse_args()


def main() -> None:
    args = _parse_args()
    print(json.dumps(generate_heightmap(args.output, args.samples), sort_keys=True))


if __name__ == "__main__":
    main()
