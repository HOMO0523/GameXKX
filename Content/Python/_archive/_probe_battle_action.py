"""Transient probe: drive one battle-board action via BlueprintCallable seams."""

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
    subsystem_type = getattr(unreal, "GameXXKMVPSubsystem", None)
    subsystem = None
    if game_instance and subsystem_type:
        try:
            subsystem = game_instance.get_subsystem(subsystem_type)
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


def _hud_slot(world, unit_id):
    """Read one projected HUD's canvas slot geometry and parent chain."""
    board = _board(world)
    if not board:
        return {"ok": False, "reason": "board_missing"}
    try:
        hud = board.get_projected_unit_hud_for_test(unreal.Name(unit_id))
    except Exception as exc:
        return {"ok": False, "reason": f"hud_lookup_error: {exc}"}
    if not hud:
        return {"ok": False, "reason": "hud_missing"}
    entry = {"hud_path": _name(hud.get_path_name())}
    try:
        res = hud.get_resource_widget_for_test()
        if res:
            entry["text_hex"] = res.get_health_display_text_for_test().encode("utf-8", "replace").hex()
    except Exception as exc:
        entry["text_err"] = str(exc)[:100]
    try:
        slot = hud.slot
        if slot:
            for prop in ("anchors", "position", "size", "alignment"):
                try:
                    value = slot.get_editor_property(prop)
                    entry[prop] = _name(value)
                except Exception as exc:
                    entry[prop + "_err"] = str(exc)[:80]
    except Exception as exc:
        entry["slot_err"] = str(exc)[:100]
    try:
        parent = hud.get_parent()
        entry["parent"] = _name(parent.get_path_name()) if parent else None
    except Exception as exc:
        entry["parent_err"] = str(exc)[:100]
    return {"ok": True, "hud": entry}


def _experiment_set_hp_text(world, unit_id, text):
    """Manual SetText on the unit HUD's HealthText; verifies Slate/UMG sync."""
    board = _board(world)
    if not board:
        return {"ok": False, "reason": "board_missing"}
    try:
        hud = board.get_projected_unit_hud_for_test(unreal.Name(unit_id))
        if not hud:
            return {"ok": False, "reason": "hud_missing"}
        res = hud.get_resource_widget_for_test()
        if not res:
            return {"ok": False, "reason": "resource_missing"}
        changed = []
        tree = res.get_widget_tree()
        if tree:
            def walk(widget, depth):
                if widget is None or depth > 20:
                    return
                try:
                    cls = widget.get_class().get_name()
                    if "TextBlock" in cls:
                        try:
                            current = str(widget.get_text())
                            if "气血" in current:
                                widget.set_text(unreal.Text(text))
                                changed.append(_name(widget.get_name()))
                        except Exception:
                            pass
                    for i in range(widget.get_children_count()):
                        try:
                            walk(widget.get_child_at(i), depth + 1)
                        except Exception:
                            pass
                except Exception:
                    pass
            walk(tree.root_widget, 0)
        return {"ok": True, "changed": changed}
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}


def _walk_tree_hp_texts(world):
    """Walk the board widget tree and collect every HP text block."""
    board = _board(world)
    if not board:
        return {"ok": False, "reason": "board_missing"}
    found = []

    def walk(widget, depth):
        if widget is None or depth > 50:
            return
        try:
            cls = widget.get_class().get_name()
            if "TextBlock" in cls:
                try:
                    text = widget.get_text()
                    if text and "气血" in str(text):
                        found.append(
                            {
                                "depth": depth,
                                "name": _name(widget.get_name()),
                                "text_hex": str(text).encode("utf-8", "replace").hex(),
                            }
                        )
                except Exception:
                    pass
            children = widget.get_children_count()
            for i in range(children):
                try:
                    walk(widget.get_child_at(i), depth + 1)
                except Exception:
                    pass
        except Exception:
            pass

    try:
        tree = board.get_widget_tree()
        if tree:
            walk(tree.root_widget, 0)
        return {"ok": True, "hp_texts": found}
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}


def _hand_ids(world):
    subsystem = _subsystem(world)
    if not subsystem:
        return {"ok": False, "reason": "subsystem_missing"}
    try:
        state = subsystem.get_runtime_state_copy()
    except Exception as exc:
        return {"ok": False, "reason": f"state_error: {exc}"}
    run = _property(state, "card_run", "CardRun")
    battle = _property(run, "active_battle", "ActiveBattle")
    deck = _property(battle, "deck", "Deck")
    hand = _property(deck, "hand", "Hand") or []
    ids = []
    for card in hand:
        ids.append(
            {
                "instance_id": _name(_property(card, "instance_id", "InstanceId")),
                "card_id": _name(_property(card, "card_id", "CardId")),
            }
        )
    return {"ok": True, "hand_ids": ids}


