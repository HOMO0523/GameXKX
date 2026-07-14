"""Read-only PIE diagnostic for the Asian Village Town-exit interaction."""

from __future__ import annotations

import json
import sys

import unreal


def _object_path(value) -> str:
    if value is None:
        return ""
    for method_name in ("get_path_name", "get_name"):
        method = getattr(value, method_name, None)
        if method is None:
            continue
        try:
            return str(method())
        except Exception:
            continue
    return str(value)


def _label(actor) -> str:
    if actor is None:
        return ""
    try:
        return str(actor.get_actor_label())
    except Exception:
        return _object_path(actor)


def _vector(value) -> dict[str, float]:
    return {
        "x": round(float(value.x), 2),
        "y": round(float(value.y), 2),
        "z": round(float(value.z), 2),
    }


def _box_summary(actor) -> list[dict[str, object]]:
    items: list[dict[str, object]] = []
    for component in actor.get_components_by_class(unreal.BoxComponent):
        items.append(
            {
                "name": str(component.get_name()),
                "class": _object_path(component.get_class()),
            }
        )
    return items


def main() -> None:
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor_subsystem.get_game_world() if editor_subsystem else None
    if world is None:
        raise RuntimeError("no active PIE world")

    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if pawn is None:
        raise RuntimeError("PIE did not create a player pawn")

    if "--teleport" in sys.argv[1:]:
        index = sys.argv.index("--teleport")
        pawn.set_actor_location(
            unreal.Vector(
                float(sys.argv[index + 1]),
                float(sys.argv[index + 2]),
                float(sys.argv[index + 3]),
            ),
            False,
            True,
        )

    exits = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.GameXXKTownExitActor)
    interaction = pawn.get_interaction_component()
    focused_actor = interaction.get_focused_actor() if interaction else None
    pawn_location = pawn.get_actor_location()
    interact_requested = "--interact" in sys.argv[1:]
    interaction_invoked = False
    if interact_requested:
        pawn.interact()
        interaction_invoked = True

    reports: list[dict[str, object]] = []
    for exit_actor in exits:
        location = exit_actor.get_actor_location()
        delta = location - pawn_location
        distance_2d = (float(delta.x) ** 2 + float(delta.y) ** 2) ** 0.5
        reports.append(
            {
                "name": str(exit_actor.get_name()),
                "label": _label(exit_actor),
                "class": _object_path(exit_actor.get_class()),
                "location_cm": _vector(location),
                "rotation": {
                    "pitch": round(float(exit_actor.get_actor_rotation().pitch), 2),
                    "yaw": round(float(exit_actor.get_actor_rotation().yaw), 2),
                    "roll": round(float(exit_actor.get_actor_rotation().roll), 2),
                },
                "distance_to_pawn_2d_cm": round(distance_2d, 2),
                "within_default_proximity_360cm": distance_2d <= 360.0,
                "interaction_boxes": _box_summary(exit_actor),
                "last_interaction_succeeded": bool(exit_actor.was_last_interaction_successful()),
                "last_failure_reason": str(exit_actor.get_last_failure_reason()),
            }
        )

    report = {
        "ok": True,
        "world": _object_path(world.get_outermost()),
        "pawn": {
            "name": str(pawn.get_name()),
            "class": _object_path(pawn.get_class()),
            "location_cm": _vector(pawn_location),
        },
        "focused_actor": {
            "name": "" if focused_actor is None else str(focused_actor.get_name()),
            "label": _label(focused_actor),
            "class": "" if focused_actor is None else _object_path(focused_actor.get_class()),
        },
        "interaction_requested": interact_requested,
        "interaction_invoked": interaction_invoked,
        "exit_count": len(reports),
        "exits": reports,
    }
    print("[GAMEXXK] " + json.dumps(report, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
