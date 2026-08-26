"""Transient 2K pilot session helper: battle setup, hero attack trigger, screenshot."""

import json
import sys
import time

import unreal


def _world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


def _call(obj, name, *args):
    fn = getattr(obj, name, None)
    if fn is None or not callable(fn):
        return None
    try:
        return fn(*args)
    except Exception as exc:
        return f"ERR:{exc}"


def _prop(value, name):
    try:
        return value.get_editor_property(name)
    except Exception:
        return None


def _subsystem(world):
    gi = _call(world, "get_game_instance")
    if gi is None:
        try:
            gi = unreal.GameplayStatics.get_game_instance(world)
        except Exception:
            gi = None
    subsystem_type = getattr(unreal, "GameXXKMVPSubsystem", None)
    if gi and subsystem_type:
        sub = _call(gi, "get_subsystem", subsystem_type)
        if sub and not isinstance(sub, str):
            return sub
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    for getter in ("get_battle_board_widget_for_test", "get_route_map_widget_for_test", "get_town_overlay_widget_for_test"):
        widget = _call(controller, getter)
        sub = _call(widget, "get_mvp_subsystem") if widget and not isinstance(widget, str) else None
        if sub and not isinstance(sub, str):
            return sub
    return None


def _setup_battle(sub):
    steps = []
    steps.append(("start_new_game", bool(_call(sub, "start_new_game"))))
    steps.append(("accept_quest", bool(_call(sub, "accept_quest"))))
    steps.append(("open_dungeon_from_town_exit", bool(_call(sub, "open_dungeon_from_town_exit"))))
    steps.append(("select_start", bool(_call(sub, "select_route_node_by_id", 0))))
    node_kind = getattr(unreal, "GameXXKNodeKind", None)
    battle_kind = getattr(node_kind, "BATTLE", None) if node_kind else None
    steps.append(("select_battle", bool(_call(sub, "select_dungeon_node", battle_kind)) if battle_kind is not None else "enum_missing"))
    return steps


def _trigger_hero_attack(board):
    import importlib
    import gamexxk_trigger_battle_animation_sample
    importlib.reload(gamexxk_trigger_battle_animation_sample)
    from gamexxk_trigger_battle_animation_sample import trigger_card_attack
    world = _world()
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    sub = _subsystem(world)
    state = _call(sub, "get_runtime_state_copy")
    run = _prop(state, "card_run")
    battle = _prop(run, "active_battle")
    deck = _prop(battle, "deck")
    hand = _prop(deck, "hand") or []
    cards = []
    for item in hand:
        try:
            cards.append({
                "card_id": str(item.get_editor_property("card_id")),
                "instance_id": str(item.get_editor_property("instance_id")),
                "owner_unit_id": str(item.get_editor_property("owner_unit_id")),
            })
        except Exception:
            pass
    units = _prop(battle, "units") or []
    enemy_unit_id = None
    for unit in units:
        try:
            if "ENEMY" in str(unit.get_editor_property("side")).upper() and bool(unit.get_editor_property("living")):
                enemy_unit_id = str(unit.get_editor_property("unit_id"))
                break
        except Exception:
            continue
    result = trigger_card_attack(board, cards, enemy_unit_id or "Enemy.Rooster.P3", unreal.Name)
    return result, cards, enemy_unit_id


def _screenshot(world):
    try:
        unreal.SystemLibrary.execute_console_command(world, "HighResShot 1280x720")
    except Exception as exc:
        return f"ERR:{exc}"
    time.sleep(2.0)
    shots_dir = unreal.Paths.project_saved_dir() + "Screenshots/WindowsEditor"
    return {"command": "ok", "dir": shots_dir}


def _enter_real_battle(sub, controller):
    """Enter the battle through the real route-map click path (map travel included)."""
    steps = []
    steps.append(("start_new_game", bool(_call(sub, "start_new_game"))))
    steps.append(("accept_quest", bool(_call(sub, "accept_quest"))))
    steps.append(("open_dungeon_from_town_exit", bool(_call(sub, "open_dungeon_from_town_exit"))))
    route_widget = _call(controller, "get_route_map_widget_for_test")
    if not route_widget or isinstance(route_widget, str):
        steps.append(("route_widget", "missing"))
        return steps
    steps.append(("execute_start_node", bool(_call(route_widget, "execute_route_node", 0))))
    # Find the first reachable battle node id from the runtime state copy.
    state = _call(sub, "get_runtime_state_copy")
    nodes = _prop(state, "route_map_nodes") if state and not isinstance(state, str) else None
    battle_node_id = None
    if nodes:
        for node in nodes:
            try:
                kind = str(node.get_editor_property("node_kind")).upper()
                node_id = int(node.get_editor_property("node_id"))
            except Exception:
                continue
            if "BATTLE" in kind and battle_node_id is None:
                battle_node_id = node_id
    steps.append(("battle_node_id", battle_node_id))
    if battle_node_id is not None:
        steps.append(("execute_battle_node", bool(_call(route_widget, "execute_route_node", battle_node_id))))
    return steps


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "setup"
    world = _world()
    if not world:
        print(json.dumps({"ok": False, "reason": "no_pie_world"}, ensure_ascii=False))
        return
    sub = _subsystem(world)
    if not sub:
        print(json.dumps({"ok": False, "reason": "no_subsystem"}, ensure_ascii=False))
        return
    out = {"mode": mode}
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    if mode == "setup":
        out["steps"] = _setup_battle(sub)
    elif mode == "real_entry":
        out["steps"] = _enter_real_battle(sub, controller)
    elif mode == "fixture":
        try:
            result = sub.apply_pilot_comparison_fixture_for_test()
            out["fixture"] = {
                "raw": str(result),
                "ok": bool(result[1]) if isinstance(result, tuple) and len(result) > 1 else bool(result),
                "error": str(result[0]) if isinstance(result, tuple) else "",
            }
        except Exception as exc:
            out["fixture"] = f"ERR:{exc}"
    board = _call(controller, "get_battle_board_widget_for_test")
    if board and not isinstance(board, str):
        _call(board, "refresh_from_state")
        if mode == "attack":
            out["attack"] = _trigger_hero_attack(board)
    if mode in ("idle_shot", "attack_shot"):
        if mode == "attack_shot":
            out["attack"] = _trigger_hero_attack(board) if board and not isinstance(board, str) else "board_missing"
            time.sleep(1.2)
        out["screenshot"] = _screenshot(world)
    state = _call(sub, "get_runtime_state_copy")
    run = _prop(state, "card_run") if state and not isinstance(state, str) else None
    active = _prop(run, "active_battle") if run else None
    if active:
        out["phase"] = str(_prop(active, "phase"))
        units = _prop(active, "units") or []
        out["units"] = []
        for unit in units:
            try:
                out["units"].append({
                    "id": str(unit.get_editor_property("unit_id")),
                    "side": str(unit.get_editor_property("side")),
                })
            except Exception:
                pass
    out["ok"] = True
    print(json.dumps(out, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
