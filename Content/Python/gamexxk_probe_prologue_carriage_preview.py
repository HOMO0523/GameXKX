"""Read-only observation of an already-running prologue carriage PIE session."""

from __future__ import annotations

import argparse
import json

import unreal


def _call(value: object, method_name: str, *args: object) -> object:
    method = getattr(value, method_name, None) if value is not None else None
    if not callable(method):
        return None
    try:
        return method(*args)
    except Exception:
        return None


def _property(value: object, *names: str) -> object:
    if value is None:
        return None
    for name in names:
        try:
            return value.get_editor_property(name)
        except Exception:
            candidate = getattr(value, name, None)
            if candidate is not None:
                return candidate
    return None


def _object_name(value: object) -> str:
    if value is None:
        return ""
    for name in ("get_path_name", "get_name"):
        result = _call(value, name)
        if result is not None:
            return str(result)
    return str(value)


def _vector(value: object) -> dict[str, float]:
    if value is None:
        return {"x": 0.0, "y": 0.0, "z": 0.0}
    return {
        "x": round(float(value.x), 3),
        "y": round(float(value.y), 3),
        "z": round(float(value.z), 3),
    }


def _component(actor: object, name: str) -> object:
    if actor is None:
        return None
    for component in actor.get_components_by_class(unreal.SceneComponent):
        if str(component.get_name()) == name:
            return component
    return None


def _component_report(actor: object, name: str) -> dict[str, object]:
    component = _component(actor, name)
    return {
        "name": name,
        "present": component is not None,
        "relative_location": _vector(_property(component, "relative_location")),
        "world_location": _vector(_call(component, "get_world_location")),
        "visible": bool(_call(component, "is_visible")) if component else False,
    }


def observe() -> dict[str, object]:
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world() if editor else None
    if world is None:
        raise RuntimeError("no active PIE world")
    rig_class = getattr(unreal, "GameXXKPrologueCarriageRig", None)
    if rig_class is None:
        raise RuntimeError("native prologue carriage Rig class is unavailable")
    rigs = list(unreal.GameplayStatics.get_all_actors_of_class(world, rig_class))
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)

    rig_reports: list[dict[str, object]] = []
    for rig in rigs:
        timeline = _call(rig, "get_timeline_state_for_test")
        rig_reports.append(
            {
                "name": str(rig.get_name()),
                "label": str(rig.get_actor_label()),
                "active": bool(_call(rig, "is_presentation_active")),
                "phase": str(_property(timeline, "phase")),
                "phase_elapsed_seconds": float(
                    _property(timeline, "phase_elapsed_seconds") or 0.0
                ),
                "paused": bool(_call(rig, "is_sequence_paused")),
                "presented_frame": int(
                    _call(rig, "get_presented_frame_for_test") or -1
                ),
                "pause_overlay_visible": bool(
                    _call(rig, "is_pause_overlay_visible_for_test")
                ),
                "actor_location": _vector(rig.get_actor_location()),
                "components": {
                    name: _component_report(rig, name)
                    for name in (
                        "CarriageStart",
                        "CarriageStop",
                        "CarriageExit",
                        "HeroReveal",
                        "CarriageDisplay",
                        "IntroCamera",
                    )
                },
            }
        )

    root = _property(pawn, "root_component")
    movement = _call(pawn, "get_character_movement")
    if movement is None and pawn is not None:
        movement_components = pawn.get_components_by_class(
            unreal.CharacterMovementComponent
        )
        movement = movement_components[0] if movement_components else None
    report = {
        "ok": True,
        "world": _object_name(world.get_outermost()),
        "rig_count": len(rigs),
        "rigs": rig_reports,
        "controller": {
            "present": controller is not None,
            "active_rig_owned": bool(
                _call(controller, "has_active_prologue_carriage_for_test")
            ),
            "view_target": _object_name(_call(controller, "get_view_target")),
            "move_input_ignored": bool(_call(controller, "is_move_input_ignored")),
            "look_input_ignored": bool(_call(controller, "is_look_input_ignored")),
            "show_mouse_cursor": bool(
                _property(controller, "show_mouse_cursor", "b_show_mouse_cursor")
            ),
        },
        "pawn": {
            "present": pawn is not None,
            "name": _object_name(pawn),
            "location": _vector(_call(pawn, "get_actor_location")),
            "collision": str(_call(root, "get_collision_enabled")),
            "movement_mode": str(_property(movement, "movement_mode")),
        },
    }
    print(json.dumps(report, ensure_ascii=False, sort_keys=True))
    return report


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("phase", choices=("observe",), nargs="?", default="observe")
    parser.parse_args()
    observe()


if __name__ == "__main__":
    main()
