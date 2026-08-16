"""Transient debug probe: battle board widget presence/visibility in PIE."""

import json
import sys

import unreal


def _world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


def main(argv):
    world = _world()
    pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    result = {"ok": True}
    if not pc:
        print(json.dumps({"ok": False, "reason": "pc_missing"}))
        return
    try:
        board = pc.get_battle_board_widget_for_test()
    except Exception as exc:
        result["board_error"] = str(exc)[:200]
        board = None
    result["board_exists"] = board is not None
    if board:
        for name in ("is_in_viewport", "get_visibility"):
            try:
                result[name] = str(getattr(board, name)())
            except Exception as exc:
                result[name + "_err"] = str(exc)[:120]
        try:
            children = board.get_all_children()
            result["direct_children"] = [c.get_name() for c in children][:20]
        except Exception as exc:
            result["children_err"] = str(exc)[:200]
        try:
            result["path"] = str(board.get_path_name())
        except Exception:
            pass
    try:
        state_probe = unreal.GameXXKMVPSubsystem
    except Exception:
        state_probe = None
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main(sys.argv[1:])
