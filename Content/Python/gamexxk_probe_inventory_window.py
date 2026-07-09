from __future__ import annotations

import argparse
import json
import sys

import unreal


def _path(obj) -> str:
    if not obj:
        return ""
    try:
        return obj.get_path_name()
    except Exception:
        return str(obj)


def _class_name(obj) -> str:
    if not obj:
        return ""
    try:
        return obj.get_class().get_name()
    except Exception:
        return type(obj).__name__


def _enum_name(value) -> str:
    try:
        return str(value).split("::")[-1]
    except Exception:
        return str(value)


def _get_game_world():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if not subsystem:
        return None
    try:
        return subsystem.get_game_world()
    except Exception:
        return None


def _map_name(world) -> str:
    if not world:
        return ""
    if hasattr(world, "get_map_name"):
        try:
            return str(world.get_map_name())
        except Exception:
            pass
    try:
        return str(world.get_name())
    except Exception:
        return _path(world)


def _first_player_controller(world):
    if not world:
        return None
    try:
        return unreal.GameplayStatics.get_player_controller(world, 0)
    except Exception:
        return None


def _first_player_pawn(world):
    controller = _first_player_controller(world)
    if controller:
        try:
            return controller.get_pawn()
        except Exception:
            pass
    try:
        return unreal.GameplayStatics.get_player_pawn(world, 0)
    except Exception:
        return None


def _all_actors(world):
    if not world:
        return []
    try:
        return unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    except Exception:
        return []


def _get_focused_actor(pawn):
    if not pawn or not hasattr(pawn, "get_interaction_component"):
        return None
    try:
        interaction = pawn.get_interaction_component()
    except Exception:
        interaction = None
    if not interaction or not hasattr(interaction, "get_focused_actor"):
        return None
    try:
        return interaction.get_focused_actor()
    except Exception:
        return None


def _find_merchant_actor(world):
    for actor in _all_actors(world):
        actor_text = f"{_class_name(actor)} {actor.get_name()}".upper()
        role = ""
        if hasattr(actor, "get_npc_role"):
            try:
                role = str(actor.get_npc_role()).upper()
            except Exception:
                role = ""
        if "MERCHANT" in role or "MERCHANT" in actor_text:
            return actor
    return None


def _find_quest_actor(world):
    for actor in _all_actors(world):
        actor_text = f"{_class_name(actor)} {actor.get_name()}".upper()
        role = ""
        if hasattr(actor, "get_npc_role"):
            try:
                role = str(actor.get_npc_role()).upper()
            except Exception:
                role = ""
        if "QUEST" in role or "QUEST" in actor_text:
            return actor
    return None


def _force_focus_actor(pawn, actor):
    if not pawn or not hasattr(pawn, "get_interaction_component"):
        return False
    try:
        interaction = pawn.get_interaction_component()
    except Exception:
        interaction = None
    if not interaction:
        return False
    if hasattr(interaction, "set_focused_actor"):
        try:
            interaction.set_focused_actor(actor)
            return True
        except Exception:
            return False
    if hasattr(interaction, "add_focused_actor"):
        try:
            interaction.add_focused_actor(actor)
            return True
        except Exception:
            return False
    return False


def _widget_summary(widget):
    result = {
        "exists": bool(widget),
        "path": _path(widget),
        "class": _class_name(widget),
    }
    if not widget:
        return result
    for method_name in (
        "is_in_viewport",
        "has_window_frame_for_test",
        "has_close_button_for_test",
        "is_modal_input_lock_active_for_test",
    ):
        if hasattr(widget, method_name):
            try:
                result[method_name] = bool(getattr(widget, method_name)())
            except Exception as exc:
                result[method_name] = {"error": str(exc)}
    if hasattr(widget, "get_visibility"):
        try:
            result["visibility"] = _enum_name(widget.get_visibility())
        except Exception as exc:
            result["visibility"] = {"error": str(exc)}
    if hasattr(widget, "get_window_mode_for_test"):
        try:
            result["window_mode"] = _enum_name(widget.get_window_mode_for_test())
        except Exception as exc:
            result["window_mode"] = {"error": str(exc)}
    return result


def _controller_summary(controller):
    result = {
        "exists": bool(controller),
        "path": _path(controller),
        "class": _class_name(controller),
    }
    if not controller:
        return result
    if hasattr(controller, "get_inventory_window_widget_for_test"):
        try:
            result["inventory_window"] = _widget_summary(controller.get_inventory_window_widget_for_test())
        except Exception as exc:
            result["inventory_window"] = {"error": str(exc)}
    if hasattr(controller, "is_inventory_window_modal_input_locked_for_test"):
        try:
            result["modal_lock"] = bool(controller.is_inventory_window_modal_input_locked_for_test())
        except Exception as exc:
            result["modal_lock"] = {"error": str(exc)}
    return result


