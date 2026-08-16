"""Transient probe: dump reward-row geometry + runtime reward state in PIE."""

import json
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
    board = _board(world)
    if board:
        try:
            return board.get_mvp_subsystem()
        except Exception:
            pass
    game_instance = None
    try:
        game_instance = world.get_game_instance() if world else None
    except Exception:
        game_instance = unreal.GameplayStatics.get_game_instance(world) if world else None
    subsystem_type = getattr(unreal, "GameXXKMVPSubsystem", None)
    if game_instance and subsystem_type:
        try:
            return game_instance.get_subsystem(subsystem_type)
        except Exception:
            return None
    return None


def _prop(target, *names):
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


def _slot_info(widget):
    """Read canvas-slot anchors/offsets/alignment or hbox padding if available."""
    info = {}
    try:
        slot = widget.slot
    except Exception:
        return info
    if not slot:
        return info
    for key in ("anchors", "offsets", "alignment", "padding", "z_order", "size"):
        try:
            value = slot.get_editor_property(key)
            info[key] = str(value)
        except Exception:
            pass
    return info


def _button_brush(widget):
    """Best-effort read of the button background brush resource name."""
    try:
        bg = widget.get_editor_property("background")
    except Exception:
        return None
    res = _prop(bg, "resource", "Resource")
    if res:
        try:
            return str(res.get_name())
        except Exception:
            return str(res)
    return None


def main():
    world = _world()
    result = {"ok": False}
    if not world:
        print(json.dumps({"ok": False, "reason": "no_pie_world"}, ensure_ascii=False))
        return
    board = _board(world)
    result["board"] = bool(board)
    subsystem = _subsystem(world)
    if subsystem:
        try:
            state = subsystem.get_runtime_state_copy()
            run = _prop(state, "card_run", "CardRun")
            battle = _prop(run, "active_battle", "ActiveBattle")
            deck = _prop(battle, "deck", "Deck")
            reward = _prop(run, "pending_reward", "PendingReward")
            result["screen"] = str(_prop(state, "screen", "Screen"))
            result["hand_count"] = len(_prop(deck, "hand", "Hand") or [])
            ids = _prop(reward, "card_ids", "CardIds") or []
            result["pending_reward_card_ids"] = [str(i) for i in ids]
        except Exception as exc:
            result["state_error"] = str(exc)
    if not board:
        print(json.dumps(result, ensure_ascii=False))
        return
    found = []
    # UUserWidget exposes no widget tree in editor Python; climb from a known
    # panel seam (the hand card box) to the board's root canvas, then walk down
    # through panel widgets (get_child_at / get_children_count).
    root = None
    try:
        root = board.get_hand_card_box_for_test()
        for _ in range(12):
            parent = root.get_parent()
            if parent is None:
                break
            root = parent
    except Exception as exc:
        result["tree_error"] = f"root_climb_failed: {exc}"
        print(json.dumps(result, ensure_ascii=False))
        return
    if root is None:
        result["tree_error"] = "no_root"
        print(json.dumps(result, ensure_ascii=False))
        return

    def walk(widget, depth):
        if widget is None or depth > 10:
            return
        try:
            name = str(widget.get_name())
            cls = str(widget.get_class().get_name())
            if any(tok in name for tok in ("Reward", "SkipReward", "HandCard")):
                entry = {"name": name, "class": cls, "depth": depth}
                try:
                    entry["visibility"] = str(widget.get_visibility())
                except Exception:
                    pass
                entry["slot"] = _slot_info(widget)
                if "SizeBox" in cls:
                    for key in ("width_override", "height_override"):
                        try:
                            entry[key] = float(widget.get_editor_property(key))
                        except Exception:
                            pass
                if "Button" in cls:
                    entry["brush"] = _button_brush(widget)
                found.append(entry)
        except Exception:
            pass
        try:
            for i in range(widget.get_children_count()):
                walk(widget.get_child_at(i), depth + 1)
        except Exception:
            pass

    walk(root, 0)
    result["found"] = found
    result["ok"] = True
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
