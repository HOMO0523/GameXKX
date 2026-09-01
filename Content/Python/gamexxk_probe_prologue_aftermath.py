"""Read-only observation of the post-carriage prologue while PIE is running."""

from __future__ import annotations

import json
import math

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


def _name(value: object) -> str:
    result = _call(value, "get_name")
    return str(result) if result is not None else ""


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


def _distance(a: object, b: object) -> float:
    if a is None or b is None:
        return -1.0
    return round(
        math.sqrt(
            (float(a.x) - float(b.x)) ** 2
            + (float(a.y) - float(b.y)) ** 2
            + (float(a.z) - float(b.z)) ** 2
        ),
        3,
    )


def observe() -> dict[str, object]:
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world() if editor else None
    if world is None:
        raise RuntimeError("no active PIE world")
    aftermath_class = getattr(unreal, "GameXXKPrologueAftermathController", None)
    npc_class = getattr(unreal, "GameXXKTownNpcCharacter", None)
    if aftermath_class is None or npc_class is None:
        raise RuntimeError("native prologue observation classes are unavailable")

    controllers = list(
        unreal.GameplayStatics.get_all_actors_of_class(world, aftermath_class)
    )
    player_controller = unreal.GameplayStatics.get_player_controller(world, 0)
    player_pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    interaction = None
    interaction_class = getattr(unreal, "GameXXKInteractionComponent", None)
    if player_pawn is not None and interaction_class is not None:
        components = list(player_pawn.get_components_by_class(interaction_class))
        interaction = components[0] if components else None
    focused_actor = _call(interaction, "get_focused_actor")
    yuebai = None
    for npc in unreal.GameplayStatics.get_all_actors_of_class(world, npc_class):
        if str(_call(npc, "get_npc_id")) == "Npc.YueBai":
            yuebai = npc
            break

    controller_reports: list[dict[str, object]] = []
    for controller in controllers:
        state = _property(controller, "aftermath_state")
        panel = _property(controller, "dialogue_panel")
        body = _property(panel, "body_text")
        map_widget = _property(controller, "map_widget")
        passive_prompt = _property(controller, "passive_prompt_widget")
        reveal = _component(controller, "YueBaiReveal")
        intro = _component(controller, "YueBaiIntroDisplay")
        statue_area = _component(controller, "StatueInteractionArea")
        statue_location = _call(statue_area, "get_world_location")
        hero_location = _call(player_pawn, "get_actor_location")
        controller_reports.append(
            {
                "name": _name(controller),
                "label": str(controller.get_actor_label()),
                "phase": str(_property(state, "phase")),
                "paused": bool(_property(state, "paused", "b_paused")),
                "dialogue_text": str(_call(body, "get_text") or ""),
                "map_widget_present": map_widget is not None,
                "passive_prompt_present": passive_prompt is not None,
                "passive_prompt_visible": bool(
                    _call(passive_prompt, "is_bubble_visible_for_test")
                ),
                "reveal_world": _vector(_call(reveal, "get_world_location")),
                "intro_visible": bool(_call(intro, "is_visible")),
                "statue_interaction": {
                    "world": _vector(statue_location),
                    "hero_distance": _distance(statue_location, hero_location),
                    "radius": float(
                        _call(statue_area, "get_unscaled_sphere_radius") or 0.0
                    ),
                    "collision": str(
                        _call(statue_area, "get_collision_enabled") or ""
                    ),
                    "overlaps_hero": bool(
                        _call(statue_area, "is_overlapping_actor", player_pawn)
                    ),
                },
            }
        )

    report = {
        "ok": True,
        "world": str(world.get_outermost().get_name()),
        "aftermath_count": len(controllers),
        "aftermath": controller_reports,
        "player_controller": {
            "present": player_controller is not None,
            "active_aftermath": bool(
                _call(
                    player_controller,
                    "has_active_prologue_aftermath_for_test",
                )
            ),
            "input_locked": bool(
                _call(
                    player_controller,
                    "is_prologue_aftermath_input_locked_for_test",
                )
            ),
            "move_input_ignored": bool(
                _call(player_controller, "is_move_input_ignored")
            ),
            "look_input_ignored": bool(
                _call(player_controller, "is_look_input_ignored")
            ),
            "focused_actor": {
                "name": _name(focused_actor),
                "label": str(_call(focused_actor, "get_actor_label") or ""),
                "class": str(
                    _call(_call(focused_actor, "get_class"), "get_name") or ""
                ),
            },
        },
        "hero": {
            "present": player_pawn is not None,
            "location": _vector(_call(player_pawn, "get_actor_location")),
        },
        "yuebai": {
            "present": yuebai is not None,
            "location": _vector(_call(yuebai, "get_actor_location")),
            "hidden": bool(_call(yuebai, "is_hidden")),
            "following": bool(
                _property(
                    yuebai,
                    "narrative_follower_active",
                    "b_narrative_follower_active",
                )
            ),
        },
    }
    print(json.dumps(report, ensure_ascii=False, sort_keys=True))
    return report


if __name__ == "__main__":
    print(json.dumps(observe(), ensure_ascii=False, indent=2, sort_keys=True))
