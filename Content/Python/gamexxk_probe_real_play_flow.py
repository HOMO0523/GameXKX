from __future__ import annotations

import argparse
import builtins
import json
import math
import re
import sys
import time
from pathlib import Path

import unreal


DEFAULT_SAVE_SLOT = "GameXXK_MVP_SaveSlot_1"
PROJECT_ROOT = Path(__file__).resolve().parents[2]
_HIGH_RES_SCREENSHOT_STATE_ATTR = "_gamexxk_high_res_screenshot_task_state"


def _finite_float(value):
    try:
        number = float(value)
    except (TypeError, ValueError, OverflowError):
        return None
    return number if math.isfinite(number) else None


def _json_safe(value):
    if isinstance(value, float):
        return value if math.isfinite(value) else None
    if isinstance(value, dict):
        return {str(key): _json_safe(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_json_safe(item) for item in value]
    return value


def _strict_json_dumps(value):
    return json.dumps(_json_safe(value), ensure_ascii=False, sort_keys=True, allow_nan=False)


def _vector_to_dict(value):
    if value is None:
        return None
    coordinates = [_finite_float(getattr(value, axis, None)) for axis in ("x", "y", "z")]
    if any(coordinate is None for coordinate in coordinates):
        return None
    return {
        "x": coordinates[0],
        "y": coordinates[1],
        "z": coordinates[2],
    }


def _vector2d_to_dict(value):
    if value is None:
        return None
    return {
        "x": float(getattr(value, "x", 0.0)),
        "y": float(getattr(value, "y", 0.0)),
    }


def _rotator_to_dict(value):
    if value is None:
        return None
    return {
        "pitch": float(getattr(value, "pitch", 0.0)),
        "yaw": float(getattr(value, "yaw", 0.0)),
        "roll": float(getattr(value, "roll", 0.0)),
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
        pass
    try:
        return unreal.GameplayStatics.get_game_instance(world)
    except Exception:
        return None


def _get_mvp_subsystem(world):
    game_instance = _get_game_instance(world)
    subsystem_type = getattr(unreal, "GameXXKMVPSubsystem", None)
    if subsystem_type is None:
        try:
            subsystem_type = unreal.load_class(None, "/Script/GameXXK.GameXXKMVPSubsystem")
        except Exception:
            subsystem_type = None
    if not game_instance or subsystem_type is None:
        return None
    try:
        return game_instance.get_subsystem(subsystem_type)
    except Exception:
        return None


def _get_mvp_subsystem_from_player_controller(player_controller):
    if not player_controller:
        return None
    for getter_name in (
        "get_main_menu_widget_for_test",
        "get_town_overlay_widget_for_test",
        "get_route_map_widget_for_test",
        "get_battle_board_widget_for_test",
    ):
        try:
            widget = getattr(player_controller, getter_name)()
            if not widget:
                continue
            subsystem = widget.get_mvp_subsystem()
            if subsystem:
                return subsystem
        except Exception:
            pass
    return None


def _struct_get(value, *names):
    if value is None:
        return None
    for name in names:
        try:
            return getattr(value, name)
        except Exception:
            pass
        try:
            return value.get_editor_property(name)
        except Exception:
            pass
    return None


def _guid_to_string(value):
    if value is None:
        return "00000000-00000000-00000000-00000000"
    parts = []
    for lower, upper in (("a", "A"), ("b", "B"), ("c", "C"), ("d", "D")):
        part = _struct_get(value, lower, upper)
        try:
            parts.append(f"{int(part) & 0xFFFFFFFF:08X}")
        except (TypeError, ValueError, OverflowError):
            return ""
    return "-".join(parts)


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

    state = None
    for getter_name in ("get_runtime_state_copy", "get_runtime_state"):
        try:
            state = getattr(subsystem, getter_name)()
            break
        except Exception:
            pass
    if state is None:
        return {"error": "get_runtime_state_failed"}

    result = {}
    for key, names in (
        ("screen", ("screen", "Screen")),
        ("quest_state", ("quest_state", "QuestState")),
        ("current_region", ("current_region", "CurrentRegion")),
        ("player_level", ("player_level", "PlayerLevel")),
        ("player_xp", ("player_xp", "PlayerXP")),
        ("player_gold", ("player_gold", "PlayerGold")),
        ("player_hp", ("player_hp", "PlayerHP")),
        ("player_max_hp", ("player_max_hp", "PlayerMaxHP")),
        ("player_mp", ("player_mp", "PlayerMP")),
        ("player_max_mp", ("player_max_mp", "PlayerMaxMP")),
        ("b_has_player_location", ("b_has_player_location", "has_player_location", "bHasPlayerLocation", "HasPlayerLocation")),
        ("player_location", ("player_location", "PlayerLocation")),
        ("b_follower_joined", ("b_follower_joined", "follower_joined", "bFollowerJoined", "FollowerJoined")),
        ("b_has_quest_npc_location", ("b_has_quest_npc_location", "has_quest_npc_location", "bHasQuestNpcLocation", "HasQuestNpcLocation")),
        ("quest_npc_location", ("quest_npc_location", "QuestNpcLocation")),
        ("b_dungeon_active", ("b_dungeon_active", "dungeon_active", "bDungeonActive", "DungeonActive")),
        ("dungeon_node_index", ("dungeon_node_index", "DungeonNodeIndex")),
        ("b_has_generated_route_map", ("b_has_generated_route_map", "bHasGeneratedRouteMap")),
        ("current_route_node_id", ("current_route_node_id", "CurrentRouteNodeId")),
        ("pending_route_node_id", ("pending_route_node_id", "PendingRouteNodeId")),
    ):
        value = _struct_get(state, *names)
        if value is None:
            continue
        if key in ("screen", "quest_state"):
            value = _enum_name(value)
        elif key == "current_region":
            value = str(value)
        elif key in ("player_location", "quest_npc_location"):
            value = _vector_to_dict(value)
        result[key] = value

    for key, names in (
        ("visited_route_node_ids", ("visited_route_node_ids", "VisitedRouteNodeIds")),
        ("reachable_route_node_ids", ("reachable_route_node_ids", "ReachableRouteNodeIds")),
    ):
        values = _struct_get(state, *names)
        try:
            result[key] = [int(value) for value in values]
        except Exception:
            result[key] = []

    checkpoint = _struct_get(state, "battle_entry_checkpoint", "BattleEntryCheckpoint")
    result["battle_entry_checkpoint"] = {
        "b_valid": bool(_struct_get(checkpoint, "b_valid", "bValid")),
        "source_node_id": _struct_get(checkpoint, "source_node_id", "SourceNodeId"),
        "previous_current_route_node_id": _struct_get(
            checkpoint,
            "previous_current_route_node_id",
            "PreviousCurrentRouteNodeId",
        ),
        "previous_dungeon_node_index": _struct_get(
            checkpoint,
            "previous_dungeon_node_index",
            "PreviousDungeonNodeIndex",
        ),
        "previous_player_hp": _struct_get(checkpoint, "previous_player_hp", "PreviousPlayerHP"),
        "previous_player_mp": _struct_get(checkpoint, "previous_player_mp", "PreviousPlayerMP"),
        "previous_visited_route_node_ids": [
            int(value)
            for value in (_struct_get(
                checkpoint,
                "previous_visited_route_node_ids",
                "PreviousVisitedRouteNodeIds",
            ) or [])
        ],
        "previous_reachable_route_node_ids": [
            int(value)
            for value in (_struct_get(
                checkpoint,
                "previous_reachable_route_node_ids",
                "PreviousReachableRouteNodeIds",
            ) or [])
        ],
    }

    card_run = _struct_get(state, "card_run", "CardRun")
    active_battle = _struct_get(card_run, "active_battle", "ActiveBattle")
    route_progress = _struct_get(card_run, "route_progress", "RouteProgress")
    pending_reward = _struct_get(card_run, "pending_reward", "PendingReward")
    result["route_travel_money"] = int(
        _struct_get(card_run, "route_travel_money", "RouteTravelMoney") or 0
    )
    result["route_card_acquisition_count"] = int(
        _struct_get(
            route_progress,
            "actual_route_card_acquisition_count",
            "ActualRouteCardAcquisitionCount",
        )
        or 0
    )
    result["battle_phase"] = _enum_name(_struct_get(active_battle, "phase", "Phase"))
    result["battle_round_number"] = int(
        _struct_get(active_battle, "round_number", "RoundNumber") or 0
    )
    result["has_active_card_battle"] = bool(
        _struct_get(card_run, "b_has_active_card_battle", "bHasActiveCardBattle")
    )
    pending_options = _struct_get(pending_reward, "options", "Options")
    try:
        result["pending_reward_option_count"] = len(list(pending_options))
    except Exception:
        result["pending_reward_option_count"] = 0
    result["last_applied_route_settlement_id"] = _guid_to_string(
        _struct_get(
            card_run,
            "last_applied_route_settlement_id",
            "LastAppliedRouteSettlementId",
        )
    )

    inventory = _struct_get(state, "inventory", "Inventory")
    enhancement_stone_count = 0
    try:
        for item_id, quantity in inventory.items():
            if str(item_id) == "Item.EnhancementStone":
                enhancement_stone_count = int(quantity)
                break
    except Exception:
        pass
    result["enhancement_stone_count"] = enhancement_stone_count
    try:
        result["battle_auto_play_enabled"] = bool(subsystem.is_battle_auto_play_enabled())
    except Exception:
        result["battle_auto_play_enabled"] = None
    units = _struct_get(active_battle, "units", "Units")
    battle_units = {}
    try:
        unit_entries = list(units) if units is not None else []
    except Exception:
        unit_entries = []
    for unit in unit_entries:
        unit_id = str(_struct_get(unit, "unit_id", "UnitId") or "")
        if not unit_id:
            continue
        armor_value = _struct_get(unit, "armor", "Armor")
        battle_units[unit_id] = {
            "unit_id": unit_id,
            "side": _battle_side_name(_struct_get(unit, "side", "Side")),
            "slot": _struct_get(unit, "battle_slot_number", "BattleSlotNumber"),
            "stable_sort_order": _struct_get(unit, "stable_sort_order", "StableSortOrder"),
            "hp": _struct_get(unit, "hp", "HP"),
            "max_hp": _struct_get(unit, "max_hp", "MaxHP"),
            "armor": armor_value,
            "shield": armor_value,
            "living": bool(_struct_get(unit, "b_living", "living", "bLiving", "Living")),
        }
    result["battle_units"] = battle_units

    deck = _struct_get(active_battle, "deck", "Deck")
    hand = _struct_get(deck, "hand", "Hand")
    result["battle_hand"] = []
    try:
        hand_entries = list(hand) if hand is not None else []
    except Exception:
        hand_entries = []
    for card in hand_entries:
        result["battle_hand"].append({
            "instance_id": str(_struct_get(card, "instance_id", "InstanceId") or ""),
            "card_id": str(_struct_get(card, "card_id", "CardId") or ""),
            "owner_unit_id": str(_struct_get(card, "owner_unit_id", "OwnerUnitId") or ""),
        })
    try:
        result["target_outcome_fixture_active"] = bool(subsystem.is_target_outcome_fixture_active_for_test())
    except Exception:
        result["target_outcome_fixture_active"] = None

    return result


def _save_state():
    result = {"exists": False}
    try:
        if not unreal.GameplayStatics.does_save_game_exist(DEFAULT_SAVE_SLOT, 0):
            return result
        save_game = unreal.GameplayStatics.load_game_from_slot(DEFAULT_SAVE_SLOT, 0)
    except Exception as exc:
        return {"exists": False, "error": str(exc)}

    result["exists"] = save_game is not None
    save_state = _struct_get(save_game, "save_state", "SaveState")
    if save_state is None:
        result["error"] = "save_state_missing"
        return result

    for key, names in (
        ("quest_state", ("quest_state", "QuestState")),
        ("b_has_player_location", ("b_has_player_location", "has_player_location", "bHasPlayerLocation", "HasPlayerLocation")),
        ("player_location", ("player_location", "PlayerLocation")),
        ("b_follower_joined", ("b_follower_joined", "follower_joined", "bFollowerJoined", "FollowerJoined")),
        ("b_has_quest_npc_location", ("b_has_quest_npc_location", "has_quest_npc_location", "bHasQuestNpcLocation", "HasQuestNpcLocation")),
        ("quest_npc_location", ("quest_npc_location", "QuestNpcLocation")),
    ):
        value = _struct_get(save_state, *names)
        if value is None:
            continue
        if key == "quest_state":
            value = _enum_name(value)
        elif key in ("player_location", "quest_npc_location"):
            value = _vector_to_dict(value)
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


def _viewport_dimensions(viewport_size):
    """Normalize a live viewport result without supplying a fallback size."""
    try:
        width = _finite_float(viewport_size[0])
        height = _finite_float(viewport_size[1])
    except (IndexError, TypeError):
        return None
    if width is None or height is None or width <= 0.0 or height <= 0.0:
        return None
    return {
        "width": width,
        "height": height,
    }


def _pie_viewport_summary(player_controller, diagnostics=None):
    """Return live PIE dimensions and optionally append structured read diagnostics."""
    if not player_controller:
        _append_structured_diagnostic(diagnostics, "player_controller.get_viewport_size", code="player_controller_missing")
        return None
    try:
        viewport_size = player_controller.get_viewport_size()
    except Exception as exc:
        _append_structured_diagnostic(diagnostics, "player_controller.get_viewport_size", exc=exc)
        return None
    result = _viewport_dimensions(viewport_size)
    if result is None:
        _append_structured_diagnostic(diagnostics, "player_controller.get_viewport_size", code="invalid_viewport_size")
        return None
    result["source"] = "player_controller.get_viewport_size"
    return result


def _text_to_string(value):
    if value is None:
        return ""
    try:
        return value.to_string()
    except Exception:
        return str(value)


def _hud_summary(hud):
    result = {
        "path": _object_path(hud),
        "class": _class_path(hud),
        "class_chain": _class_chain(hud),
    }
    if not hud:
        return result
    try:
        result["status_text"] = _text_to_string(hud.build_status_text())
    except Exception:
        pass
    commands = []
    try:
        for command in hud.build_visible_commands():
            command_name = _struct_get(command, "command_name", "CommandName")
            label = _struct_get(command, "label", "Label")
            enabled = _struct_get(command, "b_enabled", "bEnabled", "Enabled")
            commands.append({
                "command_name": str(command_name),
                "label": _text_to_string(label),
                "b_enabled": bool(enabled),
            })
    except Exception as exc:
        result["visible_commands_error"] = str(exc)
    result["visible_commands"] = commands
    return result


def _widget_summary(widget):
    result = {
        "path": _object_path(widget),
        "class": _class_path(widget),
        "class_chain": _class_chain(widget),
    }
    if not widget:
        return result
    try:
        result["is_in_viewport"] = bool(widget.is_in_viewport())
    except Exception:
        pass
    try:
        result["visibility"] = _enum_name(widget.get_visibility())
    except Exception:
        pass
    try:
        result["is_enabled"] = bool(widget.get_is_enabled())
    except Exception:
        pass
    for method_name in (
        "is_town_overlay_visible",
        "is_battle_board_visible",
        "is_dialog_open",
        "is_task_panel_open_for_test",
        "is_showing_task_offers_for_test",
    ):
        try:
            result[method_name] = bool(getattr(widget, method_name)())
        except Exception:
            pass
    if hasattr(widget, "get_route_node_visual_states_for_test"):
        try:
            result["route_node_visual_states"] = [
                _route_node_visual_state_summary(state)
                for state in widget.get_route_node_visual_states_for_test()
            ]
        except Exception as exc:
            result["route_node_visual_states_error"] = str(exc)
    return result


def _route_node_visual_state_summary(state):
    label = _struct_get(state, "label", "Label")
    node_kind = _struct_get(state, "node_kind", "NodeKind")
    room_type = _struct_get(state, "room_type", "RoomType")
    node_id = _struct_get(state, "node_id", "NodeId")
    visual_index = _struct_get(state, "visual_index", "VisualIndex")
    return {
        "node_id": int(node_id) if node_id is not None else -1,
        "visual_index": int(visual_index) if visual_index is not None else -1,
        "command_name": str(_struct_get(state, "command_name", "CommandName") or ""),
        "label": _text_to_string(label),
        "node_kind": _enum_name(node_kind),
        "room_type": _enum_name(room_type),
        "b_enabled": bool(_struct_get(state, "b_enabled", "bEnabled", "Enabled")),
        "b_visited": bool(_struct_get(state, "b_visited", "bVisited", "Visited")),
        "normalized_position": _vector2d_to_dict(_struct_get(state, "normalized_position", "NormalizedPosition")),
        "canvas_position": _vector2d_to_dict(_struct_get(state, "canvas_position", "CanvasPosition")),
        "hit_box_position": _vector2d_to_dict(_struct_get(state, "hit_box_position", "HitBoxPosition")),
        "hit_box_size": _vector2d_to_dict(_struct_get(state, "hit_box_size", "HitBoxSize")),
        "viewport_hit_box_position": _vector2d_to_dict(_struct_get(state, "viewport_hit_box_position", "ViewportHitBoxPosition")),
        "viewport_hit_box_center": _vector2d_to_dict(_struct_get(state, "viewport_hit_box_center", "ViewportHitBoxCenter")),
        "screen_hit_box_position": _vector2d_to_dict(_struct_get(state, "screen_hit_box_position", "ScreenHitBoxPosition")),
        "screen_hit_box_center": _vector2d_to_dict(_struct_get(state, "screen_hit_box_center", "ScreenHitBoxCenter")),
        "icon_path": str(_struct_get(state, "icon_path", "IconPath") or ""),
    }


def _player_controller_summary(player_controller):
    result = {
        "path": _object_path(player_controller),
        "class": _class_path(player_controller),
        "class_chain": _class_chain(player_controller),
    }
    if not player_controller:
        return result

    flow_widgets = {}
    for key, getter_name in (
        ("main_menu", "get_main_menu_widget_for_test"),
        ("town_overlay", "get_town_overlay_widget_for_test"),
        ("route_map", "get_route_map_widget_for_test"),
        ("battle_board", "get_battle_board_widget_for_test"),
        ("quest_dialog", "get_quest_dialog_widget_for_test"),
        ("route_encounter", "get_route_encounter_panel_widget_for_test"),
        ("relic_bar", "get_relic_bar_widget_for_test"),
        ("task_panel", "get_task_panel_widget_for_test"),
    ):
        try:
            flow_widgets[key] = _widget_summary(getattr(player_controller, getter_name)())
        except Exception as exc:
            flow_widgets[key] = {"error": str(exc)}
    result["flow_widgets"] = flow_widgets
    try:
        view_target = player_controller.get_view_target()
        view_rotation = view_target.get_actor_rotation() if view_target else None
        view_location = view_target.get_actor_location() if view_target else None
        result["view_target"] = {
            "name": view_target.get_name() if view_target else "",
            "label": view_target.get_actor_label() if view_target and hasattr(view_target, "get_actor_label") else "",
            "path": _object_path(view_target),
            "class": _class_path(view_target),
            "class_chain": _class_chain(view_target),
            "tags": [str(tag) for tag in list(view_target.get_editor_property("tags"))] if view_target else [],
            "location": _vector_to_dict(view_location),
            "rotation": _rotator_to_dict(view_rotation),
        }
        try:
            camera_component = view_target.get_camera_component()
        except Exception:
            camera_component = None
        if not camera_component and view_target:
            try:
                camera_component = view_target.get_editor_property("camera_component")
            except Exception:
                camera_component = None
        if camera_component:
            result["view_target"]["camera"] = {
                "projection_mode": _enum_name(camera_component.get_editor_property("projection_mode")),
                "field_of_view": float(camera_component.get_editor_property("field_of_view")),
            }
    except Exception as exc:
        result["view_target"] = {"error": str(exc)}
    try:
        camera_manager = player_controller.player_camera_manager
        result["player_camera"] = {
            "path": _object_path(camera_manager),
            "class": _class_path(camera_manager),
            "location": _vector_to_dict(camera_manager.get_camera_location()),
            "rotation": _rotator_to_dict(camera_manager.get_camera_rotation()),
            "field_of_view": float(camera_manager.get_fov_angle()),
        }
    except Exception as exc:
        result["player_camera"] = {"error": str(exc)}
    return result


def _handle_hud_command(world, command_name):
    player_controller = _first_player_controller(world)
    hud = _first_hud(player_controller)
    if not hud:
        return {"ok": False, "command": command_name, "reason": "hud_missing"}
    try:
        result = bool(hud.handle_demo_command(unreal.Name(command_name)))
        return {"ok": result, "command": command_name}
    except Exception as exc:
        return {"ok": False, "command": command_name, "reason": str(exc)}


def _handle_town_command(world, command_name):
    player_controller = _first_player_controller(world)
    if not player_controller:
        return {"ok": False, "command": command_name, "reason": "player_controller_missing"}
    try:
        town_overlay = player_controller.get_town_overlay_widget_for_test()
    except Exception as exc:
        return {"ok": False, "command": command_name, "reason": str(exc)}
    if not town_overlay:
        return {"ok": False, "command": command_name, "reason": "town_overlay_missing"}
    try:
        if command_name == "EnterDungeon":
            result = bool(town_overlay.enter_route_map())
        elif command_name == "SaveSlot1":
            result = bool(town_overlay.save_to_slot_one())
        else:
            result = bool(town_overlay.execute_town_command_for_test(unreal.Name(command_name)))
        return {"ok": result, "command": command_name}
    except Exception as exc:
        return {"ok": False, "command": command_name, "reason": str(exc)}


def _handle_route_node(world, node_index):
    player_controller = _first_player_controller(world)
    if not player_controller:
        return {"ok": False, "node_index": node_index, "reason": "player_controller_missing"}
    try:
        route_map = player_controller.get_route_map_widget_for_test()
    except Exception as exc:
        return {"ok": False, "node_index": node_index, "reason": str(exc)}
    if not route_map:
        return {"ok": False, "node_index": node_index, "reason": "route_map_missing"}
    try:
        return {"ok": bool(route_map.execute_route_node(int(node_index))), "node_index": int(node_index)}
    except Exception as exc:
        return {"ok": False, "node_index": node_index, "reason": str(exc)}


def _fixture_apply_result(value):
    """Normalize UE Python's bool/out-parameter return variants."""
    if value is None:
        return False, ""
    if isinstance(value, str):
        # UE 5.8 returns the sole FString out parameter directly for a bool + one-out call.
        # An empty string is therefore a successful call with no error text.
        return True, value
    if isinstance(value, (tuple, list)):
        ok = bool(value[0]) if value else False
        error = "" if len(value) < 2 or value[1] is None else str(value[1])
        return ok, error
    if isinstance(value, dict):
        return (
            bool(value.get("return_value", value.get("ok", False))),
            str(value.get("out_error", value.get("error", "")) or ""),
        )
    return bool(value), ""


def _handle_apply_battle_hud_fixture(world):
    player_controller = _first_player_controller(world)
    subsystem = _get_mvp_subsystem(world) or _get_mvp_subsystem_from_player_controller(player_controller)
    if not player_controller:
        return {"ok": False, "reason": "player_controller_missing"}
    if not subsystem:
        return {"ok": False, "reason": "mvp_subsystem_missing"}
    try:
        ok, out_error = _fixture_apply_result(subsystem.apply_battle_hud_fixture_for_test())
    except Exception as exc:
        return {"ok": False, "reason": f"fixture_apply_failed:{exc}"}
    if not ok:
        return {"ok": False, "reason": out_error or "fixture_apply_rejected"}
    try:
        player_controller.refresh_player_flow_widgets_for_test()
    except Exception as exc:
        try:
            subsystem.clear_battle_hud_fixture_for_test()
        except Exception:
            pass
        return {"ok": False, "reason": f"fixture_refresh_failed:{exc}"}
    return {
        "ok": True,
        "player_controller": _object_path(player_controller),
        "subsystem": _object_path(subsystem),
        "out_error": out_error,
    }


def _handle_clear_battle_hud_fixture(world):
    player_controller = _first_player_controller(world)
    subsystem = _get_mvp_subsystem(world) or _get_mvp_subsystem_from_player_controller(player_controller)
    if not subsystem:
        return {"ok": False, "reason": "mvp_subsystem_missing"}
    try:
        subsystem.clear_battle_hud_fixture_for_test()
    except Exception as exc:
        return {"ok": False, "reason": f"fixture_clear_failed:{exc}"}
    if player_controller:
        try:
            player_controller.refresh_player_flow_widgets_for_test()
        except Exception as exc:
            return {"ok": False, "reason": f"fixture_clear_refresh_failed:{exc}"}
    return {"ok": True, "subsystem": _object_path(subsystem)}


def _handle_apply_target_outcome_fixture(world, scenario_id):
    player_controller = _first_player_controller(world)
    subsystem = _get_mvp_subsystem(world) or _get_mvp_subsystem_from_player_controller(player_controller)
    if not player_controller:
        return {"ok": False, "reason": "player_controller_missing"}
    if not subsystem:
        return {"ok": False, "reason": "mvp_subsystem_missing"}
    try:
        ok, out_error = _fixture_apply_result(
            subsystem.apply_target_outcome_fixture_for_test(unreal.Name(str(scenario_id)))
        )
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}
    if not ok:
        return {"ok": False, "reason": out_error or "target_outcome_fixture_apply_rejected"}
    try:
        player_controller.refresh_player_flow_widgets_for_test()
    except Exception as exc:
        clear_error = ""
        try:
            _, clear_error = _fixture_apply_result(subsystem.clear_target_outcome_fixture_for_test())
        except Exception:
            pass
        return {"ok": False, "reason": str(exc), "clear_error": clear_error}
    return {
        "ok": True,
        "scenario_id": str(scenario_id),
        "active": bool(subsystem.is_target_outcome_fixture_active_for_test()),
        "out_error": out_error,
    }


def _handle_clear_target_outcome_fixture(world):
    player_controller = _first_player_controller(world)
    subsystem = _get_mvp_subsystem(world) or _get_mvp_subsystem_from_player_controller(player_controller)
    if not subsystem:
        return {"ok": False, "reason": "mvp_subsystem_missing"}
    try:
        ok, out_error = _fixture_apply_result(subsystem.clear_target_outcome_fixture_for_test())
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}
    if not ok:
        return {"ok": False, "reason": out_error or "target_outcome_fixture_clear_rejected"}
    if player_controller:
        try:
            player_controller.refresh_player_flow_widgets_for_test()
        except Exception as exc:
            return {"ok": False, "reason": str(exc)}
    try:
        active = bool(subsystem.is_target_outcome_fixture_active_for_test())
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}
    return {"ok": not active, "active": active, "out_error": out_error}


