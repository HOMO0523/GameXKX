"""Transient probe: diagnose why the tiered reward row stays collapsed in PIE."""

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


def main():
    world = _world()
    board = _board(world)
    result = {"ok": False}
    if not board:
        print(json.dumps({"ok": False, "reason": "board_missing"}, ensure_ascii=False))
        return

    def seam(name):
        try:
            fn = getattr(board, name)
            value = fn() if callable(fn) else fn
            return str(value)
        except Exception as exc:
            return f"ERR:{exc}"

    result["board_path"] = str(board.get_path_name()) if hasattr(board, "get_path_name") else "?"
    result["has_pending_reward"] = seam("has_pending_route_reward")
    result["pending_card_ids"] = seam("get_pending_route_reward_card_ids")
    result["presentation_locked"] = seam("is_battle_presentation_locked_for_test")
    try:
        board.refresh_from_state()
        result["refresh_ok"] = True
    except Exception as exc:
        result["refresh_ok"] = f"ERR:{exc}"
    result["pending_card_ids_after"] = seam("get_pending_route_reward_card_ids")
    result["has_pending_after"] = seam("has_pending_route_reward")
    # Walk reward box visibility after refresh.
    found = []
    root = None
    try:
        root = board.get_hand_card_box_for_test()
        for _ in range(12):
            parent = root.get_parent()
            if parent is None:
                break
            root = parent
    except Exception:
        pass
    if root is not None:
        def walk(widget, depth):
            if widget is None or depth > 10:
                return
            try:
                name = str(widget.get_name())
                if name in ("BattleRewardCardBox", "BattleHandCardBox", "BattleSkipRewardButton"):
                    found.append({"name": name, "visibility": str(widget.get_visibility())})
            except Exception:
                pass
            try:
                for i in range(widget.get_children_count()):
                    walk(widget.get_child_at(i), depth + 1)
            except Exception:
                pass
        walk(root, 0)
    result["visibility_after_refresh"] = found
    result["ok"] = True
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