def _start_game(world):
    subsystem = _subsystem(world)
    if not subsystem:
        return {"ok": False, "reason": "subsystem_missing"}
    try:
        return {"ok": bool(subsystem.start_game())}
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}


def _select_route_node(world, node_id):
    subsystem = _subsystem(world)
    if not subsystem:
        return {"ok": False, "reason": "subsystem_missing"}
    try:
        return {"ok": bool(subsystem.select_route_node_by_id(int(node_id))), "node_id": int(node_id)}
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}


def _accept_quest(world):
    subsystem = _subsystem(world)
    if not subsystem:
        return {"ok": False, "reason": "subsystem_missing"}
    try:
        return {"ok": bool(subsystem.accept_quest())}
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}


def _select_battle(world):
    subsystem = _subsystem(world)
    if not subsystem:
        return {"ok": False, "reason": "subsystem_missing"}
    kind_type = getattr(unreal, "GameXXKNodeKind", None)
    if not kind_type:
        return {"ok": False, "reason": "node_kind_type_missing"}
    try:
        kind = getattr(kind_type, "BATTLE")
    except Exception as exc:
        return {"ok": False, "reason": f"node_kind_battle_missing: {exc}"}
    try:
        return {"ok": bool(subsystem.select_dungeon_node(kind))}
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}


def _select_qingshan(world):
    pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    if not pc:
        return {"ok": False, "reason": "pc_missing"}
    try:
        world_map = pc.get_world_map_widget_for_test()
    except Exception as exc:
        return {"ok": False, "reason": f"world_map_missing: {exc}"}
    if not world_map:
        return {"ok": False, "reason": "world_map_missing"}
    try:
        return {"ok": bool(world_map.try_select_region(unreal.Name("Region.Qingshan")))}
    except Exception as exc:
        return {"ok": False, "reason": str(exc)}


def _route_nodes(world):
    pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    if not pc:
        return {"ok": False, "reason": "pc_missing"}
    try:
        route = pc.get_route_map_widget_for_test()
    except Exception as exc:
        return {"ok": False, "reason": f"route_missing: {exc}"}
    if not route:
        return {"ok": False, "reason": "route_map_missing"}
    try:
        states = route.get_route_node_visual_states_for_test()
    except Exception as exc:
        return {"ok": False, "reason": f"route_states_error: {exc}"}
    nodes = []
    for state in states:
        node = {
            "node_id": int(_property(state, "node_id", "NodeId") or -1),
            "command_name": _name(_property(state, "command_name", "CommandName")),
            "enabled": bool(_property(state, "b_enabled", "bEnabled")),
            "label": str(_property(state, "label", "Label") or ""),
            "node_kind": _name(_property(state, "node_kind", "NodeKind")),
        }
        nodes.append(node)
    return {"ok": True, "nodes": nodes}