def _handle_town_key(world, key_name, state):
    pawn = _first_player_pawn(world)
    if not pawn:
        return {"ok": False, "reason": "player_pawn_missing"}
    key = str(key_name).upper()
    state = str(state).lower()
    if key not in {"D", "A", "W", "S"}:
        return {"ok": False, "key": key, "state": state, "reason": "unsupported_town_key"}
    if state not in {"down", "up"}:
        return {"ok": False, "key": key, "state": state, "reason": "unsupported_town_key_state"}
    try:
        pressed = state == "down"
        result = bool(pawn.set_town_automation_key_state(unreal.Name(key), pressed))
        return {
            "ok": result,
            "key": key,
            "state": state,
            "pressed": pressed,
            "pawn": _object_path(pawn),
        }
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}


def _handle_town_interact(world):
    pawn = _first_player_pawn(world)
    if not pawn:
        return {"ok": False, "reason": "player_pawn_missing"}
    try:
        pawn.interact()
        return {"ok": True, "pawn": _object_path(pawn)}
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}


def _handle_open_quest_offer(world):
    player_controller = _first_player_controller(world)
    pawn = _first_player_pawn(world)
    if not player_controller or not pawn:
        return {"ok": False, "reason": "player_controller_or_pawn_missing"}
    for actor in _all_actors(world):
        try:
            if not _enum_name(actor.get_npc_role()).upper().endswith("QUEST"):
                continue
            return {
                "ok": bool(player_controller.open_task_offer_panel_for_npc(actor, pawn)),
                "npc": _object_path(actor),
            }
        except Exception:
            continue
    return {"ok": False, "reason": "quest_npc_missing"}


