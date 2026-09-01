"""Inspect and drive the live controllable town hero through UE MCP."""

from __future__ import annotations

import json
import sys

import unreal


def _game_world():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return subsystem.get_game_world() if subsystem else None


def _asset_path(asset) -> str:
    try:
        return str(asset.get_path_name()) if asset else ""
    except Exception:
        return ""


def sample() -> dict:
    world = _game_world()
    if world is None:
        return {"ok": False, "reason": "pie_world_unavailable"}
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if pawn is None or not hasattr(pawn, "get_town_visual_component"):
        return {
            "ok": False,
            "reason": "controllable_town_hero_unavailable",
            "pawnClass": pawn.get_class().get_name() if pawn else "",
        }
    visual = pawn.get_town_visual_component()
    flipbook = pawn.get_current_town_flipbook()
    scale = (
        visual.get_editor_property("relative_scale3d")
        if visual
        else unreal.Vector(0.0, 0.0, 0.0)
    )
    return {
        "ok": True,
        "pawnClass": pawn.get_class().get_name(),
        "flipbook": _asset_path(flipbook),
        "facing": str(pawn.get_town_facing_direction()),
        "moving": bool(pawn.is_town_moving()),
        "looping": bool(visual.is_looping()) if visual else False,
        "playing": bool(visual.is_playing()) if visual else False,
        "frame": int(visual.get_playback_position_in_frames()) if visual else -1,
        "scale": [float(scale.x), float(scale.y), float(scale.z)],
    }


def set_key(key_name: str, pressed: bool) -> dict:
    world = _game_world()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
    if pawn is None or not hasattr(pawn, "set_town_automation_key_state"):
        return {"ok": False, "reason": "controllable_town_hero_unavailable"}
    accepted = bool(
        pawn.set_town_automation_key_state(unreal.Name(key_name.upper()), pressed)
    )
    return {
        "ok": accepted,
        "key": key_name.upper(),
        "pressed": bool(pressed),
        "sample": sample(),
    }


def toggle_town() -> dict:
    world = _game_world()
    controller = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    if controller is None or not hasattr(
        controller, "request_desktop_town_toggle_from_workbench"
    ):
        return {"ok": False, "reason": "desktop_town_toggle_unavailable"}
    source_world = world.get_path_name()
    accepted = bool(controller.request_desktop_town_toggle_from_workbench())
    return {
        "ok": accepted,
        "sourceWorld": str(source_world),
        "requested": True,
    }


def play_action(action_name: str) -> dict:
    world = _game_world()
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if world else None
    enum_type = getattr(unreal, "GameXXKHeroTownAction", None)
    enum_value = getattr(enum_type, action_name.upper(), None) if enum_type else None
    if pawn is None or enum_value is None or not hasattr(pawn, "play_town_action"):
        return {"ok": False, "reason": "town_action_unavailable", "action": action_name}
    accepted = bool(pawn.play_town_action(enum_value))
    return {
        "ok": accepted,
        "action": action_name,
        "sample": sample(),
    }


def main(argv: list[str]) -> dict:
    if argv == ["--toggle-town"]:
        return toggle_town()
    if len(argv) == 2 and argv[0] == "--action":
        return play_action(argv[1])
    if len(argv) == 3 and argv[0] == "--key":
        state = argv[2].casefold()
        if state not in {"down", "up"}:
            return {"ok": False, "reason": "key_state_must_be_down_or_up"}
        return set_key(argv[1], state == "down")
    return sample()


if __name__ == "__main__":
    print(json.dumps(main(sys.argv[1:]), ensure_ascii=False, indent=2))