def main():
    argv = sys.argv[1:]
    action = argv[0] if argv else ""
    arg = argv[1] if len(argv) > 1 else ""
    world = _world()
    board = _board(world)
    result = {"ok": False, "action": action}
    if action == "hand_ids":
        result.update(_hand_ids(world))
    elif action == "start_game":
        result.update(_start_game(world))
    elif action == "select_qingshan":
        result.update(_select_qingshan(world))
    elif action == "accept_quest":
        result.update(_accept_quest(world))
    elif action == "select_battle":
        result.update(_select_battle(world))
    elif action == "refresh_route":
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if not pc:
            result["reason"] = "pc_missing"
        else:
            try:
                route = pc.get_route_map_widget_for_test()
                if not route:
                    result["reason"] = "route_map_missing"
                else:
                    route.refresh_from_state()
                    result["ok"] = True
            except Exception as exc:
                result["reason"] = str(exc)
    elif action == "select_elite":
        subsystem = _subsystem(world)
        if not subsystem:
            result["reason"] = "subsystem_missing"
        else:
            kind_type = getattr(unreal, "GameXXKNodeKind", None)
            if not kind_type:
                result["reason"] = "node_kind_type_missing"
            else:
                try:
                    kind = getattr(kind_type, "ELITE")
                    result["ok"] = bool(subsystem.select_dungeon_node(kind))
                except Exception as exc:
                    result["reason"] = str(exc)
    elif action == "select_route":
        result.update(_select_route_node(world, arg))
    elif action == "route_nodes":
        result.update(_route_nodes(world))
    elif action == "walk_tree":
        result.update(_walk_tree_hp_texts(world))
    elif action == "hud_slot":
        result.update(_hud_slot(world, arg))
    elif action == "set_text":
        unit_id, text = arg.split("|", 1) if "|" in arg else (arg, "气血 777 / 100")
        result.update(_experiment_set_hp_text(world, unit_id, text))
    elif action == "click_card":
        if not board:
            result["reason"] = "board_missing"
        else:
            try:
                result["ok"] = bool(board.click_card_in_hand(unreal.Name(arg)))
            except Exception as exc:
                result["reason"] = str(exc)
    elif action == "confirm":
        if not board:
            result["reason"] = "board_missing"
        else:
            try:
                result["ok"] = bool(board.confirm_targeting_unit(unreal.Name(arg)))
            except Exception as exc:
                result["reason"] = str(exc)
    elif action == "cancel_targeting":
        if not board:
            result["reason"] = "board_missing"
        else:
            try:
                result["ok"] = bool(board.cancel_battle_targeting())
            except Exception as exc:
                result["reason"] = str(exc)
    elif action == "end_phase":
        if not board:
            result["reason"] = "board_missing"
        else:
            try:
                result["ok"] = bool(board.end_card_player_phase())
                result["error_text"] = _name(board.get_last_card_interaction_error_for_test()) if hasattr(board, "get_last_card_interaction_error_for_test") else ""
            except Exception as exc:
                result["reason"] = str(exc)
    elif action == "presentation_idle":
        # The presentation-query test seams are not Blueprint-exposed, so use the
        # hand-card buttons: the interaction lock disables them during a pending
        # presentation and RefreshHandCards re-enables them after the queue drains.
        if not board:
            result["reason"] = "board_missing"
        else:
            try:
                box = board.get_hand_card_box_for_test()
                idle = True
                count = box.get_children_count() if box else 0
                for i in range(count):
                    child = box.get_child_at(i)
                    if child and hasattr(child, "get_is_enabled") and not child.get_is_enabled():
                        idle = False
                result["ok"] = idle
                result["hand_count"] = count
            except Exception as exc:
                result["reason"] = str(exc)
    elif action == "targeting_active":
        if not board:
            result["reason"] = "board_missing"
        else:
            try:
                result["ok"] = bool(board.is_card_targeting_active())
                result["pending"] = _name(board.get_pending_card_instance_id_for_test())
            except Exception as exc:
                result["reason"] = str(exc)
    elif action == "pending_reward":
        # Authoritative reward-offer state: CardRun.PendingReward.Options (3 after
        # ResolveBattleVictory) drives the board's RewardCardBox/SkipRewardButton.
        subsystem = _subsystem(world)
        if not subsystem:
            result["reason"] = "subsystem_missing"
        else:
            try:
                state = subsystem.get_runtime_state_copy()
            except Exception as exc:
                result["reason"] = f"state_error: {exc}"
            else:
                run = _property(state, "card_run", "CardRun")
                reward = _property(run, "pending_reward", "PendingReward")
                ids = _property(reward, "card_ids", "CardIds") or []
                options = _property(reward, "options", "Options") or []
                result["ok"] = len(ids) > 0 or len(options) > 0
                result["card_ids"] = [str(i) for i in ids]
                result["options"] = [
                    {
                        "kind": _name(_property(o, "kind", "Kind")),
                        "card_id": _name(_property(o, "card_id", "CardId")),
                        "relic_id": _name(_property(o, "relic_id", "RelicId")),
                    }
                    for o in options
                ]
    elif action == "choose_reward_option":
        if not board:
            result["reason"] = "board_missing"
        else:
            try:
                result["ok"] = bool(board.choose_pending_battle_reward_option(int(arg), unreal.Name("")))
            except Exception as exc:
                result["reason"] = str(exc)
    elif action == "pending_choice":
        # Deck.PendingChoice blocks all card interaction and EndCardPlayerPhase
        # (e.g. GuanXi's forced discard: "此牌要求弃置 N 张手牌"). Candidates are
        # card-instance views; the owning instance stays in Hand until confirmed.
        subsystem = _subsystem(world)
        if not subsystem:
            result["reason"] = "subsystem_missing"
        else:
            try:
                state = subsystem.get_runtime_state_copy()
            except Exception as exc:
                result["reason"] = f"state_error: {exc}"
            else:
                run = _property(state, "card_run", "CardRun")
                battle = _property(run, "active_battle", "ActiveBattle")
                deck = _property(battle, "deck", "Deck")
                choice = _property(deck, "pending_choice", "PendingChoice")
                kind_value = _property(choice, "kind", "Kind")
                kind = getattr(kind_value, "name", None) or _name(kind_value)
                result["ok"] = choice is not None and kind.upper() not in ("", "NONE", "INVALID")
                result["kind"] = kind
                result["required_count"] = int(_property(choice, "required_count", "RequiredCount") or 0)
                result["required_discard_count"] = int(_property(choice, "required_discard_count", "RequiredDiscardCount") or 0)
                result["b_can_cancel"] = bool(_property(choice, "b_can_cancel", "bCanCancel"))
                candidates = _property(choice, "candidates", "Candidates") or []
                result["candidate_ids"] = [
                    _name(_property(c, "instance_id", "InstanceId")) for c in candidates
                ]
    elif action == "submit_discard":
        if not board:
            result["reason"] = "board_missing"
        else:
            try:
                result["ok"] = bool(board.submit_pending_forced_discard(unreal.Name(arg)))
            except Exception as exc:
                result["reason"] = str(exc)
    elif action == "submit_insight":
        if not board:
            result["reason"] = "board_missing"
        else:
            try:
                result["ok"] = bool(board.submit_pending_insight_choice(unreal.Name(arg)))
            except Exception as exc:
                result["reason"] = str(exc)
    elif action == "submit_task_search":
        if not board:
            result["reason"] = "board_missing"
        else:
            try:
                result["ok"] = bool(board.submit_pending_hero_task_search_choice(unreal.Name(arg)))
            except Exception as exc:
                result["reason"] = str(exc)
    elif action == "cancel_insight":
        if not board:
            result["reason"] = "board_missing"
        else:
            try:
                result["ok"] = bool(board.cancel_pending_insight_choice())
            except Exception as exc:
                result["reason"] = str(exc)
    elif action == "target_highlights":
        # During targeting, IsTargetUnitHighlighted marks every unit that is a
        # legal candidate for the pending card. Lets the capture prefer
        # enemy-targeting attack cards over party-targeting defend/heal cards.
        if not board:
            result["reason"] = "board_missing"
        else:
            subsystem = _subsystem(world)
            if not subsystem:
                result["reason"] = "subsystem_missing"
            else:
                try:
                    state = subsystem.get_runtime_state_copy()
                except Exception as exc:
                    result["reason"] = f"state_error: {exc}"
                else:
                    run = _property(state, "card_run", "CardRun")
                    battle = _property(run, "active_battle", "ActiveBattle")
                    units = _property(battle, "units", "Units") or []
                    highlighted = []
                    for unit in units:
                        uid = _name(_property(unit, "unit_id", "UnitId"))
                        try:
                            if uid and board.is_target_unit_highlighted(unreal.Name(uid)):
                                highlighted.append(uid)
                        except Exception:
                            pass
                    result["ok"] = True
                    result["highlighted_unit_ids"] = highlighted
    elif action == "enter_route":
        subsystem = _subsystem(world)
        if not subsystem:
            result["reason"] = "subsystem_missing"
        else:
            try:
                result["ok"] = bool(subsystem.open_dungeon_from_town_exit())
            except Exception as exc:
                result["reason"] = str(exc)
    elif action == "resolve_victory":
        subsystem = _subsystem(world)
        if not subsystem:
            result["reason"] = "subsystem_missing"
        else:
            try:
                result["ok"] = bool(subsystem.resolve_battle_victory(False))
            except Exception as exc:
                result["reason"] = str(exc)
    elif action == "refresh_board":
        if not board:
            result["reason"] = "board_missing"
        else:
            try:
                board.refresh_from_state()
                result["ok"] = True
            except Exception as exc:
                result["reason"] = str(exc)
    elif action == "battle_state":
        subsystem = _subsystem(world)
        if not subsystem:
            result["reason"] = "subsystem_missing"
        else:
            try:
                state = subsystem.get_runtime_state_copy()
                run = _property(state, "card_run", "CardRun")
                battle = _property(run, "active_battle", "ActiveBattle")
                deck = _property(battle, "deck", "Deck")
                reward = _property(run, "pending_reward", "PendingReward")
                ids = _property(reward, "card_ids", "CardIds") or []
                result["ok"] = True
                result["screen"] = _name(_property(state, "screen", "Screen"))
                result["b_has_active_card_battle"] = bool(_property(run, "b_has_active_card_battle", "bHasActiveCardBattle"))
                result["phase"] = _name(_property(battle, "phase", "Phase"))
                result["terrain"] = _name(_property(battle, "terrain", "Terrain"))
                units = _property(battle, "units", "Units") or []
                result["units"] = [
                    {
                        "unit_id": _name(_property(u, "unit_id", "UnitId")),
                        "side": _name(_property(u, "side", "Side")),
                        "b_living": bool(_property(u, "b_living", "bLiving")),
                    }
                    for u in units
                ]
                result["hand_count"] = len(_property(deck, "hand", "Hand") or [])
                result["pending_reward_card_ids"] = [str(i) for i in ids]
            except Exception as exc:
                result["reason"] = str(exc)
    else:
        result["reason"] = f"unknown_action:{action}"
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
