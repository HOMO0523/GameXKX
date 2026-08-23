"""Read and exercise PartyDeck runtime evidence inside an already-running PIE world.

This is a deliberately narrow MCP helper.  It never saves a package, map, or
SaveGame and never changes an asset.  Every mutating action below calls the
same public UMG/Subsystem API that a player-facing interaction uses.  Forced
reward/event fixtures are intentionally *not* fabricated through private
runtime-state reflection; the canonical C++ automation tests own those pure
fixture branches.
"""

from __future__ import annotations

import argparse
import json
import sys

import unreal


PARTY_DECK_RUNTIME_PROBE_VERSION = "2026-07-17.4"


def _object_path(value):
    if value is None:
        return ""
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def _class_name(value):
    if value is None:
        return ""
    try:
        return value.get_class().get_name()
    except Exception:
        return type(value).__name__


def _enum_name(value):
    if value is None:
        return ""
    try:
        return str(value.name)
    except Exception:
        return str(value).rsplit(".", 1)[-1]


def _enum_token(value):
    return _enum_name(value).rsplit("::", 1)[-1]


def _name(value):
    return str(value or "")


def _to_unreal_name(value):
    try:
        return unreal.Name(str(value))
    except Exception:
        return str(value)


def _text(value):
    if value is None:
        return ""
    try:
        return value.to_string()
    except Exception:
        return str(value)


def _vec2(value):
    if value is None:
        return {}
    return {"x": float(getattr(value, "x", 0.0)), "y": float(getattr(value, "y", 0.0))}


def _vec3(value):
    if value is None:
        return {}
    return {
        "x": float(getattr(value, "x", 0.0)),
        "y": float(getattr(value, "y", 0.0)),
        "z": float(getattr(value, "z", 0.0)),
    }


def _prop(value, *names):
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


def _call(value, name, *args):
    if value is None:
        return None
    try:
        return getattr(value, name)(*args)
    except Exception:
        return None


def _get_editor_subsystem():
    try:
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    except Exception:
        return None


def _get_game_world():
    subsystem = _get_editor_subsystem()
    return _call(subsystem, "get_game_world")


def _first_controller(world):
    try:
        return unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    except Exception:
        return None


def _get_game_instance(world):
    value = _call(world, "get_game_instance")
    if value:
        return value
    try:
        return unreal.GameplayStatics.get_game_instance(world) if world else None
    except Exception:
        return None


def _get_mvp_subsystem(world, controller):
    game_instance = _get_game_instance(world)
    subsystem_type = getattr(unreal, "GameXXKMVPSubsystem", None)
    if game_instance and subsystem_type:
        value = _call(game_instance, "get_subsystem", subsystem_type)
        if value:
            return value
    for getter in (
        "get_battle_board_widget_for_test",
        "get_route_map_widget_for_test",
        "get_town_overlay_widget_for_test",
    ):
        widget = _call(controller, getter)
        subsystem = _call(widget, "get_mvp_subsystem")
        if subsystem:
            return subsystem
    return None


def _state(subsystem):
    return _call(subsystem, "get_runtime_state_copy")


def _find_pending_node(state):
    pending_id = _prop(state, "pending_route_node_id", "PendingRouteNodeId")
    try:
        pending_id = int(pending_id)
    except Exception:
        pending_id = -1
    for node in _prop(state, "route_map_nodes", "RouteMapNodes") or []:
        try:
            node_id = int(_prop(node, "node_id", "NodeId"))
        except Exception:
            continue
        if node_id == pending_id:
            return node
    return None


def _instance_summary(instance):
    return {
        "instance_id": _name(_prop(instance, "instance_id", "InstanceId")),
        "card_id": _name(_prop(instance, "card_id", "CardId")),
        "owner_unit_id": _name(_prop(instance, "owner_unit_id", "OwnerUnitId")),
        "source_entry_id": _name(_prop(instance, "source_entry_id", "SourceEntryId")),
    }