def _handle_accept_task_offer(world, task_id):
    player_controller = _first_player_controller(world)
    if not player_controller:
        return {"ok": False, "task_id": task_id, "reason": "player_controller_missing"}
    if not task_id:
        return {"ok": False, "task_id": task_id, "reason": "task_id_missing"}
    try:
        return {
            "ok": bool(player_controller.accept_task_offer_by_id(unreal.Name(task_id))),
            "task_id": task_id,
        }
    except Exception as exc:
        return {"ok": False, "task_id": task_id, "reason": str(exc)}


def _all_actors(world):
    if not world:
        return []
    try:
        return unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    except Exception:
        return []


def _strict_vector2d_to_dict(value):
    if value is None:
        return None
    try:
        x = _finite_float(value.x)
        y = _finite_float(value.y)
        return {"x": x, "y": y} if x is not None and y is not None else None
    except Exception:
        return None


def _viewport_pixel_position(value):
    if isinstance(value, (list, tuple)):
        value = value[0] if value else None
    return _strict_vector2d_to_dict(value)


def _short_exception(exc):
    message = str(exc).replace("\r", " ").replace("\n", " ").strip()
    return f"{type(exc).__name__}: {message[:160]}" if message else type(exc).__name__


def _append_structured_diagnostic(diagnostics, stage, exc=None, code=""):
    if diagnostics is None:
        return
    result = {"stage": stage}
    if exc is not None:
        result["exception"] = _short_exception(exc)
    elif code:
        result["code"] = code
    diagnostics.append(result)


def _append_geometry_error(result, stage, exc):
    result["errors"].append({"stage": stage, "exception": _short_exception(exc)})


