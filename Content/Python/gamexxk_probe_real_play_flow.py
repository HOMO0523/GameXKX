from __future__ import annotations

import argparse
import json
import math
import sys

import unreal


DEFAULT_SAVE_SLOT = "GameXXK_MVP_SaveSlot_1"


def _vector_to_dict(value):
    if value is None:
        return None
    return {
        "x": float(getattr(value, "x", 0.0)),
        "y": float(getattr(value, "y", 0.0)),
        "z": float(getattr(value, "z", 0.0)),
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
        ("b_has_player_location", ("b_has_player_location", "has_player_location", "bHasPlayerLocation", "HasPlayerLocation")),
        ("player_location", ("player_location", "PlayerLocation")),
        ("b_follower_joined", ("b_follower_joined", "follower_joined", "bFollowerJoined", "FollowerJoined")),
        ("b_has_quest_npc_location", ("b_has_quest_npc_location", "has_quest_npc_location", "bHasQuestNpcLocation", "HasQuestNpcLocation")),
        ("quest_npc_location", ("quest_npc_location", "QuestNpcLocation")),
        ("b_dungeon_active", ("b_dungeon_active", "dungeon_active", "bDungeonActive", "DungeonActive")),
        ("dungeon_node_index", ("dungeon_node_index", "DungeonNodeIndex")),
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
    return {
        "ok": world is not None,
        "has_pie_world": world is not None,
        "map_name": _get_map_name(world),
        "world_path": _object_path(world),
        "runtime_state": _runtime_state(subsystem),
        "save_state": _save_state(),
        "player_controller": _player_controller_summary(player_controller),
        "hud": _hud_summary(hud),
        "pawn": _hero_summary(pawn),
        "actors": _actors_summary(world),
    }


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--delete-default-save", action="store_true")
    parser.add_argument("--hud-command", default="")
    parser.add_argument("--town-command", default="")
    parser.add_argument("--route-node", type=int, default=None)
    args = parser.parse_args(argv)

    result = {}
    world = _get_game_world()
    if args.delete_default_save:
        try:
            result["delete_default_save"] = bool(unreal.GameplayStatics.delete_game_in_slot(DEFAULT_SAVE_SLOT, 0))
        except Exception as exc:
            result["delete_default_save_error"] = str(exc)
    if args.hud_command:
        result["hud_command"] = _handle_hud_command(world, args.hud_command)
    if args.town_command:
        result["town_command"] = _handle_town_command(world, args.town_command)
    if args.route_node is not None:
        result["route_node"] = _handle_route_node(world, args.route_node)
    result["probe"] = probe()
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main(sys.argv[1:])