def _unit_summary(unit):
    return {
        "unit_id": _name(_prop(unit, "unit_id", "UnitId")),
        "side": _enum_name(_prop(unit, "side", "Side")),
        "role": _enum_name(_prop(unit, "role", "Role")),
        "living": bool(_prop(unit, "b_living", "living", "bLiving", "Living")),
        "hp": int(_prop(unit, "hp", "HP") or 0),
        "max_hp": int(_prop(unit, "max_hp", "MaxHP") or 0),
        "mana": int(_prop(unit, "mana", "Mana") or 0),
        "max_mana": int(_prop(unit, "max_mana", "MaxMana") or 0),
    }


def _card_run_summary(state):
    run = _prop(state, "card_run", "CardRun")
    if run is None:
        return {}
    active_battle = _prop(run, "active_battle", "ActiveBattle")
    deck = _prop(active_battle, "deck", "Deck")
    party = _prop(run, "party_selection", "PartySelection")
    quest = _prop(party, "quest_npc", "QuestNpc")
    pending_reward = _prop(run, "pending_reward", "PendingReward")
    pending_event = _prop(run, "pending_event", "PendingEvent")
    route_progress = _prop(run, "route_progress", "RouteProgress")
    route_attributes = _prop(run, "route_attribute_bonuses", "RouteAttributeBonuses")
    relics = _prop(run, "relics", "Relics") or []
    rewarded_nodes = _prop(run, "rewarded_travel_money_nodes", "RewardedTravelMoneyNodes") or []
    return {
        "has_active_card_battle": bool(_prop(run, "b_has_active_card_battle", "has_active_card_battle", "bHasActiveCardBattle")),
        "loadout_locked_for_route": bool(_prop(run, "b_loadout_locked_for_route", "loadout_locked_for_route", "bLoadoutLockedForRoute")),
        "route_travel_money": int(_prop(run, "route_travel_money", "RouteTravelMoney") or 0),
        "route_progress_chapter": int(_prop(route_progress, "current_chapter", "CurrentChapter") or 0),
        "route_max_health_bonus": int(_prop(route_attributes, "max_health", "MaxHealth") or 0),
        "next_relic_acquisition_ordinal": int(_prop(run, "next_relic_acquisition_ordinal", "NextRelicAcquisitionOrdinal") or 0),
        "rewarded_travel_money_nodes": [
            {
                "chapter": int(_prop(receipt, "chapter", "Chapter") or 0),
                "node_id": int(_prop(receipt, "node_id", "NodeId") if _prop(receipt, "node_id", "NodeId") is not None else -1),
                "amount": int(_prop(receipt, "amount", "Amount") or 0),
            }
            for receipt in rewarded_nodes
        ],
        "relic_ids": [
            _name(_prop(relic, "relic_id", "RelicId"))
            for relic in relics
            if _name(_prop(relic, "relic_id", "RelicId"))
        ],
        "relics": [
            {
                "relic_id": _name(_prop(relic, "relic_id", "RelicId")),
                "stacks": int(_prop(relic, "stacks", "Stacks") or 0),
                "acquisition_ordinal": int(_prop(relic, "acquisition_ordinal", "AcquisitionOrdinal") or 0),
            }
            for relic in relics
            if _name(_prop(relic, "relic_id", "RelicId"))
        ],
        "boss_card_slots": [_name(value) for value in (_prop(run, "boss_card_slots", "BossCardSlots") or [])],
        "active_temporary_quest_npc_id": _name(_prop(run, "active_temporary_quest_npc_id", "ActiveTemporaryQuestNpcId")),
        "party_selection": {
            "active_permanent_companion_instance_id": _name(_prop(party, "active_permanent_companion_instance_id", "ActivePermanentCompanionInstanceId")),
            "quest_npc": {
                "npc_id": _name(_prop(quest, "npc_id", "NpcId")),
                "selected_card_ids": [_name(value) for value in (_prop(quest, "selected_card_ids", "SelectedCardIds") or [])],
            },
        },
        "pending_reward": {
            "source_node_id": int(_prop(pending_reward, "source_node_id", "SourceNodeId") or -1),
            "card_ids": [_name(value) for value in (_prop(pending_reward, "card_ids", "CardIds") or [])],
            "requires_route_card_replacement": bool(_prop(pending_reward, "b_requires_route_card_replacement", "requires_route_card_replacement", "bRequiresRouteCardReplacement")),
        },
        "pending_event": {
            "source_node_id": int(_prop(pending_event, "source_node_id", "SourceNodeId") or -1),
            "event_npc_id": _name(_prop(pending_event, "event_npc_id", "EventNpcId")),
            "can_recruit_permanent_companion": bool(_prop(pending_event, "b_can_recruit_permanent_companion", "can_recruit_permanent_companion", "bCanRecruitPermanentCompanion")),
        },
        "active_battle": {
            "phase": _enum_name(_prop(active_battle, "phase", "Phase")),
            "round_number": int(_prop(active_battle, "round_number", "RoundNumber") or 0),
            "shared_energy": int(_prop(deck, "shared_energy", "SharedEnergy") or 0),
            "hand_limit": int(_prop(deck, "hand_limit", "HandLimit") or 0),
            "hand": [_instance_summary(item) for item in (_prop(deck, "hand", "Hand") or [])],
            "units": [_unit_summary(item) for item in (_prop(active_battle, "units", "Units") or [])],
        },
    }