def _actor_summary(actor):
    result = {
        "exists": bool(actor),
        "path": _path(actor),
        "class": _class_name(actor),
    }
    if not actor:
        return result
    try:
        result["label"] = actor.get_actor_label()
    except Exception:
        result["label"] = actor.get_name()
    if hasattr(actor, "get_npc_role"):
        try:
            result["npc_role"] = _enum_name(actor.get_npc_role())
        except Exception as exc:
            result["npc_role"] = {"error": str(exc)}
    if hasattr(actor, "was_last_interaction_successful"):
        try:
            result["was_last_interaction_successful"] = bool(actor.was_last_interaction_successful())
        except Exception as exc:
            result["was_last_interaction_successful"] = {"error": str(exc)}
    try:
        location = actor.get_actor_location()
        result["location"] = {"x": location.x, "y": location.y, "z": location.z}
    except Exception:
        pass
    if hasattr(actor, "get_interaction_area"):
        try:
            area = actor.get_interaction_area()
            result["interaction_area"] = {
                "exists": bool(area),
                "class": _class_name(area),
                "collision_enabled": _enum_name(area.get_collision_enabled()) if area else "",
                "pawn_response": _enum_name(area.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)) if area else "",
                "generate_overlap": bool(area.get_generate_overlap_events()) if area else False,
            }
            if area and hasattr(area, "get_unscaled_sphere_radius"):
                result["interaction_area"]["radius"] = float(area.get_unscaled_sphere_radius())
            if area and hasattr(area, "get_unscaled_box_extent"):
                extent = area.get_unscaled_box_extent()
                result["interaction_area"]["extent"] = {"x": extent.x, "y": extent.y, "z": extent.z}
        except Exception as exc:
            result["interaction_area"] = {"error": str(exc)}
    return result


def main(argv: list[str]) -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--pawn-interact", action="store_true")
    parser.add_argument("--merchant-apply-default", action="store_true")
    parser.add_argument("--controller-open-merchant", action="store_true")
    parser.add_argument("--controller-close-inventory", action="store_true")
    parser.add_argument("--focus-merchant", action="store_true")
    parser.add_argument("--focus-quest", action="store_true")
    parser.add_argument("--clear-focus", action="store_true")
    parser.add_argument("--teleport-near-merchant", action="store_true")
    args = parser.parse_args(argv)

    world = _get_game_world()
    controller = _first_player_controller(world)
    pawn = _first_player_pawn(world)
    focused_actor = _get_focused_actor(pawn)
    merchant_actor = _find_merchant_actor(world)
    quest_actor = _find_quest_actor(world)

    actions = {}
    if args.controller_close_inventory and controller and hasattr(controller, "close_inventory_window"):
        try:
            actions["controller_close_inventory"] = {"ok": bool(controller.close_inventory_window())}
        except Exception as exc:
            actions["controller_close_inventory"] = {"ok": False, "error": str(exc)}
    if args.clear_focus:
        actions["clear_focus"] = {"ok": _force_focus_actor(pawn, None)}
        focused_actor = _get_focused_actor(pawn)
    if args.teleport_near_merchant and pawn and merchant_actor:
        try:
            location = merchant_actor.get_actor_location()
            location.x += 300.0
            pawn.set_actor_location(location, False, False)
            actions["teleport_near_merchant"] = {"ok": True}
        except Exception as exc:
            actions["teleport_near_merchant"] = {"ok": False, "error": str(exc)}
    if args.focus_merchant:
        actions["focus_merchant"] = {"ok": _force_focus_actor(pawn, merchant_actor)}
        focused_actor = _get_focused_actor(pawn)
    if args.focus_quest:
        actions["focus_quest"] = {"ok": _force_focus_actor(pawn, quest_actor)}
        focused_actor = _get_focused_actor(pawn)
    if args.pawn_interact and pawn and hasattr(pawn, "interact"):
        try:
            pawn.interact()
            actions["pawn_interact"] = {"ok": True}
        except Exception as exc:
            actions["pawn_interact"] = {"ok": False, "error": str(exc)}
    if args.merchant_apply_default and merchant_actor and hasattr(merchant_actor, "apply_default_interaction"):
        try:
            actions["merchant_apply_default"] = {"ok": bool(merchant_actor.apply_default_interaction(pawn))}
        except Exception as exc:
            actions["merchant_apply_default"] = {"ok": False, "error": str(exc)}
    if args.controller_open_merchant and controller and hasattr(controller, "open_merchant_trade_window"):
        try:
            actions["controller_open_merchant"] = {"ok": bool(controller.open_merchant_trade_window())}
        except Exception as exc:
            actions["controller_open_merchant"] = {"ok": False, "error": str(exc)}

    result = {
        "ok": bool(world),
        "map_name": _map_name(world),
        "controller": _controller_summary(controller),
        "pawn": _actor_summary(pawn) | {"focused_actor": _actor_summary(focused_actor)},
        "merchant_actor": _actor_summary(merchant_actor),
        "quest_actor": _actor_summary(quest_actor),
        "actions": actions,
    }
    if controller and hasattr(controller, "get_inventory_window_widget_for_test"):
        try:
            result["controller"]["inventory_window_after_actions"] = _widget_summary(controller.get_inventory_window_widget_for_test())
        except Exception as exc:
            result["controller"]["inventory_window_after_actions"] = {"error": str(exc)}
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main(sys.argv[1:])