def _slate_library():
    # UE 5.8 honors USlateBlueprintLibrary's ScriptName="SlateLibrary" in the
    # Python API.  Older installations expose the generated class name instead.
    return getattr(unreal, "SlateLibrary", getattr(unreal, "SlateBlueprintLibrary", None))


def _read_geometry_vector(geometry, method_name, result_key, result):
    try:
        geometry_method = getattr(geometry, method_name, None)
        if callable(geometry_method):
            value = geometry_method()
        else:
            # UE 5.8's Python FGeometry wrapper does not expose these C++ member
            # helpers, but the reflected Slate library exposes the same geometry
            # transforms and reads the actual cached Slate layout.
            slate_library = _slate_library()
            if method_name == "get_absolute_position":
                value = slate_library.local_to_absolute(geometry, unreal.Vector2D(0.0, 0.0))
            else:
                value = getattr(slate_library, method_name)(geometry)
    except Exception as exc:
        _append_geometry_error(result, f"geometry.{method_name}", exc)
        return None
    vector = _strict_vector2d_to_dict(value)
    result[result_key] = vector
    if value is not None and vector is None:
        result["errors"].append({"stage": f"geometry.{method_name}", "exception": "invalid_vector"})
    return vector


def _cached_geometry_diagnostics(widget):
    result = {
        "cached": False,
        "local_size": None,
        "absolute_position": None,
        "absolute_size": None,
        "errors": [],
    }
    if not widget:
        result["errors"].append({"stage": "widget.get_cached_geometry", "exception": "widget_missing"})
        return None, result
    try:
        geometry = widget.get_cached_geometry()
    except Exception as exc:
        _append_geometry_error(result, "widget.get_cached_geometry", exc)
        return None, result
    result["cached"] = geometry is not None
    if geometry is None:
        result["errors"].append({"stage": "widget.get_cached_geometry", "exception": "geometry_missing"})
        return None, result
    _read_geometry_vector(geometry, "get_local_size", "local_size", result)
    _read_geometry_vector(geometry, "get_absolute_position", "absolute_position", result)
    _read_geometry_vector(geometry, "get_absolute_size", "absolute_size", result)
    return geometry, result


def _screen_rect_with_diagnostics(world, widget):
    """Return the widget's Slate pixel rect together with its geometry diagnostics."""
    geometry, diagnostics = _cached_geometry_diagnostics(widget)
    if not world:
        diagnostics["errors"].append({"stage": "screen_rect", "exception": "world_missing"})
        return None, diagnostics
    if not geometry:
        return None, diagnostics
    local_size = diagnostics["local_size"]
    if not local_size:
        if not any(error["stage"] == "geometry.get_local_size" for error in diagnostics["errors"]):
            diagnostics["errors"].append({"stage": "screen_rect.local_size", "exception": "local_size_unavailable"})
        return None, diagnostics
    if local_size["x"] <= 0.0 or local_size["y"] <= 0.0:
        diagnostics["errors"].append({"stage": "screen_rect.local_size", "exception": "non_positive_local_size"})
        return None, diagnostics
    try:
        slate_library = _slate_library()
        if hasattr(unreal, "SlateLibrary"):
            top_left_raw = slate_library.local_to_viewport(
                world, geometry, unreal.Vector2D(0.0, 0.0)
            )
        else:
            top_left_raw = unreal.SlateBlueprintLibrary.local_to_viewport(
                world, geometry, unreal.Vector2D(0.0, 0.0)
            )
        top_left = _viewport_pixel_position(top_left_raw)
    except Exception as exc:
        _append_geometry_error(diagnostics, "SlateBlueprintLibrary.local_to_viewport.top_left", exc)
        return None, diagnostics
    try:
        if hasattr(unreal, "SlateLibrary"):
            bottom_right_raw = slate_library.local_to_viewport(
                world, geometry, unreal.Vector2D(local_size["x"], local_size["y"])
            )
        else:
            bottom_right_raw = unreal.SlateBlueprintLibrary.local_to_viewport(
                world, geometry, unreal.Vector2D(local_size["x"], local_size["y"])
            )
        bottom_right = _viewport_pixel_position(bottom_right_raw)
    except Exception as exc:
        _append_geometry_error(diagnostics, "SlateBlueprintLibrary.local_to_viewport.bottom_right", exc)
        return None, diagnostics
    if not top_left or not bottom_right:
        diagnostics["errors"].append({"stage": "screen_rect.viewport_position", "exception": "invalid_vector"})
        return None, diagnostics
    return (
        {
            "left": min(top_left["x"], bottom_right["x"]),
            "top": min(top_left["y"], bottom_right["y"]),
            "right": max(top_left["x"], bottom_right["x"]),
            "bottom": max(top_left["y"], bottom_right["y"]),
        },
        diagnostics,
    )


def _screen_rect(world, widget):
    """Compatibility wrapper for existing consumers that only need the Slate pixel rect."""
    return _screen_rect_with_diagnostics(world, widget)[0]


def _append_legacy_error_with_diagnostic(result, legacy_error, stage, exc=None, code=""):
    result["errors"].append(legacy_error)
    _append_structured_diagnostic(result.get("diagnostics"), stage, exc=exc, code=code)


def _component_value(result, key, owner, attribute_name, invoke=True, diagnostic_stage_prefix="widget"):
    try:
        value = getattr(owner, attribute_name)
        return value() if invoke and callable(value) else value
    except Exception as exc:
        _append_legacy_error_with_diagnostic(
            result,
            f"{key}_unavailable:{exc}",
            f"{diagnostic_stage_prefix}.{attribute_name}",
            exc=exc,
        )
        return None


def _rendered_widget_value(result, widget, key, getter_name, *args, diagnostic_stage_prefix="widget"):
    try:
        return getattr(widget, getter_name)(*args)
    except Exception as exc:
        _append_legacy_error_with_diagnostic(
            result,
            f"rendered_{key}_unavailable:{type(exc).__name__}",
            f"{diagnostic_stage_prefix}.{getter_name}",
            exc=exc,
        )
        return None


def _rendered_resource_summary(widget, result, diagnostic_stage_prefix="widget"):
    rendered = {
        "health_text": None,
        "mana_text": None,
        "health_percent": None,
        "mana_percent": None,
    }
    for key, getter_name in (
        ("health_text", "get_health_display_text_for_test"),
        ("mana_text", "get_mana_display_text_for_test"),
    ):
        value = _rendered_widget_value(
            result,
            widget,
            key,
            getter_name,
            diagnostic_stage_prefix=diagnostic_stage_prefix,
        )
        if value is not None:
            rendered[key] = _text_to_string(value)
    for key, getter_name in (
        ("health_percent", "get_health_percent_for_test"),
        ("mana_percent", "get_mana_percent_for_test"),
    ):
        value = _rendered_widget_value(
            result,
            widget,
            key,
            getter_name,
            diagnostic_stage_prefix=diagnostic_stage_prefix,
        )
        if value is None:
            continue
        numeric_value = _finite_float(value)
        if numeric_value is None:
            _append_legacy_error_with_diagnostic(
                result,
                f"rendered_{key}_invalid",
                f"{diagnostic_stage_prefix}.{getter_name}",
                code="invalid_value",
            )
        else:
            rendered[key] = numeric_value
    return rendered


def _rendered_status_summary(widget, result, diagnostic_stage_prefix="widget"):
    rendered = {"icon_count": None, "badges": []}
    count = _rendered_widget_value(
        result,
        widget,
        "icon_count",
        "get_icon_count_for_test",
        diagnostic_stage_prefix=diagnostic_stage_prefix,
    )
    try:
        numeric_count = _finite_float(count)
        count = int(numeric_count) if numeric_count is not None else None
    except (TypeError, ValueError, OverflowError):
        _append_legacy_error_with_diagnostic(
            result,
            "rendered_icon_count_invalid",
            f"{diagnostic_stage_prefix}.get_icon_count_for_test",
            code="invalid_icon_count",
        )
        return rendered
    if count is None:
        _append_legacy_error_with_diagnostic(
            result,
            "rendered_icon_count_invalid",
            f"{diagnostic_stage_prefix}.get_icon_count_for_test",
            code="invalid_icon_count",
        )
        return rendered
    if count < 0 or count > 16:
        _append_legacy_error_with_diagnostic(
            result,
            "rendered_icon_count_out_of_range",
            f"{diagnostic_stage_prefix}.get_icon_count_for_test",
            code="icon_count_out_of_range",
        )
        return rendered
    rendered["icon_count"] = count
    for index in range(count):
        icon_id = _rendered_widget_value(
            result,
            widget,
            f"icon_{index}_id",
            "get_icon_id_for_test",
            index,
            diagnostic_stage_prefix=diagnostic_stage_prefix,
        )
        displayed_stack = _rendered_widget_value(
            result,
            widget,
            f"icon_{index}_displayed_stack",
            "get_icon_displayed_stack_for_test",
            index,
            diagnostic_stage_prefix=diagnostic_stage_prefix,
        )
        if icon_id is None or displayed_stack is None:
            continue
        rendered["badges"].append(
            {
                "icon_id": _text_to_string(icon_id),
                "displayed_stack": _text_to_string(displayed_stack),
            }
        )
    return rendered