def _runtime_state_summary(state, subsystem=None):
    pending_node = _find_pending_node(state)
    healing_powder_count = 0
    healing_powder_count_observed = False
    subsystem_count = _call(subsystem, "get_item_count", _to_unreal_name("Item.HealingPowder"))
    if subsystem_count is not None:
        healing_powder_count = int(subsystem_count)
        healing_powder_count_observed = True
    inventory = _prop(state, "inventory", "Inventory")
    if not healing_powder_count_observed:
        try:
            for item_id, quantity in inventory.items():
                if _name(item_id) == "Item.HealingPowder":
                    healing_powder_count = int(quantity)
                    healing_powder_count_observed = True
                    break
        except Exception:
            pass
    return {
        "screen": _enum_name(_prop(state, "screen", "Screen")),
        "current_map_id": _name(_prop(state, "current_map_id", "CurrentMapId")),
        "player_gold": int(_prop(state, "player_gold", "PlayerGold") or 0),
        "player_hp": int(_prop(state, "player_hp", "PlayerHP") or 0),
        "player_max_hp": int(_prop(state, "player_max_hp", "PlayerMaxHP") or 0),
        "healing_powder_count": healing_powder_count,
        "healing_powder_count_observed": healing_powder_count_observed,
        "pending_route_node_id": int(_prop(state, "pending_route_node_id", "PendingRouteNodeId") or -1),
        "pending_route_node_kind": _enum_name(_prop(pending_node, "node_kind", "NodeKind")),
        "visited_route_node_ids": [int(value) for value in (_prop(state, "visited_route_node_ids", "VisitedRouteNodeIds") or [])],
        "b_dungeon_active": bool(_prop(state, "b_dungeon_active", "dungeon_active", "bDungeonActive")),
    }


def _widget_visible(widget):
    if widget is None:
        return False
    in_viewport = bool(_call(widget, "is_in_viewport"))
    visibility = _enum_name(_call(widget, "get_visibility")).upper()
    return in_viewport and ("VISIBLE" in visibility or "SELFHITTESTINVISIBLE" in visibility)


