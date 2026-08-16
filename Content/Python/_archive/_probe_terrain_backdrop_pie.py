"""Transient probe: terrain-backdrop PIE verification for the Formation Master adaptation.

Actions:
  terrain          -> active battle terrain (value + name)
  backdrop         -> current backdrop brush texture asset path
  hand_formation   -> hand cards owned by the Formation Master profession
  enter_route      -> open the dungeon route map from the town exit
"""

import json
import sys

import unreal


def _world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


def _board(world):
    pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    if not pc:
        return None
    try:
        return pc.get_battle_board_widget_for_test()
    except Exception:
        return None


def _subsystem(world):
    game_instance = None
    try:
        game_instance = world.get_game_instance() if world else None
    except Exception:
        game_instance = unreal.GameplayStatics.get_game_instance(world) if world else None
    subsystem = None
    if game_instance:
        try:
            subsystem = game_instance.get_subsystem(unreal.GameXXKMVPSubsystem)
        except Exception:
            subsystem = None
    if subsystem is None:
        board = _board(world)
        if board:
            try:
                subsystem = board.get_mvp_subsystem()
            except Exception:
                subsystem = None
    return subsystem


def _property(target, *names):
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


def _name(value):
    return str(value or "")


def _find_widget_named(root, wanted):
    if not root:
        return None
    for getter in (lambda w: getattr(w, "name", None), lambda w: w.get_name()):
        try:
            if str(getter(root)) == wanted:
                return root
        except Exception:
            pass
    children = None
    try:
        children = root.get_all_children()
    except Exception:
        children = None
    for child in children or []:
        found = _find_widget_named(child, wanted)
        if found:
            return found
    return None


def _state_copy(world):
    subsystem = _subsystem(world)
    if not subsystem:
        return None
    return subsystem.get_runtime_state_copy()


def action_terrain(world):
    state = _state_copy(world)
    if state is None:
        return {"ok": False, "reason": "subsystem_missing"}
    run = _property(state, "card_run", "CardRun")
    battle = _property(run, "active_battle", "ActiveBattle")
    terrain = _property(battle, "terrain", "Terrain")
    if terrain is None:
        return {"ok": False, "reason": "terrain_missing"}
    terrain_value = _property(terrain, "value")
    if terrain_value is None:
        import re
        match = re.search(r": (\d+)>", _name(terrain))
        terrain_value = int(match.group(1)) if match else None
    raw_name = _name(terrain)
    member = raw_name.split(".", 1)[1].split(":", 1)[0] if "." in raw_name else raw_name
    return {"ok": True, "terrain": terrain_value, "terrain_name": member}


def action_backdrop(world):
    board = _board(world)
    if not board:
        return {"ok": False, "reason": "board_missing"}
    tree = _property(board, "widget_tree", "WidgetTree")
    root = _property(tree, "root_widget", "RootWidget") if tree else None
    image = _find_widget_named(root, "BattleBackdropImage")
    if not image:
        return {"ok": False, "reason": "backdrop_image_missing"}
    brush = _property(image, "brush", "Brush")
    resource = _property(brush, "resource_object", "ResourceObject") if brush else None
    if resource is None:
        return {"ok": False, "reason": "backdrop_resource_missing"}
    return {"ok": True, "texture": _name(resource.get_path_name())}


def action_hand_formation(world):
    state = _state_copy(world)
    if state is None:
        return {"ok": False, "reason": "subsystem_missing"}
    run = _property(state, "card_run", "CardRun")
    battle = _property(run, "active_battle", "ActiveBattle")
    deck = _property(battle, "deck", "Deck")
    hand = _property(deck, "hand", "Hand") or []
    cards = []
    for card in hand:
        card_id = _name(_property(card, "card_id", "CardId"))
        if "FormationMaster" in card_id:
            cards.append({"instance_id": _name(_property(card, "instance_id", "InstanceId")), "card_id": card_id})
    return {"ok": True, "formation_cards": cards}


def action_enter_route(world):
    subsystem = _subsystem(world)
    if not subsystem:
        return {"ok": False, "reason": "subsystem_missing"}
    try:
        ok = bool(subsystem.open_dungeon_from_town_exit())
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}
    return {"ok": ok}


def action_select_start(world):
    subsystem = _subsystem(world)
    if not subsystem:
        return {"ok": False, "reason": "subsystem_missing"}
    try:
        kind = getattr(unreal.GameXXKNodeKind, "START")
        return {"ok": bool(subsystem.select_dungeon_node(kind))}
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}


def main():
    argv = sys.argv[1:]
    action = argv[0] if argv else ""
    world = _world()
    result = {"ok": False, "action": action}
    if action == "terrain":
        result.update(action_terrain(world))
    elif action == "backdrop":
        result.update(action_backdrop(world))
    elif action == "hand_formation":
        result.update(action_hand_formation(world))
    elif action == "enter_route":
        result.update(action_enter_route(world))
    elif action == "select_start":
        result.update(action_select_start(world))
    else:
        result["reason"] = "unknown_action"
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