def _widget_screen_summary(world, widget, diagnostic_stage_prefix="widget"):
    result = {
        "path": _object_path(widget),
        "class": _class_path(widget),
        "visible": None,
        "visibility": None,
        "screen_rect": None,
        "geometry": None,
        "errors": [],
        "diagnostics": [],
    }
    if not widget:
        _, result["geometry"] = _cached_geometry_diagnostics(widget)
        _append_legacy_error_with_diagnostic(result, "widget_missing", diagnostic_stage_prefix, code="widget_missing")
        return result
    visible = _component_value(
        result,
        "visible",
        widget,
        "is_visible",
        diagnostic_stage_prefix=diagnostic_stage_prefix,
    )
    if visible is not None:
        result["visible"] = bool(visible)
    visibility = _component_value(
        result,
        "visibility",
        widget,
        "get_visibility",
        diagnostic_stage_prefix=diagnostic_stage_prefix,
    )
    if visibility is not None:
        result["visibility"] = _enum_name(visibility)
    enabled = _component_value(
        result,
        "is_enabled",
        widget,
        "get_is_enabled",
        diagnostic_stage_prefix=diagnostic_stage_prefix,
    )
    if enabled is not None:
        result["is_enabled"] = bool(enabled)
    render_opacity = _component_value(
        result,
        "render_opacity",
        widget,
        "get_render_opacity",
        diagnostic_stage_prefix=diagnostic_stage_prefix,
    )
    if render_opacity is not None:
        result["render_opacity"] = _finite_float(render_opacity)
    result["screen_rect"], result["geometry"] = _screen_rect_with_diagnostics(world, widget)
    if result["screen_rect"] is None:
        _append_legacy_error_with_diagnostic(
            result,
            "screen_rect_unavailable",
            f"{diagnostic_stage_prefix}.screen_rect",
            code="screen_rect_unavailable",
        )
    return result


def _handle_apply_route_exit_acceptance_fixture(world):
    subsystem = _get_mvp_subsystem(world) or _get_mvp_subsystem_from_player_controller(
        _first_player_controller(world)
    )
    if not subsystem:
        return {"ok": False, "reason": "subsystem_missing"}
    try:
        ok, out_error = _fixture_apply_result(
            subsystem.apply_route_exit_acceptance_fixture_for_test()
        )
    except Exception as exc:
        return {"ok": False, "reason": f"route_exit_fixture_exception:{exc}"}
    return {
        "ok": bool(ok),
        "error": str(out_error or ""),
        "active": bool(subsystem.is_route_exit_acceptance_fixture_active_for_test()),
    }


def _handle_clear_route_exit_acceptance_fixture(world):
    subsystem = _get_mvp_subsystem(world) or _get_mvp_subsystem_from_player_controller(
        _first_player_controller(world)
    )
    if not subsystem:
        return {"ok": False, "reason": "subsystem_missing"}
    try:
        ok, out_error = _fixture_apply_result(
            subsystem.clear_route_exit_acceptance_fixture_for_test()
        )
    except Exception as exc:
        return {"ok": False, "reason": f"route_exit_fixture_clear_exception:{exc}"}
    return {
        "ok": bool(ok),
        "error": str(out_error or ""),
        "active": bool(subsystem.is_route_exit_acceptance_fixture_active_for_test()),
    }


def _handle_high_res_screenshot(world, name, width, height):
    if not world:
        return {"ok": False, "reason": "pie_world_missing"}
    try:
        width = int(width)
        height = int(height)
    except (TypeError, ValueError):
        return {"ok": False, "reason": "invalid_dimensions"}
    if width <= 0 or height <= 0:
        return {"ok": False, "reason": "invalid_dimensions"}
    safe_name = Path(str(name)).name
    if not safe_name.lower().endswith(".png"):
        safe_name += ".png"
    output_path = PROJECT_ROOT / "Saved" / "VisualReview" / "20260819-battle-retreat-route-abandon" / safe_name
    output_path.parent.mkdir(parents=True, exist_ok=True)
    if output_path.exists():
        output_path.unlink()
    try:
        active = getattr(builtins, _HIGH_RES_SCREENSHOT_STATE_ATTR, None)
        if isinstance(active, dict):
            active_task = active.get("task")
            if (
                active_task is not None
                and bool(active_task.is_valid_task())
                and not bool(active_task.is_task_done())
            ):
                return {
                    "ok": False,
                    "reason": "screenshot_task_busy",
                    "path": str(active.get("path", "")),
                }
            delattr(builtins, _HIGH_RES_SCREENSHOT_STATE_ATTR)
        task = unreal.AutomationLibrary.take_high_res_screenshot(
            width,
            height,
            str(output_path),
            mask_enabled=False,
            capture_hdr=False,
            comparison_tolerance=unreal.ComparisonTolerance.LOW,
            comparison_notes="GameXXK two-level route exit visual acceptance",
            delay=0.0,
            force_game_view=True,
        )
        if task is None or not bool(task.is_valid_task()):
            return {"ok": False, "reason": "screenshot_task_invalid", "path": str(output_path)}
        setattr(
            builtins,
            _HIGH_RES_SCREENSHOT_STATE_ATTR,
            {
                "task": task,
                "path": str(output_path),
                "width": width,
                "height": height,
            },
        )
        return {
            "ok": True,
            "path": str(output_path),
            "width": width,
            "height": height,
            "state": "started",
            "done": False,
        }
    except Exception as exc:
        return {"ok": False, "reason": f"screenshot_exception:{exc}", "path": str(output_path)}


def _handle_poll_high_res_screenshot():
    state = getattr(builtins, _HIGH_RES_SCREENSHOT_STATE_ATTR, None)
    if not isinstance(state, dict):
        return {"ok": False, "reason": "screenshot_task_missing"}
    path = Path(str(state.get("path", "")))
    result = {
        "path": str(path),
        "width": int(state.get("width") or 0),
        "height": int(state.get("height") or 0),
    }
    try:
        task = state.get("task")
        if task is None or not bool(task.is_valid_task()):
            delattr(builtins, _HIGH_RES_SCREENSHOT_STATE_ATTR)
            return {**result, "ok": False, "reason": "screenshot_task_invalid"}
        if not bool(task.is_task_done()):
            return {**result, "ok": True, "state": "pending", "done": False}
        if not path.is_file() or path.stat().st_size <= 0:
            return {**result, "ok": True, "state": "file_pending", "done": False}
        delattr(builtins, _HIGH_RES_SCREENSHOT_STATE_ATTR)
        return {
            **result,
            "ok": True,
            "state": "complete",
            "done": True,
            "size_bytes": path.stat().st_size,
        }
    except Exception as exc:
        if hasattr(builtins, _HIGH_RES_SCREENSHOT_STATE_ATTR):
            delattr(builtins, _HIGH_RES_SCREENSHOT_STATE_ATTR)
        return {**result, "ok": False, "reason": f"screenshot_poll_exception:{exc}"}


def _battle_scene_unit_ids(world):
    """Legacy scene-actor lookup retained only for compatibility diagnostics."""
    unit_ids = []
    for actor in _all_actors(world):
        try:
            if "BattleSceneUnitActor" not in actor.get_class().get_name():
                continue
            unit_id = str(actor.get_unit_id())
        except Exception:
            continue
        if unit_id and unit_id not in unit_ids:
            unit_ids.append(unit_id)
    return unit_ids


def _active_battle_unit_ids(subsystem):
    """Read stable Board-HUD keys from the authoritative actor-free battle state."""
    if not subsystem:
        return []
    try:
        state = subsystem.get_runtime_state_copy()
    except Exception:
        return []
    card_run = _struct_get(state, "card_run", "CardRun")
    active_battle = _struct_get(card_run, "active_battle", "ActiveBattle")
    units = _struct_get(active_battle, "units", "Units")
    if units is None:
        return []
    unit_ids = []
    try:
        entries = list(units)
    except Exception:
        return []
    for unit in entries:
        unit_id = str(_struct_get(unit, "unit_id", "UnitId") or "")
        living = _struct_get(unit, "b_living", "living", "bLiving", "Living")
        if not unit_id or living is False or unit_id in unit_ids:
            continue
        unit_ids.append(unit_id)
    return unit_ids


def _battle_side_name(value):
    side_name = _enum_name(value)
    normalized = str(side_name).strip().casefold()
    if normalized.endswith("party"):
        return "Party"
    if normalized.endswith("enemy"):
        return "Enemy"
    return str(side_name)


def _canvas_slot_summary(widget):
    result = {"position": None, "size": None, "offsets": None, "z_order": None, "visibility": None, "errors": []}
    if not widget:
        result["errors"].append({"stage": "widget.slot", "exception": "widget_missing"})
        return result
    try:
        result["visibility"] = _enum_name(widget.get_visibility())
    except Exception as exc:
        result["errors"].append({"stage": "widget.get_visibility", "exception": _short_exception(exc)})
    try:
        slot = widget.slot
    except Exception:
        try:
            slot = widget.get_editor_property("slot")
        except Exception as exc:
            result["errors"].append({"stage": "widget.slot", "exception": _short_exception(exc)})
            return result
    if not slot:
        result["errors"].append({"stage": "widget.slot", "exception": "slot_missing"})
        return result
    for key, getter_name in (("position", "get_position"), ("size", "get_size")):
        try:
            value = getattr(slot, getter_name)()
        except Exception as exc:
            result["errors"].append({"stage": f"canvas_slot.{getter_name}", "exception": _short_exception(exc)})
            continue
        result[key] = _strict_vector2d_to_dict(value)
        if value is not None and result[key] is None:
            result["errors"].append({"stage": f"canvas_slot.{getter_name}", "exception": "invalid_vector"})
    try:
        offsets = slot.get_offsets()
        offset_values = {name: _finite_float(getattr(offsets, name, None)) for name in ("left", "top", "right", "bottom")}
        result["offsets"] = offset_values if all(value is not None for value in offset_values.values()) else None
        if result["offsets"] is None:
            result["errors"].append({"stage": "canvas_slot.get_offsets", "exception": "invalid_margin"})
    except Exception as exc:
        result["errors"].append({"stage": "canvas_slot.get_offsets", "exception": _short_exception(exc)})
    try:
        result["z_order"] = int(slot.get_z_order())
    except Exception as exc:
        result["errors"].append({"stage": "canvas_slot.get_z_order", "exception": _short_exception(exc)})
    return result


def _projection_anchor_summary(board, unit_name, present_getter_name, anchor_getter_name, error_prefix):
    result = {"present": None, "value": None, "source": anchor_getter_name, "diagnostics": []}
    try:
        result["present"] = bool(getattr(board, present_getter_name)(unit_name))
    except Exception as exc:
        result["error"] = f"{error_prefix}_presence_unavailable:{_short_exception(exc)}"
        _append_structured_diagnostic(result["diagnostics"], f"board.{present_getter_name}", exc=exc)
    try:
        result["value"] = _strict_vector2d_to_dict(getattr(board, anchor_getter_name)(unit_name))
    except Exception as exc:
        result["error"] = f"{error_prefix}_value_unavailable:{_short_exception(exc)}"
        _append_structured_diagnostic(result["diagnostics"], f"board.{anchor_getter_name}", exc=exc)
    return result