def _battle_board_summary(controller, card_run):
    board = _call(controller, "get_battle_board_widget_for_test")
    if board is None:
        return {}
    unit_ids = [_name(item.get("unit_id")) for item in card_run.get("active_battle", {}).get("units", []) if _name(item.get("unit_id"))]
    highlighted = []
    for unit_id in unit_ids:
        if bool(_call(board, "is_target_unit_highlighted", _to_unreal_name(unit_id))):
            highlighted.append(unit_id)
    pending_card_instance_id = _name(_call(board, "get_pending_card_instance_id_for_test"))
    pending_hand_card = next(
        (
            item
            for item in card_run.get("active_battle", {}).get("hand", [])
            if isinstance(item, dict) and _name(item.get("instance_id")) == pending_card_instance_id
        ),
        {},
    )
    source = _call(board, "get_targeting_source_position_for_test")
    pointer = _call(board, "get_targeting_pointer_position_for_test")
    legal = _prop(board, "legal_card_target_unit_ids", "LegalCardTargetUnitIds") or []
    return {
        "path": _object_path(board),
        "is_in_viewport": _widget_visible(board),
        "is_battle_board_visible": bool(_call(board, "is_battle_board_visible")),
        "is_card_targeting_active": bool(_call(board, "is_card_targeting_active")),
        "pending_card_instance_id": pending_card_instance_id,
        "pending_card_owner_unit_id": _name(pending_hand_card.get("owner_unit_id")),
        "legal_target_unit_ids": sorted({_name(value) for value in legal if _name(value)} | set(highlighted)),
        "targeting_source_position": _vec2(source),
        "targeting_pointer_position": _vec2(pointer),
        "has_pending_route_reward": bool(_call(board, "has_pending_route_reward")),
        "pending_route_reward_card_ids": [_name(value) for value in (_call(board, "get_pending_route_reward_card_ids") or [])],
        "route_reward_replacement_card_ids": [_name(value) for value in (_call(board, "get_route_reward_replacement_card_ids") or [])],
        "selected_route_reward_replacement_card_id": _name(_prop(board, "selected_route_reward_replacement_card_id", "SelectedRouteRewardReplacementCardId")),
    }


def _project_world_to_screen(controller, location):
    if controller is None or location is None:
        return {}
    for name in ("project_world_location_to_screen", "project_world_location_to_widget_position"):
        try:
            result = getattr(controller, name)(location, False)
        except Exception:
            continue
        if isinstance(result, tuple):
            values = list(result)
            if len(values) >= 2 and isinstance(values[-1], unreal.Vector2D):
                return _vec2(values[-1])
        if isinstance(result, unreal.Vector2D):
            return _vec2(result)
    return {}


def _all_actors(world):
    try:
        return unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor) if world else []
    except Exception:
        return []


def _battle_scene_summaries(world, controller):
    result = []
    for actor in _all_actors(world):
        if "BattleSceneUnitActor" not in _class_name(actor):
            continue
        visual = _call(actor, "get_battle_visual_component")
        flipbook = _call(actor, "get_current_battle_flipbook")
        status_widget = _call(actor, "get_status_widget_component_for_test")
        summary = {
            "unit_id": _name(_call(actor, "get_unit_id")),
            "is_enemy_unit": bool(_call(actor, "is_enemy_unit")),
            "is_card_target_highlighted": bool(_call(actor, "is_card_target_highlighted")),
            "is_card_target_outline_enabled": bool(_call(actor, "is_card_target_outline_enabled")),
            "has_visual": bool(visual and _call(visual, "is_visible") is not False and flipbook),
            "flipbook": _object_path(flipbook),
            "location": _vec3(_call(actor, "get_actor_location")),
            "screen_position": _project_world_to_screen(controller, _call(actor, "get_actor_location")),
        }
        if status_widget:
            summary["foot_status_widget"] = {
                "visible": _call(status_widget, "is_visible"),
                "hidden_in_game": _call(status_widget, "is_hidden_in_game"),
                "draw_size": _vec2(_call(status_widget, "get_draw_size")),
                "relative_location": _vec3(_prop(status_widget, "relative_location", "RelativeLocation")),
                "world_location": _vec3(_call(status_widget, "get_component_location")),
                "screen_position": _project_world_to_screen(controller, _call(status_widget, "get_component_location")),
            }
        result.append(summary)
    return result


def _route_encounter_summary(world, state, card_run):
    result = []
    event_npc_id = _name(card_run.get("pending_event", {}).get("event_npc_id"))
    for actor in _all_actors(world):
        if "RouteEncounterSceneActor" not in _class_name(actor):
            continue
        feedback_component = _call(actor, "get_feedback_text_component")
        label = ""
        try:
            label = actor.get_actor_label()
        except Exception:
            label = actor.get_name()
        screen = _enum_name(_call(actor, "get_encounter_screen"))
        result.append({
            "name": actor.get_name(),
            "path": _object_path(actor),
            "label": label,
            "class": _class_name(actor),
            "screen": screen,
            "matches_runtime_screen": bool(_call(actor, "matches_runtime_screen", _prop(state, "screen", "Screen"))),
            "last_interaction_successful": bool(_call(actor, "was_last_interaction_successful")),
            "last_failure_reason": _text(_call(actor, "get_last_failure_reason")),
            "feedback_text": _text(_prop(feedback_component, "text", "Text")),
            "event_npc_id": event_npc_id,
            "event_only_identity": event_npc_id in {"Event.NiuHuan", "Npc.Event.NiuHuan", "Npc.NiuHuan", "NiuHuan"},
        })
    return result


