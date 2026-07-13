"""Report candidate bridge, stair, road, and shoreline geometry for gameplay proxies."""

from __future__ import annotations

import json

import unreal


TOKENS = ("bridge", "stair", "road", "river", "shore", "bank", "thatched_roof_11")


def _vec(value):
    return [round(float(value.x), 2), round(float(value.y), 2), round(float(value.z), 2)]


def main() -> None:
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        raise RuntimeError("no editor world")

    rows = []
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        label = str(actor.get_actor_label())
        class_name = str(actor.get_class().get_name())
        searchable = f"{label} {class_name}".lower()
        if not isinstance(actor, unreal.PlayerStart) and not any(token in searchable for token in TOKENS):
            continue
        origin, extent = actor.get_actor_bounds(False)
        rows.append({
            "label": label,
            "class": class_name,
            "location_cm": _vec(actor.get_actor_location()),
            "bounds_origin_cm": _vec(origin),
            "bounds_extent_cm": _vec(extent),
        })

    rows.sort(key=lambda row: (row["class"], row["label"]))
    samples = []
    for x in (15400.0, 15800.0, 16200.0):
        for y in range(5000, 8401, 200):
            hit = unreal.SystemLibrary.line_trace_single(
                world,
                unreal.Vector(x, float(y), 5000.0),
                unreal.Vector(x, float(y), -2000.0),
                unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                True,
                [],
                unreal.DrawDebugTrace.NONE,
                True,
            )
            payload = {} if hit is None else hit.to_dict()
            point = payload.get("impact_point")
            if point is not None:
                samples.append({"x": x, "y": y, "z": round(float(point.z), 2)})
    print("[GAMEXXK] " + json.dumps({"ok": True, "actors": rows, "height_samples": samples}, ensure_ascii=False))


if __name__ == "__main__":
    main()
