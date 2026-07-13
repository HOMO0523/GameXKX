"""Capture the actual PIE pawn state for the migrated demo map."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal


OUT = Path(__file__).resolve().parents[2] / "docs/production/evidence/asian-village-integration/demo-pie-pawn.json"


def main() -> None:
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = subsystem.get_game_world() if subsystem else None
    if world is None:
        raise RuntimeError("no active PIE world")
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if pawn is None:
        raise RuntimeError("PIE did not create a player pawn")
    if "--reproduce-stair" in sys.argv[1:]:
        pawn.set_actor_location(unreal.Vector(15897.8, 7024.09, 1162.0), False, False)
    if "--teleport" in sys.argv[1:]:
        index = sys.argv.index("--teleport")
        pawn.set_actor_location(
            unreal.Vector(float(sys.argv[index + 1]), float(sys.argv[index + 2]), float(sys.argv[index + 3])),
            False,
            True,
        )
    blocker_sweep_requested = "--test-building-blocker" in sys.argv[1:]
    blocker_sweep_result = None
    if blocker_sweep_requested:
        pawn.set_actor_location(unreal.Vector(15720.0, 7150.6, 1075.0), False, True)
        blocker_sweep_result = pawn.set_actor_location(unreal.Vector(16126.2, 7150.6, 1075.0), True, False)
    if "--start-move-right" in sys.argv[1:]:
        pawn.move_horizontal(1.0)
    if "--stop-move" in sys.argv[1:]:
        pawn.move_horizontal(0.0)
    location = pawn.get_actor_location()
    velocity = pawn.get_velocity()
    ground_hit = unreal.SystemLibrary.line_trace_single(
        world,
        unreal.Vector(float(location.x), float(location.y), float(location.z) + 2000.0),
        unreal.Vector(float(location.x), float(location.y), float(location.z) - 2000.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        True,
        [pawn],
        unreal.DrawDebugTrace.NONE,
        True,
    )
    ground_payload = {} if ground_hit is None else ground_hit.to_dict()
    ground_point = ground_payload.get("impact_point")
    ground_actor = ground_payload.get("actor") or ground_payload.get("hit_actor")
    ground_component = ground_payload.get("component") or ground_payload.get("hit_component")
    game_mode = unreal.GameplayStatics.get_game_mode(world)
    player_starts = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerStart)
    try:
        root = pawn.get_editor_property("root_component")
    except Exception:
        root = None
    report = {
        "ok": True,
        "world": str(world.get_outermost().get_path_name()),
        "pawn_class": str(pawn.get_class().get_path_name()),
        "game_mode_class": "" if game_mode is None else str(game_mode.get_class().get_path_name()),
        "player_starts_cm": [
            [round(float(start.get_actor_location().x), 2), round(float(start.get_actor_location().y), 2), round(float(start.get_actor_location().z), 2)]
            for start in player_starts
        ],
        "location_cm": [round(float(location.x), 2), round(float(location.y), 2), round(float(location.z), 2)],
        "velocity_cm_s": [round(float(velocity.x), 2), round(float(velocity.y), 2), round(float(velocity.z), 2)],
        "ground_hit_cm": [] if ground_point is None else [
            round(float(ground_point.x), 2), round(float(ground_point.y), 2), round(float(ground_point.z), 2)
        ],
        "ground_actor": "" if ground_actor is None else str(ground_actor.get_actor_label()),
        "ground_component": "" if ground_component is None else str(ground_component.get_name()),
        "ground_payload_keys": sorted(str(key) for key in ground_payload.keys()),
        "root_collision": "" if root is None else str(root.get_collision_enabled()),
        "blocker_sweep_requested": blocker_sweep_requested,
        "blocker_sweep_result": "" if blocker_sweep_result is None else str(blocker_sweep_result),
    }
    OUT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("[GAMEXXK] " + json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
