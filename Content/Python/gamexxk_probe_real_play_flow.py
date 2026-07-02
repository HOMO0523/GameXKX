from __future__ import annotations

import argparse
import json
import math
import sys

import unreal


DEFAULT_SAVE_SLOT = "GameXXK_MVP_Autosave"


def _vector_to_dict(value):
    if value is None:
        return None
    return {
        "x": float(getattr(value, "x", 0.0)),
        "y": float(getattr(value, "y", 0.0)),
        "z": float(getattr(value, "z", 0.0)),
    }


def _object_path(value):
    if value is None:
        return ""
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def _class_path(value):
    if value is None:
        return ""
    try:
        return value.get_class().get_path_name()
    except Exception:
        return ""


def _class_chain(value):
    names = []
    try:
        klass = value.get_class() if not isinstance(value, unreal.Class) else value
        while klass:
            names.append(klass.get_name())
            klass = klass.get_super_class()
    except Exception:
        pass
    return names


def _get_editor_subsystem():
    try:
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    except Exception:
        return None


def _get_game_world():
    subsystem = _get_editor_subsystem()
    if not subsystem:
        return None
    try:
        return subsystem.get_game_world()
    except Exception:
        return None


def _get_map_name(world):
    if not world:
        return ""
    for getter in ("get_map_name", "get_name"):
        try:
            value = getattr(world, getter)()
            if value:
                return str(value)
        except Exception:
            pass
    return _object_path(world)


def _get_game_instance(world):
    if not world:
        return None
    try:
        return world.get_game_instance()
    except Exception:
        return None


def _get_mvp_subsystem(world):
    game_instance = _get_game_instance(world)
    subsystem_type = getattr(unreal, "GameXXKMVPSubsystem", None)
    if not game_instance or subsystem_type is None:
        return None
    try:
        return game_instance.get_subsystem(subsystem_type)
    except Exception:
        return None


def _enum_name(value):
    if value is None:
        return ""
    try:
        return value.name
    except Exception:
        return str(value)


def _runtime_state(subsystem):
    if not subsystem:
        return {}
    try:
        state = subsystem.get_runtime_state()
    except Exception:
        return {"error": "get_runtime_state_failed"}

    result = {}
    for key in (
        "screen",
        "quest_state",
        "current_region",
        "player_level",
        "player_xp",
        "player_gold",
        "player_hp",
        "player_max_hp",
        "b_follower_joined",
    ):
        try:
            value = getattr(state, key)
        except Exception:
            continue
        if key in ("screen", "quest_state"):
            value = _enum_name(value)
        elif key == "current_region":
            value = str(value)
        result[key] = value
    return result


def _first_player_controller(world):
    if not world:
        return None
    try:
        return unreal.GameplayStatics.get_player_controller(world, 0)
    except Exception:
        return None


def _first_player_pawn(world):
    if not world:
        return None
    try:
        return unreal.GameplayStatics.get_player_pawn(world, 0)
    except Exception:
        return None


def _first_hud(player_controller):
    if not player_controller:
        return None
    for getter in ("get_hud", "get_hud_actor"):
        try:
            value = getattr(player_controller, getter)()
            if value:
                return value
        except Exception:
            pass
    try:
        return player_controller.get_editor_property("my_hud")
    except Exception:
        return None


def _all_actors(world):
    if not world:
        return []
    try:
        return unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    except Exception:
        return []


def _actors_summary(world):
    result = []
    for actor in _all_actors(world):
        try:
            label = actor.get_actor_label()
        except Exception:
            label = actor.get_name()
        class_name = ""
        try:
            class_name = actor.get_class().get_name()
        except Exception:
            pass
        if any(token in class_name for token in ("GameXXK", "PlayerStart")) or any(token in label for token in ("Qingshan", "PlayerStart")):
            summary = {
                "name": actor.get_name(),
                "label": label,
                "class": class_name,
                "location": _vector_to_dict(actor.get_actor_location()),
            }
            if class_name == "GameXXKTownNpcActor":
                for method_name in ("get_npc_role", "was_last_interaction_successful", "is_follower_active"):
                    try:
                        value = getattr(actor, method_name)()
                        if method_name == "get_npc_role":
                            value = _enum_name(value)
                        summary[method_name] = value
                    except Exception:
                        pass
                try:
                    summary["follow_target"] = _object_path(actor.get_follow_target())
                except Exception:
                    pass
            result.append(summary)
    return result