def _active_widget_summaries(world):
    raw = []
    try:
        raw = unreal.WidgetBlueprintLibrary.get_all_widgets_of_class(world, unreal.UserWidget, False)
    except Exception:
        return []
    return [
        {
            "name": item.get_name(),
            "class": _class_name(item),
            "path": _object_path(item),
            "is_in_viewport": bool(_call(item, "is_in_viewport")),
            "visibility": _enum_name(_call(item, "get_visibility")),
        }
        for item in raw
    ]


def _route_panel_summary(controller, runtime_state, route_encounters, widgets):
    expected_kind = _name(runtime_state.get("pending_route_node_kind"))
    expected_screen = _name(runtime_state.get("screen"))
    panel = _call(controller, "get_route_encounter_panel_widget_for_test")
    primary_action = _enum_token(_call(panel, "get_primary_action_for_test"))
    secondary_action = _enum_token(_call(panel, "get_secondary_action_for_test"))
    widget_tree = _prop(panel, "widget_tree", "WidgetTree")
    primary_button = _call(widget_tree, "find_widget", _to_unreal_name("RouteEncounterPrimaryAction"))
    secondary_button = _call(widget_tree, "find_widget", _to_unreal_name("RouteEncounterSecondaryAction"))
    panel_widget = next((item for item in widgets if "RouteEncounterPanelWidget" in _name(item.get("class"))), {})
    source_actor_path = _object_path(_call(controller, "get_route_encounter_source_actor_for_test"))
    matched_actor = next((item for item in route_encounters if _name(item.get("path")) == source_actor_path), {})
    if not matched_actor:
        matched_actor = next((item for item in route_encounters if _name(item.get("screen")) == expected_screen), {})
    # The panel exposes its own presentation state, while the controller owns
    # the authoritative modal-open bit.  Read the latter so a swallowed Python
    # reflection miss cannot turn every live panel into a false negative.
    is_open = bool(_call(controller, "is_route_encounter_panel_open_for_test"))
    is_in_viewport = bool(_call(panel, "is_in_viewport"))
    explicit = is_open and primary_action not in {"", "None"} and secondary_action not in {"", "None"}
    return {
        "kind": expected_kind,
        "screen": expected_screen,
        "class": _class_name(panel),
        "path": _object_path(panel),
        "is_in_viewport": is_in_viewport,
        "is_open": is_open,
        "is_explicit": explicit,
        "speaker": _text(_call(panel, "get_speaker_text_for_test")),
        "primary_action": primary_action,
        "secondary_action": secondary_action,
        "primary_label": _text(_call(panel, "get_primary_action_text_for_test")),
        "secondary_label": _text(_call(panel, "get_secondary_action_text_for_test")),
        "primary_enabled": bool(_call(primary_button, "get_is_enabled")),
        "secondary_enabled": bool(_call(secondary_button, "get_is_enabled")),
        "primary_tooltip": _text(_call(primary_button, "get_tool_tip_text")),
        "secondary_tooltip": _text(_call(secondary_button, "get_tool_tip_text")),
        "window_frame_resource_path": _name(_call(panel, "get_window_frame_resource_path_for_test")),
        "header_resource_path": _name(_call(panel, "get_header_resource_path_for_test")),
        "action_resource_path": _name(_call(panel, "get_action_resource_path_for_test")),
        "source_actor_path": source_actor_path,
        "actor": matched_actor,
        "widget": panel_widget,
    }


