"""Find verified collision-ground spawn candidates in the migrated Asian Village demo."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


MAP = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
OUT = Path(__file__).resolve().parents[2] / "docs/production/evidence/asian-village-integration/demo-spawn-probe.json"


def _current_map() -> str:
    world = unreal.EditorLevelLibrary.get_editor_world()
    return "" if world is None else str(world.get_outermost().get_path_name()).split(".", 1)[0]


def _hit_at(world, x, y):
    start = unreal.Vector(float(x), float(y), 50000.0)
    end = unreal.Vector(float(x), float(y), -50000.0)
    hit = unreal.SystemLibrary.line_trace_single(
        world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    payload = hit.to_dict()
    point = payload.get("impact_point")
    actor = payload.get("actor")
    if point is None:
        return None
    return {
        "location_cm": [round(float(point.x), 2), round(float(point.y), 2), round(float(point.z), 2)],
        "actor": "" if actor is None else str(actor.get_actor_label()),
    }


def main() -> None:
    if _current_map() != MAP:
        raise RuntimeError(f"must inspect open demo map, got {_current_map()}")
    world = unreal.EditorLevelLibrary.get_editor_world()
    # The demo's central plaza and both bridge approaches; candidates are checked
    # with the same collision trace the player will require.
    starts = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if isinstance(actor, unreal.PlayerStart)]
    points = [(0, 0), (1000, 0), (-1000, 0), (0, 1000), (0, -1000), (2000, 1000), (-2000, -1000)]
    points.extend((float(actor.get_actor_location().x), float(actor.get_actor_location().y)) for actor in starts)
    candidates = [item for item in (_hit_at(world, x, y) for x, y in points) if item is not None]
    report = {
        "ok": True,
        "map": MAP,
        "player_start_count": len(starts),
        "player_starts": [
            {"label": str(actor.get_actor_label()), "location_cm": [round(float(actor.get_actor_location().x), 2), round(float(actor.get_actor_location().y), 2), round(float(actor.get_actor_location().z), 2)]}
            for actor in starts
        ],
        "candidates": candidates,
    }
    OUT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("[GAMEXXK] " + json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