def _latest_applied_anchor(slot_summary):
    position = slot_summary.get("position")
    size = slot_summary.get("size")
    result = {
        "present": False,
        "value": None,
        "source": "canvas_slot.position_and_size",
        "kind": "slot_top_center",
    }
    if not position or not size:
        return result
    result["present"] = True
    result["value"] = {
        "x": position["x"] + size["x"] * 0.5,
        "y": position["y"],
    }
    return result


def _viewport_dpi_revision(world, player_controller):
    viewport_diagnostics = []
    result = {
        "viewport": _pie_viewport_summary(player_controller, viewport_diagnostics),
        "dpi_scale": None,
        "revision": None,
        "errors": viewport_diagnostics,
    }
    if not world:
        result["errors"].append({"stage": "SlateBlueprintLibrary.get_viewport_scale", "exception": "world_missing"})
    else:
        try:
            dpi_scale = _finite_float(unreal.SlateBlueprintLibrary.get_viewport_scale(world))
        except Exception as exc:
            result["errors"].append(
                {"stage": "SlateBlueprintLibrary.get_viewport_scale", "exception": _short_exception(exc)}
            )
        else:
            if dpi_scale is None or dpi_scale <= 0.0:
                result["errors"].append(
                    {"stage": "SlateBlueprintLibrary.get_viewport_scale", "exception": "invalid_scale"}
                )
            else:
                result["dpi_scale"] = dpi_scale
    # This capture key lets reports be compared across a viewport or DPI change without
    # claiming that the Board owns a mutable production revision counter.
    result["revision"] = {"viewport": result["viewport"], "dpi_scale": result["dpi_scale"]}
    return result


def _projection_geometry_summary(widget_summary):
    geometry = widget_summary.get("geometry") or {}
    return {
        "cached": geometry.get("cached"),
        "local_size": geometry.get("local_size"),
        "absolute_position": geometry.get("absolute_position"),
        "absolute_size": geometry.get("absolute_size"),
        "screen_rect": widget_summary.get("screen_rect"),
        "errors": geometry.get("errors", []),
    }


def _projection_canvas_summary(layer_summary):
    result = _projection_geometry_summary(layer_summary)
    result["size"] = result["local_size"]
    return result


def _board_unit_hud_summary(world, board, unit_id):
    result = {
        "unit_id": unit_id,
        "side": "",
        "slot": None,
        "projected_anchor": None,
        "screen_rect": None,
        "visible": None,
        "resource": {"visible": None, "screen_rect": None, "rendered": {}, "mana_row_visible": None},
        "status": {"visible": None, "screen_rect": None, "rendered": {}, "badges": []},
        "projection": {
            "received_anchor": {"present": None, "value": None},
            "transient_anchor": {"present": None, "value": None},
            "latest_anchor": {"present": False, "value": None, "source": "canvas_slot.position_and_size"},
            "latest_applied_anchor": {"present": False, "value": None, "source": "canvas_slot.position_and_size"},
            "applied_slot": {"position": None, "size": None, "offsets": None, "z_order": None, "visibility": None, "errors": []},
            "visible": None,
            "visibility": None,
        },
        "errors": [],
        "diagnostics": [],
    }
    try:
        widget = board.get_projected_unit_hud_for_test(unreal.Name(unit_id))
    except Exception as exc:
        _append_legacy_error_with_diagnostic(
            result,
            f"unit_hud_unavailable:{exc}",
            "battle_board.get_projected_unit_hud_for_test",
            exc=exc,
        )
        return result
    widget_summary = _widget_screen_summary(world, widget, "battle_unit_hud")
    result["screen_rect"] = widget_summary["screen_rect"]
    result["visible"] = widget_summary["visible"]
    result["projection"]["visible"] = widget_summary["visible"]
    result["projection"]["visibility"] = widget_summary["visibility"]
    result["errors"].extend(widget_summary["errors"])
    result["diagnostics"].extend(widget_summary["diagnostics"])
    if not widget:
        return result
    metadata_stage = "battle_unit_hud.get_unit_id_for_test"
    try:
        result["unit_id"] = str(widget.get_unit_id_for_test())
        metadata_stage = "battle_unit_hud.get_side_for_test"
        result["side"] = _battle_side_name(widget.get_side_for_test())
        metadata_stage = "battle_unit_hud.get_slot_number_for_test"
        result["slot"] = int(widget.get_slot_number_for_test())
    except Exception as exc:
        _append_legacy_error_with_diagnostic(
            result,
            f"unit_hud_metadata_unavailable:{exc}",
            metadata_stage,
            exc=exc,
        )
    try:
        unit_name = unreal.Name(unit_id)
        result["projection"]["received_anchor"] = _projection_anchor_summary(
            board,
            unit_name,
            "has_battle_unit_screen_position_for_test",
            "get_battle_unit_screen_position_for_test",
            "received_anchor",
        )
        result["projection"]["transient_anchor"] = _projection_anchor_summary(
            board,
            unit_name,
            "has_projected_unit_hud_screen_position_for_test",
            "get_projected_unit_hud_anchor_position_for_test",
            "transient_anchor",
        )
        result["projected_anchor"] = result["projection"]["transient_anchor"]["value"]
    except Exception as exc:
        _append_legacy_error_with_diagnostic(
            result,
            f"unit_hud_projection_unavailable:{_short_exception(exc)}",
            "battle_unit_hud.projection",
            exc=exc,
        )
    result["projection"]["applied_slot"] = _canvas_slot_summary(widget)
    result["projection"]["latest_applied_anchor"] = _latest_applied_anchor(result["projection"]["applied_slot"])
    result["projection"]["latest_anchor"] = result["projection"]["latest_applied_anchor"]

    for key, getter_name, rendered_summary in (
        ("resource", "get_resource_widget_for_test", _rendered_resource_summary),
        ("status", "get_status_effects_widget_for_test", _rendered_status_summary),
    ):
        try:
            child = getattr(widget, getter_name)()
        except Exception as exc:
            child = None
            _append_legacy_error_with_diagnostic(
                result,
                f"{key}_unavailable:{exc}",
                f"battle_unit_hud.{getter_name}",
                exc=exc,
            )
        child_summary = _widget_screen_summary(world, child, f"battle_unit_hud.{key}")
        result[key]["visible"] = child_summary["visible"]
        result[key]["screen_rect"] = child_summary["screen_rect"]
        result[key]["rendered"] = (
            rendered_summary(child, child_summary, diagnostic_stage_prefix=f"battle_unit_hud.{key}") if child else {}
        )
        if key == "resource" and child:
            try:
                result[key]["mana_row_visible"] = bool(child.is_mana_row_visible_for_test())
            except Exception as exc:
                _append_legacy_error_with_diagnostic(
                    result,
                    f"mana_row_visible_unavailable:{exc}",
                    "battle_unit_hud.resource.is_mana_row_visible_for_test",
                    exc=exc,
                )
        result["errors"].extend(child_summary["errors"])
        result["diagnostics"].extend(child_summary["diagnostics"])
    return result


def _battle_board_shared_energy(subsystem, result):
    if not subsystem:
        result["errors"].append("subsystem_missing")
        _append_structured_diagnostic(result.get("diagnostics"), "battle_board.subsystem", code="subsystem_missing")
        return None
    try:
        state = subsystem.get_runtime_state_copy()
    except Exception as exc:
        result["errors"].append(f"runtime_state_unavailable:{exc}")
        _append_structured_diagnostic(result.get("diagnostics"), "battle_board.get_runtime_state_copy", exc=exc)
        return None
    deck = _struct_get(
        _struct_get(
            _struct_get(state, "card_run", "CardRun"),
            "active_battle",
            "ActiveBattle",
        ),
        "deck",
        "Deck",
    )
    shared_energy = _struct_get(deck, "shared_energy", "SharedEnergy")
    shared_energy_number = _finite_float(shared_energy)
    if shared_energy_number is None:
        result["errors"].append("shared_energy_invalid")
        _append_structured_diagnostic(result.get("diagnostics"), "battle_board.shared_energy", code="invalid_shared_energy")
        return None
    return int(shared_energy_number)


