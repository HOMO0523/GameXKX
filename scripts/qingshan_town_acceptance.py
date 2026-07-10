"""Host-side helpers for compact Qingshan town acceptance evidence."""

from __future__ import annotations

import csv
import hashlib
import json
import math
import statistics
from pathlib import Path
from typing import Any, Iterable


def canonical_transform_hash(
    transforms: Iterable[dict[str, Any]], *, decimals: int = 4
) -> dict[str, Any]:
    """Return a sorted, tolerance-declared transform representation and SHA-256."""

    fields = ("location_cm", "rotation_degrees", "scale")
    canonical = []
    for transform in transforms:
        canonical.append(
            {
                field: [round(float(value), decimals) for value in transform[field]]
                for field in fields
            }
        )
    canonical.sort(
        key=lambda value: tuple(
            component for field in fields for component in value[field]
        )
    )
    encoded = json.dumps(
        canonical, sort_keys=True, separators=(",", ":"), allow_nan=False
    ).encode("utf-8")
    return {
        "decimal_places": decimals,
        "absolute_tolerance": 10.0 ** (-decimals),
        "instance_count": len(canonical),
        "transforms": canonical,
        "sha256": hashlib.sha256(encoded).hexdigest(),
    }


def _nearest_rank(values: list[float], percentile: float) -> float:
    index = max(0, min(len(values) - 1, math.ceil(percentile * len(values)) - 1))
    return sorted(values)[index]


def parse_csv_profile(path: str | Path) -> dict[str, Any]:
    """Parse UE CSV-profiler frame times into reproducible summary metrics."""

    rows = list(csv.reader(Path(path).read_text(encoding="utf-8-sig", errors="replace").splitlines()))
    header_index = next(
        index for index, row in enumerate(rows) if "FrameTime" in row
    )
    header = rows[header_index]
    frame_time_index = header.index("FrameTime")
    data_rows = rows[header_index + 1 :]
    frame_times = []
    for row in data_rows:
        if len(row) <= frame_time_index:
            continue
        try:
            value = float(row[frame_time_index])
        except ValueError:
            continue
        if math.isfinite(value) and value > 0.0:
            frame_times.append(value)
    if not frame_times:
        raise ValueError(f"no positive FrameTime samples found in {path}")
    average = statistics.fmean(frame_times)
    counter_names = (
        "GameThreadTime",
        "RenderThreadTime",
        "GPUTime",
        "RHIThreadTime",
        "RHI/DrawCalls",
        "RHI/PrimitivesDrawn",
        "GPUSceneInstanceCount",
        "TextureStreaming/StreamingPool",
        "TextureStreaming/ResidentMeshMem",
        "TextureStreaming/StreamedMeshMem",
        "GPUMem/LocalBudgetMB",
        "GPUMem/LocalUsedMB",
        "PhysicalUsedMB",
    )
    counters = {}
    for name in counter_names:
        if name not in header:
            continue
        column = header.index(name)
        values = []
        for row in data_rows:
            if len(row) <= column:
                continue
            try:
                value = float(row[column])
            except ValueError:
                continue
            if math.isfinite(value):
                values.append(value)
        if values:
            counters[name] = {
                "average": round(statistics.fmean(values), 4),
                "median": round(statistics.median(values), 4),
                "max": round(max(values), 4),
            }
    return {
        "frame_count": len(frame_times),
        "average_frame_time_ms": round(average, 4),
        "median_frame_time_ms": round(statistics.median(frame_times), 4),
        "p95_frame_time_ms": round(_nearest_rank(frame_times, 0.95), 4),
        "p99_frame_time_ms": round(_nearest_rank(frame_times, 0.99), 4),
        "average_fps": round(1000.0 / average, 4),
        "hitch_count_over_50ms": sum(value > 50.0 for value in frame_times),
        "counters": counters,
    }
