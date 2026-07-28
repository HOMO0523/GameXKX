"""Drive and inspect one live PIE enemy-intent cycle without touching assets."""

from __future__ import annotations

import argparse
import json
import sys

import unreal


def _prop(target, *names):
    if target is None:
        return None
    for name in names:
        try:
            return getattr(target, name)
        except Exception:
            pass
        try:
            return target.get_editor_property(name)
        except Exception:
            pass
    return None


def _call(target, name, *args):
    if target is None:
        return None
    try:
        return getattr(target, name)(*args)
    except Exception:
        return None


def _enum(value):
    try:
        return str(value.name)
    except Exception:
        return str(value or "")


def _name(value):
    return str(value or "")


def _world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


def _controller(world):
    return unreal.GameplayStatics.get_player_controller(world, 0) if world else None


def _subsystem(world, controller):
    try:
        game_instance = world.get_game_instance() if world else None
    except Exception:
        game_instance = unreal.GameplayStatics.get_game_instance(world) if world else None
    subsystem_type = getattr(unreal, "GameXXKMVPSubsystem", None)
    try:
        return game_instance.get_subsystem(subsystem_type) if game_instance and subsystem_type else None
    except Exception:
        board = _call(controller, "get_battle_board_widget_for_test")
        return _call(board, "get_mvp_subsystem")


def _status_summary(status):
    return {
        "status": _enum(_prop(status, "status", "Status")),
        "stacks": int(_prop(status, "stacks", "Stacks") or 0),
    }


def _unit_summary(unit):
    return {
        "unit_id": _name(_prop(unit, "unit_id", "UnitId")),
        "side": _enum(_prop(unit, "side", "Side")),
        "living": bool(_prop(unit, "b_living", "bLiving")),
        "hp": int(_prop(unit, "hp", "HP") or 0),
        "max_hp": int(_prop(unit, "max_hp", "MaxHP") or 0),
        "attack": int(_prop(unit, "attack", "Attack") or 0),
        "defense": int(_prop(unit, "defense", "Defense") or 0),
        "armor": int(_prop(unit, "armor", "Armor") or 0),
        "statuses": [_status_summary(item) for item in (_prop(unit, "statuses", "Statuses") or [])],
    }


def _effect_summary(effect):
    return {
        "type": _enum(_prop(effect, "type", "Type")),
        "target_unit_ids": [_name(value) for value in (_prop(effect, "target_unit_ids", "TargetUnitIds") or [])],
        "magnitude": int(_prop(effect, "magnitude", "Magnitude") or 0),
        "base_magnitude": int(_prop(effect, "base_magnitude", "BaseMagnitude") or 0),
        "hit_count": int(_prop(effect, "hit_count", "HitCount") or 0),
        "status": _enum(_prop(effect, "status", "Status")),
        "status_stacks": int(_prop(effect, "status_stacks", "StatusStacks") or 0),
    }


def _intent_summary(intent):
    return {
        "source_unit_id": _name(_prop(intent, "source_unit_id", "SourceUnitId")),
        "name": str(_prop(intent, "card_display_name", "CardDisplayName") or ""),
        "definition_id": _name(_prop(intent, "intent_definition_id", "IntentDefinitionId")),
        "suggested_target": _name(_prop(intent, "suggested_target_unit_id", "SuggestedTargetUnitId")),
        "damage": int(_prop(intent, "damage", "Damage") or 0),
        "charging": bool(_prop(intent, "b_charging", "bCharging")),
        "effects": [_effect_summary(item) for item in (_prop(intent, "effects", "Effects") or [])],
        "tooltip_lines": [str(value) for value in (_prop(intent, "tooltip_lines", "TooltipLines") or [])],
    }


def _snapshot(world, subsystem):
    state = _call(subsystem, "get_runtime_state_copy")
    run = _prop(state, "card_run", "CardRun")
    battle = _prop(run, "active_battle", "ActiveBattle")
    route_nodes = _prop(state, "route_map_nodes", "RouteMapNodes") or []
    return {
        "world": _name(world.get_path_name()) if world else "",
        "screen": _enum(_prop(state, "screen", "Screen")),
        "dungeon_active": bool(_prop(state, "b_dungeon_active", "bDungeonActive")),
        "phase": _enum(_prop(battle, "phase", "Phase")),
        "round": int(_prop(battle, "round_number", "RoundNumber") or 0),
        "current_route_node_id": int(_prop(state, "current_route_node_id", "CurrentRouteNodeId") or 0),
        "reachable_route_node_ids": [int(value) for value in (_prop(state, "reachable_route_node_ids", "ReachableRouteNodeIds") or [])],
        "route_nodes": [
            {
                "node_id": int(_prop(node, "node_id", "NodeId") or 0),
                "kind": _enum(_prop(node, "node_kind", "NodeKind")),
            }
            for node in route_nodes
        ],
        "next_enemy_intent_index": int(_prop(run, "next_enemy_intent_index", "NextEnemyIntentIndex") or 0),
        "units": [_unit_summary(item) for item in (_prop(battle, "units", "Units") or [])],
        "enemy_intents": [_intent_summary(item) for item in (_prop(run, "enemy_intents", "EnemyIntents") or [])],
    }


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--action", default="snapshot")
    parser.add_argument("--node-id", type=int, default=2)
    args = parser.parse_args(argv)

    world = _world()
    controller = _controller(world)
    subsystem = _subsystem(world, controller)
    before = _snapshot(world, subsystem)
    result = {"ok": True, "action": args.action}

    if args.action == "return-town":
        result["ok"] = bool(_call(subsystem, "fail_dungeon_to_town"))
    elif args.action == "open-dungeon":
        result["ok"] = bool(_call(subsystem, "open_dungeon_from_town_exit"))
    elif args.action == "select-node":
        result["ok"] = bool(_call(subsystem, "select_route_node_by_id", args.node_id))
        result["node_id"] = args.node_id
    elif args.action == "end-turn":
        board = _call(controller, "get_battle_board_widget_for_test")
        result["ok"] = bool(_call(board, "end_card_player_phase"))
    elif args.action != "snapshot":
        result = {"ok": False, "action": args.action, "error": "unsupported_action"}

    print(json.dumps({"result": result, "before": before}, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main(sys.argv[1:])
