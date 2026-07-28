"""Read-only source-map occupancy and height probe for a battle terrain slice.

This is intentionally a diagnostic step: it does not create assets, save maps,
or alter actor state.  It loads the approved town map, samples candidate ground
areas, and restores the map that was active before the probe.
"""

from __future__ import annotations

import json

import unreal

import gamexxk_audit_battle_town_terrain as audit


# These are broad, interior probes.  They deliberately avoid the town's dense
# positive-X build area; candidates are still rejected by actor bounds and
# actual landscape traces below before one can be selected.
CANDIDATE_CENTERS = [
    {"x": x, "y": y}
    for x in (-36000.0, -30000.0, -24000.0, -18000.0, -12000.0, -6000.0, 0.0, 6000.0)
    for y in (-36000.0, -30000.0, -24000.0, -18000.0, -12000.0, -6000.0, 0.0)
]


def _vector(value: object) -> dict[str, float]:
    return {
        "x": round(float(getattr(value, "x", 0.0)), 3),
        "y": round(float(getattr(value, "y", 0.0)), 3),
        "z": round(float(getattr(value, "z", 0.0)), 3),
    }


def _actor_record(actor: object) -> dict[str, object]:
    try:
        center, extent = actor.get_actor_bounds(False)
    except Exception:
        center, extent = unreal.Vector(), unreal.Vector()
    return {
        "label": audit._actor_label(actor),
        "name": str(actor.get_name()),
        "class": audit._class_path(actor),
        "center": _vector(center),
        "extent": _vector(extent),
        "tags": audit._actor_tags(actor),
    }


def _searchable(record: dict[str, object]) -> str:
    return " ".join(
        [str(record.get("label", "")), str(record.get("name", "")), str(record.get("class", ""))]
    ).lower()


def _is_ignorable_for_surface_selection(record: dict[str, object]) -> bool:
    """Exclude non-ground scene helpers from bounds rejection and trace hits."""

    token = _searchable(record)
    return any(
        value in token
        for value in (
            "camera",
            "light",
            "sky",
            "playerstart",
            "worldsettings",
            "exponentialheightfog",
            "reflectioncapture",
            "postprocess",
            "decalactor",
            "instancedfoliageactor",
            "plane_2",
        )
    )


def _bounds_overlap(
    first: dict[str, dict[str, float]],
    record: dict[str, object],
    clearance_cm: float = 250.0,
) -> bool:
    center = record["center"]
    extent = record["extent"]
    minimum_x = float(center["x"]) - float(extent["x"]) - clearance_cm
    maximum_x = float(center["x"]) + float(extent["x"]) + clearance_cm
    minimum_y = float(center["y"]) - float(extent["y"]) - clearance_cm
    maximum_y = float(center["y"]) + float(extent["y"]) + clearance_cm
    return not (
        float(first["max"]["x"]) < minimum_x
        or float(first["min"]["x"]) > maximum_x
        or float(first["max"]["y"]) < minimum_y
        or float(first["min"]["y"]) > maximum_y
    )