def _party_visual_summary(card_run, battle_units):
    active_battle = card_run.get("active_battle", {})
    roles = {_name(item.get("unit_id")): _name(item.get("role")) for item in active_battle.get("units", []) if isinstance(item, dict)}
    return [
        {
            "unit_id": _name(item.get("unit_id")),
            "role": roles.get(_name(item.get("unit_id")), ""),
            "has_visual": bool(item.get("has_visual")),
            "flipbook": _name(item.get("flipbook")),
        }
        for item in battle_units
        if not bool(item.get("is_enemy_unit"))
    ]


def _snapshot():
    world = _get_game_world()
    controller = _first_controller(world)
    subsystem = _get_mvp_subsystem(world, controller)
    state = _state(subsystem)
    runtime_state = _runtime_state_summary(state, subsystem)
    card_run = _card_run_summary(state)
    battle_units = _battle_scene_summaries(world, controller)
    route_encounters = _route_encounter_summary(world, state, card_run)
    widgets = _active_widget_summaries(world)
    return {
        "ok": world is not None and subsystem is not None,
        "version": PARTY_DECK_RUNTIME_PROBE_VERSION,
        "world": _object_path(world),
        "map_name": _name(_call(world, "get_map_name") or _call(world, "get_name")),
        "runtime_state": runtime_state,
        "card_run": card_run,
        "battle_board": _battle_board_summary(controller, card_run),
        "battle_units": battle_units,
        "route_encounters": route_encounters,
        "route_panel": _route_panel_summary(controller, runtime_state, route_encounters, widgets),
        "party_visuals": _party_visual_summary(card_run, battle_units),
        "widgets": widgets,
    }


def _board(controller):
    return _call(controller, "get_battle_board_widget_for_test")


def _matching_route_encounter_actor(world, controller):
    """Find the placed actor that represents the active route encounter.

    The probe never calls the controller modal helper directly.  It uses the
    pawn's public F binding after first setting the same focused actor that the
    overlap component would own in real play.
    """
    subsystem = _get_mvp_subsystem(world, controller)
    state = _state(subsystem)
    active_screen = _prop(state, "screen", "Screen")
    for actor in _all_actors(world):
        if "RouteEncounterSceneActor" not in _class_name(actor):
            continue
        if bool(_call(actor, "matches_runtime_screen", active_screen)):
            return actor
    return None