def _battle_board_summary(world, player_controller, subsystem):
    result = {
        "path": "",
        "class": "",
        "visible": None,
        "visibility": None,
        "shared_energy": None,
        "party_qi": None,
        "hand_card_box": None,
        "end_turn_button": None,
        "unit_hud_layer": None,
        "unit_ids": [],
        "unit_huds": {},
        "target_proxies": {},
        "hand_buttons": [],
        "outcome_preview": {
            "visible": False,
            "class": "",
            "card_instance_id": "",
            "target_unit_id": "",
            "lines": [],
            "build_count": None,
            "single_anchor": None,
            "group_anchor": None,
            "targeting_pointer": None,
            "single_offsets": None,
            "single_alignment": None,
            "background_resource": "",
        },
        "projection": {
            "canvas": {"cached": None, "local_size": None, "size": None, "absolute_position": None, "absolute_size": None, "screen_rect": None, "errors": []},
            "root_geometry": {"cached": None, "local_size": None, "absolute_position": None, "absolute_size": None, "screen_rect": None, "errors": []},
            "layer_geometry": {"cached": None, "local_size": None, "absolute_position": None, "absolute_size": None, "screen_rect": None, "errors": []},
            "layer_z_order": None,
            "viewport_dpi_revision": _viewport_dpi_revision(world, player_controller),
        },
        "errors": [],
        "diagnostics": [],
    }
    if not player_controller:
        result["errors"].append("player_controller_missing")
        _append_structured_diagnostic(result["diagnostics"], "battle_board.player_controller", code="player_controller_missing")
        return result
    try:
        board = player_controller.get_battle_board_widget_for_test()
    except Exception as exc:
        result["errors"].append(f"battle_board_unavailable:{exc}")
        _append_structured_diagnostic(result["diagnostics"], "battle_board.get_battle_board_widget_for_test", exc=exc)
        return result
    if not board:
        result["errors"].append("battle_board_missing")
        _append_structured_diagnostic(result["diagnostics"], "battle_board.get_battle_board_widget_for_test", code="battle_board_missing")
        return result

    result["path"] = _object_path(board)
    result["class"] = _class_path(board)
    result["projection"]["root_geometry"] = _projection_geometry_summary(
        _widget_screen_summary(world, board, "battle_board")
    )
    board_visible = _component_value(
        result,
        "visible",
        board,
        "is_visible",
        diagnostic_stage_prefix="battle_board",
    )
    if board_visible is not None:
        result["visible"] = bool(board_visible)
    board_visibility = _component_value(
        result,
        "visibility",
        board,
        "get_visibility",
        diagnostic_stage_prefix="battle_board",
    )
    if board_visibility is not None:
        result["visibility"] = _enum_name(board_visibility)
    result["shared_energy"] = _battle_board_shared_energy(subsystem, result)

    try:
        party_qi_widget = board.get_party_qi_widget_for_test()
    except Exception as exc:
        party_qi_widget = None
        result["errors"].append(f"party_qi_unavailable:{exc}")
        _append_structured_diagnostic(result["diagnostics"], "battle_board.get_party_qi_widget_for_test", exc=exc)
    result["party_qi"] = _widget_screen_summary(world, party_qi_widget, "battle_board.party_qi")
    if party_qi_widget:
        try:
            result["party_qi"]["value"] = int(party_qi_widget.get_shared_qi_for_test())
        except Exception as exc:
            result["party_qi"]["value"] = None
            _append_legacy_error_with_diagnostic(
                result["party_qi"],
                f"shared_qi_unavailable:{exc}",
                "battle_board.party_qi.get_shared_qi_for_test",
                exc=exc,
            )
    else:
        result["party_qi"]["value"] = None

    for result_key, getter_name in (
        ("hand_card_box", "get_hand_card_box_for_test"),
        ("end_turn_button", "get_end_turn_button_for_test"),
    ):
        try:
            result[result_key] = _widget_screen_summary(world, getattr(board, getter_name)(), f"battle_board.{result_key}")
        except Exception as exc:
            result[result_key] = _widget_screen_summary(world, None, f"battle_board.{result_key}")
            _append_legacy_error_with_diagnostic(
                result[result_key],
                f"widget_unavailable:{exc}",
                f"battle_board.{getter_name}",
                exc=exc,
            )
            _append_structured_diagnostic(result["diagnostics"], f"battle_board.{getter_name}", exc=exc)
    layer_widget = None
    try:
        layer_widget = board.get_battle_projected_unit_hud_layer_for_test()
        result["unit_hud_layer"] = _widget_screen_summary(world, layer_widget, "battle_board.unit_hud_layer")
    except Exception as exc:
        result["unit_hud_layer"] = _widget_screen_summary(world, None, "battle_board.unit_hud_layer")
        _append_legacy_error_with_diagnostic(
            result["unit_hud_layer"],
            f"unit_hud_layer_unavailable:{exc}",
            "battle_board.get_battle_projected_unit_hud_layer_for_test",
            exc=exc,
        )
        _append_structured_diagnostic(result["diagnostics"], "battle_board.get_battle_projected_unit_hud_layer_for_test", exc=exc)
    layer_slot = _canvas_slot_summary(layer_widget)
    result["projection"]["layer_geometry"] = _projection_geometry_summary(result["unit_hud_layer"])
    result["projection"]["layer_z_order"] = layer_slot["z_order"]
    result["projection"]["canvas"] = _projection_canvas_summary(result["unit_hud_layer"])
    result["unit_ids"] = _active_battle_unit_ids(subsystem)
    for unit_id in result["unit_ids"]:
        result["unit_huds"][unit_id] = _board_unit_hud_summary(world, board, unit_id)
        try:
            proxy = board.get_unit_target_proxy_for_test(unreal.Name(unit_id))
            result["target_proxies"][unit_id] = _widget_screen_summary(
                world,
                proxy,
                f"battle_board.target_proxy.{unit_id}",
            )
        except Exception as exc:
            result["errors"].append(f"target_proxy_unavailable:{unit_id}:{exc}")
            _append_structured_diagnostic(
                result["diagnostics"],
                "battle_board.get_unit_target_proxy_for_test",
                exc=exc,
            )

    runtime_summary = _runtime_state(subsystem)
    hand_cards = runtime_summary.get("battle_hand", []) if isinstance(runtime_summary, dict) else []
    # The board exposes each button as a reflected read-only seam, while its native
    # visible-count helper intentionally remains C++-only.  The runtime hand is the
    # authoritative identity/order source used by RefreshHandCards, so probe exactly
    # those slots and retain only buttons Slate reports as visible.
    for slot_index in range(min(5, len(hand_cards))):
        try:
            button = board.get_hand_card_button_for_test(slot_index)
            button_summary = _widget_screen_summary(
                world,
                button,
                f"battle_board.hand_button.{slot_index}",
            )
            button_summary["slot_index"] = slot_index
            button_summary["instance_id"] = (
                str(hand_cards[slot_index].get("instance_id", ""))
                if slot_index < len(hand_cards) and isinstance(hand_cards[slot_index], dict)
                else ""
            )
            if button_summary.get("visible"):
                result["hand_buttons"].append(button_summary)
        except Exception as exc:
            result["errors"].append(f"hand_button_unavailable:{slot_index}:{exc}")
            _append_structured_diagnostic(
                result["diagnostics"],
                "battle_board.get_hand_card_button_for_test",
                exc=exc,
            )

    outcome_getters = (
        ("visible", "is_card_outcome_preview_visible_for_test", bool),
        ("class", "get_card_outcome_preview_class_for_test", str),
        ("card_instance_id", "get_card_outcome_preview_card_instance_id_for_test", str),
        ("target_unit_id", "get_card_outcome_preview_target_unit_id_for_test", str),
        ("lines", "get_card_outcome_preview_lines_for_test", list),
        ("build_count", "get_card_outcome_preview_build_count_for_test", int),
        ("single_anchor", "get_single_outcome_preview_anchor_for_test", _strict_vector2d_to_dict),
        ("group_anchor", "get_group_outcome_preview_anchor_for_test", _strict_vector2d_to_dict),
        ("targeting_pointer", "get_targeting_pointer_position_for_test", _strict_vector2d_to_dict),
    )
    for key, getter_name, converter in outcome_getters:
        try:
            value = getattr(board, getter_name)()
            if key == "lines":
                value = [re.sub(r"^([123]P)(?=\S)", r"\1 ", str(line)) for line in list(value)]
            else:
                value = converter(value)
            result["outcome_preview"][key] = value
        except Exception as exc:
            result["errors"].append(f"outcome_preview_{key}_unavailable:{exc}")
            _append_structured_diagnostic(
                result["diagnostics"],
                f"battle_board.{getter_name}",
                exc=exc,
            )

    # The render-tree snapshot is exposed through reflected Board seams because
    # UUserWidget::GetWidgetTree is not exported to Python in UE 5.8.
    render_tree_getters = (
        ("single_offsets", "get_single_outcome_preview_offsets_for_test", lambda value: {
            "left": float(_struct_get(value, "left", "Left")),
            "top": float(_struct_get(value, "top", "Top")),
            "right": float(_struct_get(value, "right", "Right")),
            "bottom": float(_struct_get(value, "bottom", "Bottom")),
        }),
        ("single_alignment", "get_single_outcome_preview_alignment_for_test", _strict_vector2d_to_dict),
        ("background_resource", "get_card_outcome_preview_background_resource_for_test", str),
    )
    for key, getter_name, converter in render_tree_getters:
        try:
            result["outcome_preview"][key] = converter(getattr(board, getter_name)())
        except Exception as exc:
            result["errors"].append(f"outcome_preview_{key}_unavailable:{exc}")
            _append_structured_diagnostic(
                result["diagnostics"],
                f"battle_board.{getter_name}",
                exc=exc,
            )
    return result


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
        has_npc_role = hasattr(actor, "get_npc_role")
        if (
            any(token in class_name for token in ("GameXXK", "PlayerStart", "BP_NpcCharacter", "BP_MerchantCharacter"))
            or any(token in label for token in ("Qingshan", "PlayerStart"))
            or has_npc_role
        ):
            summary = {
                "name": actor.get_name(),
                "label": label,
                "class": class_name,
                "location": _vector_to_dict(actor.get_actor_location()),
            }
            if has_npc_role:
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
                try:
                    summary["body_character"] = _npc_visual_character_summary(actor)
                except Exception:
                    pass
            if class_name == "GameXXKTownNpcActor":
                try:
                    summary["visual_character_class"] = _object_path(actor.get_visual_character_class())
                except Exception:
                    pass
                try:
                    summary["visual_character"] = _npc_visual_character_summary(actor.get_visual_character())
                except Exception:
                    pass
            if hasattr(actor, "is_enemy_unit"):
                try:
                    summary["is_enemy_unit"] = bool(actor.is_enemy_unit())
                except Exception:
                    pass
                try:
                    summary["can_receive_primary_party_attack"] = bool(actor.can_receive_primary_party_attack())
                except Exception:
                    pass
                try:
                    summary["unit_index"] = int(actor.get_unit_index())
                except Exception:
                    pass
                try:
                    summary["unit_id"] = str(actor.get_unit_id())
                except Exception:
                    pass
                try:
                    summary["battle_visual"] = _visual_summary(actor)
                except Exception:
                    pass
                try:
                    summary["current_battle_flipbook"] = _object_path(actor.get_current_battle_flipbook())
                except Exception:
                    pass
            if hasattr(actor, "get_spawned_units_for_test"):
                try:
                    summary["spawned_unit_count"] = len(actor.get_spawned_units_for_test())
                except Exception:
                    pass
            result.append(summary)
    return result


def _first_component(actor, component_type):
    if not actor or component_type is None:
        return None
    try:
        components = actor.get_components_by_class(component_type)
    except Exception:
        components = []
    return components[0] if components else None