def _trace_height(world: object, x: float, y: float, ignored_actors: list[object]) -> float | None:
    hit = unreal.SystemLibrary.line_trace_single(
        world,
        unreal.Vector(float(x), float(y), 100000.0),
        unreal.Vector(float(x), float(y), -50000.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        True,
        ignored_actors,
        unreal.DrawDebugTrace.NONE,
        True,
    )
    if hit is None:
        return None
    payload = hit.to_dict()
    point = payload.get("impact_point") if isinstance(payload, dict) else None
    if point is None:
        return None
    return float(point.z)


def _height_sample_grid(
    world: object,
    bounds: dict[str, dict[str, float]],
    ignored_actors: list[object],
) -> list[float] | None:
    heights: list[float] = []
    for x_fraction in (0.0, 0.5, 1.0):
        for y_fraction in (0.0, 0.5, 1.0):
            x = float(bounds["min"]["x"]) + (
                float(bounds["max"]["x"]) - float(bounds["min"]["x"])
            ) * x_fraction
            y = float(bounds["min"]["y"]) + (
                float(bounds["max"]["y"]) - float(bounds["min"]["y"])
            ) * y_fraction
            height = _trace_height(world, x, y, ignored_actors)
            if height is None:
                return None
            heights.append(height)
    return heights


def _center_key(center: dict[str, float]) -> tuple[float, float]:
    return (float(center["x"]), float(center["y"]))


def main() -> dict[str, object]:
    audit._require_audit_session_safe()
    original_map = audit._capture_original_map()
    try:
        audit._load_map_for_reading(audit.TOWN_MAP)
        world = unreal.EditorLevelLibrary.get_editor_world()
        if world is None:
            raise RuntimeError("no editor world is available after loading the approved town map")

        actors = list(audit._all_level_actors())
        landscapes = audit.landscape_snapshot()
        by_name = {str(actor.get_name()): actor for actor in actors}
        occupied_records = []
        ignored_actors = []
        for actor in actors:
            if audit._is_landscape(actor):
                continue
            record = _actor_record(actor)
            if _is_ignorable_for_surface_selection(record):
                ignored_actors.append(actor)
                continue
            if float(record["extent"]["x"]) > 1.0 or float(record["extent"]["y"]) > 1.0:
                occupied_records.append(record)

        height_samples: list[dict[str, object]] = []
        eligible_centers: list[dict[str, float]] = []
        candidate_rank: dict[tuple[float, float], tuple[float, float, float]] = {}
        height_by_center: dict[tuple[float, float], dict[tuple[float, float], float]] = {}
        rejected_overlap_count = 0
        for center in CANDIDATE_CENTERS:
            bounds = audit._slice_bounds(center)
            overlaps = any(_bounds_overlap(bounds, record) for record in occupied_records)
            if overlaps:
                rejected_overlap_count += 1
                continue
            heights = _height_sample_grid(world, bounds, ignored_actors)
            if heights is None:
                height_samples.append({"center_cm": center, "result": "trace_miss"})
                continue
            delta = max(heights) - min(heights)
            height_samples.append({
                "center_cm": center,
                "min_z_cm": round(min(heights), 3),
                "max_z_cm": round(max(heights), 3),
                "delta_z_cm": round(delta, 3),
            })
            if delta > audit.MAX_SLICE_HEIGHT_DELTA_CM:
                continue
            key = _center_key(center)
            per_point: dict[tuple[float, float], float] = {}
            index = 0
            for x_fraction in (0.0, 0.5, 1.0):
                for y_fraction in (0.0, 0.5, 1.0):
                    x = float(bounds["min"]["x"]) + (
                        float(bounds["max"]["x"]) - float(bounds["min"]["x"])
                    ) * x_fraction
                    y = float(bounds["min"]["y"]) + (
                        float(bounds["max"]["y"]) - float(bounds["min"]["y"])
                    ) * y_fraction
                    per_point[(x, y)] = heights[index]
                    index += 1
            height_by_center[key] = per_point
            eligible_centers.append(center)
            # Prefer a real but restrained contour over the source landscape's
            # perfectly flat outer apron.  The first score is descending
            # height variation; the remaining terms keep ties near the map's
            # interior instead of a map boundary.
            candidate_rank[key] = (-delta, abs(float(center["x"])), abs(float(center["y"])))

        selected = None
        selection_error = ""
        if eligible_centers:
            active_center: dict[str, float] = min(
                eligible_centers,
                key=lambda center: candidate_rank[_center_key(center)],
            )

            def sample_height(x: float, y: float) -> float | None:
                points = height_by_center.get(_center_key(active_center), {})
                return points.get((float(x), float(y)))

            try:
                selected = audit.select_candidate_slice(
                    landscapes,
                    [active_center],
                    sample_height,
                    lambda _bounds: False,
                )
            except RuntimeError as error:
                selection_error = str(error)
        else:
            selection_error = "no candidate had an unobstructed 3 by 3 height sample within the flatness limit"

        report = {
            "source_map": audit.TOWN_MAP,
            "landscapes": landscapes,
            "candidate_centers": len(CANDIDATE_CENTERS),
            "occupied_actor_count": len(occupied_records),
            "ignored_actor_count": len(ignored_actors),
            "rejected_overlap_count": rejected_overlap_count,
            "height_sample_grid": height_samples,
            "selected_candidate": selected,
            "selection_error": selection_error,
            "ignored_actor_names": sorted(str(actor.get_name()) for actor in ignored_actors),
            "occupied_actor_names": sorted(str(record["name"]) for record in occupied_records)[:40],
        }
    finally:
        audit._restore_original_map(original_map)
    print(json.dumps(report, ensure_ascii=False, sort_keys=True))
    return report


if __name__ == "__main__":
    main()