def _action(action, controller, args):
    board = _board(controller)
    if action == "snapshot":
        return {"ok": True, "kind": "read_only"}
    if action == "click-card":
        return {"ok": bool(_call(board, "click_card_in_hand", _to_unreal_name(args.card_instance_id))), "card_instance_id": args.card_instance_id}
    if action == "update-pointer":
        if board is None:
            return {"ok": False, "reason": "battle_board_missing"}
        _call(board, "update_targeting_pointer", unreal.Vector2D(float(args.x), float(args.y)))
        return {"ok": bool(_call(board, "is_card_targeting_active")), "x": float(args.x), "y": float(args.y)}
    if action == "cancel-targeting":
        return {"ok": bool(_call(controller, "cancel_battle_targeting_for_test"))}
    if action == "confirm-target":
        return {"ok": bool(_call(board, "confirm_targeting_unit", _to_unreal_name(args.unit_id))), "unit_id": args.unit_id}
    if action == "end-turn":
        return {"ok": bool(_call(board, "end_card_player_phase"))}
    if action == "select-replacement":
        return {"ok": bool(_call(board, "select_route_reward_replacement_card", _to_unreal_name(args.route_card_id))), "route_card_id": args.route_card_id}
    if action == "choose-reward":
        return {
            "ok": bool(_call(board, "choose_pending_route_reward", _to_unreal_name(args.reward_card_id), _to_unreal_name(args.replace_route_card_id or "None"))),
            "reward_card_id": args.reward_card_id,
            "replace_route_card_id": args.replace_route_card_id,
        }
    if action == "skip-reward":
        return {"ok": bool(_call(board, "skip_pending_route_reward"))}
    if action in {"open-route-encounter-panel", "interact-route-encounter"}:
        # Exercise the pawn's public F binding.  The first unfocused attempt
        # proves that the component's nearby-target fallback cannot bypass the
        # controller gate; the second attempt uses the exact scene actor as focus.
        # Both attempts only open a visible panel.  No route rule is dispatched
        # until a panel button is chosen below.
        world = _get_game_world()
        actor = _matching_route_encounter_actor(world, controller)
        if actor is None:
            return {"ok": False, "reason": "matching_route_encounter_actor_missing"}
        pawn = _call(controller, "get_pawn")
        if pawn is None:
            return {"ok": False, "reason": "player_pawn_missing"}
        interaction = _call(pawn, "get_interaction_component")
        if interaction is None:
            return {"ok": False, "reason": "player_interaction_component_missing"}

        actor_path = _object_path(actor)
        _call(interaction, "set_focused_actor", None)
        _call(pawn, "interact")
        unfocused_panel_open = bool(_call(controller, "is_route_encounter_panel_open_for_test"))
        rejected_when_unfocused = not unfocused_panel_open

        _call(interaction, "set_focused_actor", actor)
        focused_actor_path = _object_path(_call(interaction, "get_focused_actor"))
        _call(pawn, "interact")
        panel_open = bool(_call(controller, "is_route_encounter_panel_open_for_test"))
        source_actor_path = _object_path(_call(controller, "get_route_encounter_source_actor_for_test"))
        source_last_interaction_successful_before_choice = bool(_call(actor, "was_last_interaction_successful"))
        return {
            "ok": bool(
                rejected_when_unfocused
                and panel_open
                and actor_path
                and focused_actor_path == actor_path
                and source_actor_path == actor_path
                and not source_last_interaction_successful_before_choice
            ),
            "kind": "open_route_encounter_panel_only",
            "input_path": "pawn_interaction_binding",
            "actor": actor_path,
            "rejected_when_unfocused": rejected_when_unfocused,
            "unfocused_panel_open": unfocused_panel_open,
            "focused_actor_path": focused_actor_path,
            "source_actor_path": source_actor_path,
            "source_last_interaction_successful_before_choice": source_last_interaction_successful_before_choice,
        }
    if action == "trigger-route-encounter-primary":
        panel = _call(controller, "get_route_encounter_panel_widget_for_test")
        selected_action = _enum_token(_call(panel, "get_primary_action_for_test"))
        source_actor = _call(controller, "get_route_encounter_source_actor_for_test")
        source_actor_path = _object_path(source_actor)
        triggered = bool(_call(panel, "trigger_primary_action_for_test"))
        return {
            "ok": triggered,
            "kind": "trigger-route-encounter-primary",
            "selected_action": selected_action,
            "source_actor_path_before_choice": source_actor_path,
            "source_last_interaction_successful_after_choice": bool(_call(source_actor, "was_last_interaction_successful")),
        }
    if action == "trigger-route-encounter-secondary":
        panel = _call(controller, "get_route_encounter_panel_widget_for_test")
        selected_action = _enum_token(_call(panel, "get_secondary_action_for_test"))
        source_actor = _call(controller, "get_route_encounter_source_actor_for_test")
        source_actor_path = _object_path(source_actor)
        triggered = bool(_call(panel, "trigger_secondary_action_for_test"))
        return {
            "ok": triggered,
            "kind": "trigger-route-encounter-secondary",
            "selected_action": selected_action,
            "source_actor_path_before_choice": source_actor_path,
            "source_last_interaction_successful_after_choice": bool(_call(source_actor, "was_last_interaction_successful")),
        }
    return {"ok": False, "reason": f"unsupported_action:{action}"}


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--action", default="snapshot")
    parser.add_argument("--card-instance-id", default="")
    parser.add_argument("--unit-id", default="")
    parser.add_argument("--route-card-id", default="")
    parser.add_argument("--reward-card-id", default="")
    parser.add_argument("--replace-route-card-id", default="")
    parser.add_argument("--x", type=float, default=0.0)
    parser.add_argument("--y", type=float, default=0.0)
    args = parser.parse_args(argv)

    world = _get_game_world()
    controller = _first_controller(world)
    payload_before_action = _snapshot()
    action_result = _action(args.action, controller, args)
    payload = _snapshot()
    payload["before_action"] = payload_before_action
    payload["action"] = action_result
    print(json.dumps(payload, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main(sys.argv[1:])