def _camera_summary(pawn):
    camera_type = getattr(unreal, "CameraComponent", None)
    camera = _first_component(pawn, camera_type)
    if not camera:
        return {}
    result = {
        "name": camera.get_name(),
        "path": _object_path(camera),
        "class": _class_path(camera),
    }
    for key in ("projection_mode", "ortho_width", "field_of_view", "auto_activate"):
        try:
            value = camera.get_editor_property(key)
            result[key] = _enum_name(value) if key == "projection_mode" else value
        except Exception:
            pass
    try:
        result["relative_location"] = _vector_to_dict(camera.get_editor_property("relative_location"))
    except Exception:
        pass
    try:
        result["relative_rotation"] = _rotator_to_dict(camera.get_editor_property("relative_rotation"))
    except Exception:
        pass
    try:
        result["world_location"] = _vector_to_dict(camera.get_component_location())
        result["world_rotation"] = _rotator_to_dict(camera.get_component_rotation())
    except Exception:
        pass
    try:
        result["attach_parent"] = _object_path(camera.get_attach_parent())
    except Exception:
        pass
    return result


def _spring_arm_summary(pawn):
    spring_arm_type = getattr(unreal, "SpringArmComponent", None)
    boom = _first_component(pawn, spring_arm_type)
    if not boom:
        return {}
    result = {
        "name": boom.get_name(),
        "path": _object_path(boom),
        "class": _class_path(boom),
    }
    for key in ("target_arm_length", "do_collision_test", "use_pawn_control_rotation", "absolute_rotation"):
        try:
            result[key] = boom.get_editor_property(key)
        except Exception:
            pass
    try:
        result["relative_location"] = _vector_to_dict(boom.get_editor_property("relative_location"))
    except Exception:
        pass
    try:
        result["relative_rotation"] = _rotator_to_dict(boom.get_editor_property("relative_rotation"))
    except Exception:
        pass
    try:
        result["world_location"] = _vector_to_dict(boom.get_component_location())
        result["world_rotation"] = _rotator_to_dict(boom.get_component_rotation())
    except Exception:
        pass
    try:
        result["attach_parent"] = _object_path(boom.get_attach_parent())
    except Exception:
        pass
    return result


def _visual_summary(pawn):
    visual = None
    for component_type_name in ("PaperFlipbookComponent",):
        component_type = getattr(unreal, component_type_name, None)
        visual = _first_component(pawn, component_type)
        if visual:
            break
    if not visual:
        return {}
    result = {
        "name": visual.get_name(),
        "path": _object_path(visual),
        "class": _class_path(visual),
    }
    for key in ("visible", "hidden_in_game", "component_tick_enabled"):
        try:
            if key == "visible":
                result[key] = bool(visual.is_visible())
            elif key == "hidden_in_game":
                result[key] = bool(visual.b_hidden_in_game)
            else:
                result[key] = bool(visual.is_component_tick_enabled())
        except Exception:
            pass
    try:
        result["relative_location"] = _vector_to_dict(visual.get_editor_property("relative_location"))
    except Exception:
        pass
    try:
        result["relative_rotation"] = _rotator_to_dict(visual.get_editor_property("relative_rotation"))
    except Exception:
        pass
    try:
        result["world_location"] = _vector_to_dict(visual.get_component_location())
        result["world_rotation"] = _rotator_to_dict(visual.get_component_rotation())
    except Exception:
        pass
    try:
        result["bounds_origin"] = _vector_to_dict(visual.bounds.origin)
        result["bounds_extent"] = _vector_to_dict(visual.bounds.box_extent)
    except Exception:
        pass
    try:
        origin, extent = visual.get_local_bounds()
        result["local_bounds_origin"] = _vector_to_dict(origin)
        result["local_bounds_extent"] = _vector_to_dict(extent)
    except Exception:
        pass
    try:
        result["relative_scale"] = _vector_to_dict(visual.get_editor_property("relative_scale3d"))
    except Exception:
        pass
    try:
        result["flipbook"] = _object_path(visual.get_flipbook())
    except Exception:
        pass
    return result


def _npc_visual_character_summary(character):
    if not character:
        return {}
    result = {
        "name": character.get_name(),
        "path": _object_path(character),
        "class": _class_path(character),
        "class_chain": _class_chain(character),
        "location": _vector_to_dict(character.get_actor_location()),
    }
    try:
        result["actor_tick_enabled"] = bool(character.is_actor_tick_enabled())
    except Exception:
        pass
    for method_name in (
        "has_town_visual",
        "has_assigned_town_flipbook",
        "get_default_town_flipbook_path_string",
        "is_town_moving",
    ):
        try:
            result[method_name] = getattr(character, method_name)()
        except Exception:
            pass
    try:
        result["facing"] = _enum_name(character.get_town_facing_direction())
    except Exception:
        pass
    try:
        result["current_flipbook"] = _object_path(character.get_current_town_flipbook())
    except Exception:
        pass
    result["visual"] = _visual_summary(character)
    try:
        collision = character.get_town_collision_component()
        result["collision_enabled"] = _enum_name(collision.get_collision_enabled())
        result["generate_overlap_events"] = bool(collision.get_generate_overlap_events())
    except Exception:
        pass
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
        "is_town_moving",
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
    result["visual"] = _visual_summary(pawn)
    result["spring_arm"] = _spring_arm_summary(pawn)
    result["camera"] = _camera_summary(pawn)
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
    subsystem = _get_mvp_subsystem(world) or _get_mvp_subsystem_from_player_controller(player_controller)
    pie_viewport_diagnostics = []
    pie_viewport = _pie_viewport_summary(player_controller, pie_viewport_diagnostics)
    return {
        "ok": world is not None,
        "has_pie_world": world is not None,
        "map_name": _get_map_name(world),
        "world_path": _object_path(world),
        "pie_viewport": pie_viewport,
        "pie_viewport_diagnostics": pie_viewport_diagnostics,
        "runtime_state": _runtime_state(subsystem),
        "save_state": _save_state(),
        "player_controller": _player_controller_summary(player_controller),
        "hud": _hud_summary(hud),
        "pawn": _hero_summary(pawn),
        "actors": _actors_summary(world),
        "battle_board": _battle_board_summary(world, player_controller, subsystem),
    }


def _delete_default_save_if_present():
    existed = bool(unreal.GameplayStatics.does_save_game_exist(DEFAULT_SAVE_SLOT, 0))
    if not existed:
        return {"ok": True, "existed": False, "deleted": False}
    deleted = bool(unreal.GameplayStatics.delete_game_in_slot(DEFAULT_SAVE_SLOT, 0))
    return {"ok": deleted, "existed": True, "deleted": deleted}


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--delete-default-save", action="store_true")
    parser.add_argument("--hud-command", default="")
    parser.add_argument("--town-command", default="")
    parser.add_argument("--town-key", nargs=2, metavar=("KEY", "STATE"))
    parser.add_argument("--town-interact", action="store_true")
    parser.add_argument("--route-node", type=int, default=None)
    parser.add_argument("--open-quest-offer", action="store_true")
    parser.add_argument("--accept-task-id", default="")
    parser.add_argument("--apply-battle-hud-fixture", action="store_true")
    parser.add_argument("--clear-battle-hud-fixture", action="store_true")
    parser.add_argument("--apply-target-outcome-fixture", default="")
    parser.add_argument("--clear-target-outcome-fixture", action="store_true")
    parser.add_argument("--apply-route-exit-acceptance-fixture", action="store_true")
    parser.add_argument("--clear-route-exit-acceptance-fixture", action="store_true")
    parser.add_argument("--high-res-screenshot", nargs=3, metavar=("NAME", "WIDTH", "HEIGHT"))
    parser.add_argument("--poll-high-res-screenshot", action="store_true")
    args = parser.parse_args(argv)

    result = {}
    world = _get_game_world()
    if args.delete_default_save:
        try:
            delete_detail = _delete_default_save_if_present()
            result["delete_default_save"] = bool(delete_detail["ok"])
            result["delete_default_save_detail"] = delete_detail
        except Exception as exc:
            result["delete_default_save_error"] = str(exc)
    if args.hud_command:
        result["hud_command"] = _handle_hud_command(world, args.hud_command)
    if args.town_command:
        result["town_command"] = _handle_town_command(world, args.town_command)
    if args.town_key is not None:
        result["town_key"] = _handle_town_key(world, args.town_key[0], args.town_key[1])
    if args.town_interact:
        result["town_interact"] = _handle_town_interact(world)
    if args.route_node is not None:
        result["route_node"] = _handle_route_node(world, args.route_node)
    if args.open_quest_offer:
        result["quest_offer"] = _handle_open_quest_offer(world)
    if args.accept_task_id:
        result["task_accept"] = _handle_accept_task_offer(world, args.accept_task_id)
    if args.apply_battle_hud_fixture:
        result["battle_hud_fixture"] = _handle_apply_battle_hud_fixture(world)
    if args.clear_battle_hud_fixture:
        result["battle_hud_fixture_clear"] = _handle_clear_battle_hud_fixture(world)
    if args.apply_target_outcome_fixture:
        result["target_outcome_fixture"] = _handle_apply_target_outcome_fixture(
            world,
            args.apply_target_outcome_fixture,
        )
    if args.clear_target_outcome_fixture:
        result["target_outcome_fixture_clear"] = _handle_clear_target_outcome_fixture(world)
    if args.apply_route_exit_acceptance_fixture:
        result["route_exit_acceptance_fixture"] = _handle_apply_route_exit_acceptance_fixture(world)
    if args.clear_route_exit_acceptance_fixture:
        result["route_exit_acceptance_fixture_clear"] = _handle_clear_route_exit_acceptance_fixture(world)
    if args.high_res_screenshot is not None:
        result["high_res_screenshot"] = _handle_high_res_screenshot(
            world,
            args.high_res_screenshot[0],
            args.high_res_screenshot[1],
            args.high_res_screenshot[2],
        )
    if args.poll_high_res_screenshot:
        result["high_res_screenshot"] = _handle_poll_high_res_screenshot()
    result["probe"] = probe()
    print(_strict_json_dumps(result))


if __name__ == "__main__":
    main(sys.argv[1:])