def _hero_summary(pawn):
    if not pawn:
        return {}
    controller = None
    try:
        controller = pawn.get_controller()
    except Exception:
        pass
    result = {
        "name": pawn.get_name(),
        "path": _object_path(pawn),
        "class": _class_path(pawn),
        "class_chain": _class_chain(pawn),
        "location": _vector_to_dict(pawn.get_actor_location()),
        "controller": _object_path(controller),
        "controller_class": _class_path(controller),
    }
    try:
        result["actor_tick_enabled"] = bool(pawn.is_actor_tick_enabled())
    except Exception:
        pass
    try:
        result["move_input_ignored"] = bool(pawn.is_move_input_ignored())
    except Exception:
        pass
    for method_name in (
        "has_town_visual",
        "has_assigned_town_flipbook",
        "get_default_town_flipbook_path_string",
    ):
        try:
            result[method_name] = getattr(pawn, method_name)()
        except Exception:
            pass
    try:
        result["facing"] = _enum_name(pawn.get_town_facing_direction())
    except Exception:
        pass
    try:
        flipbook = pawn.get_current_town_flipbook()
        result["current_flipbook"] = _object_path(flipbook)
    except Exception:
        pass
    try:
        result["input_binding_count"] = int(pawn.count_town_input_bindings_for_test())
    except Exception:
        pass
    try:
        movement = pawn.get_movement_component()
        result["movement_component"] = {
            "class": _class_path(movement),
            "velocity": _vector_to_dict(movement.velocity),
            "max_fly_speed": float(getattr(movement, "max_fly_speed", 0.0)),
            "max_walk_speed": float(getattr(movement, "max_walk_speed", 0.0)),
            "component_tick_enabled": bool(movement.is_component_tick_enabled()),
            "is_moving_on_ground": bool(movement.is_moving_on_ground()),
            "is_flying": bool(movement.is_flying()),
        }
        try:
            result["movement_component"]["updated_component"] = _object_path(movement.updated_component)
        except Exception:
            pass
    except Exception:
        pass
    return result


def _distance(a, b):
    if not a or not b:
        return 0.0
    return math.sqrt(sum((float(a.get(axis, 0.0)) - float(b.get(axis, 0.0))) ** 2 for axis in ("x", "y", "z")))


def probe():
    world = _get_game_world()
    player_controller = _first_player_controller(world)
    pawn = _first_player_pawn(world)
    hud = _first_hud(player_controller)
    subsystem = _get_mvp_subsystem(world)
    return {
        "ok": world is not None,
        "has_pie_world": world is not None,
        "map_name": _get_map_name(world),
        "world_path": _object_path(world),
        "runtime_state": _runtime_state(subsystem),
        "player_controller": {
            "path": _object_path(player_controller),
            "class": _class_path(player_controller),
            "class_chain": _class_chain(player_controller),
        },
        "hud": {
            "path": _object_path(hud),
            "class": _class_path(hud),
            "class_chain": _class_chain(hud),
        },
        "pawn": _hero_summary(pawn),
        "actors": _actors_summary(world),
    }


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--delete-default-save", action="store_true")
    args = parser.parse_args(argv)

    result = {}
    if args.delete_default_save:
        try:
            result["delete_default_save"] = bool(unreal.GameplayStatics.delete_game_in_slot(DEFAULT_SAVE_SLOT, 0))
        except Exception as exc:
            result["delete_default_save_error"] = str(exc)
    result["probe"] = probe()
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main(sys.argv[1:])
