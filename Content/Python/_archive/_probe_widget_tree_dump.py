"""Transient debug probe: dump battle board widget tree names."""

import json
import sys

import unreal


def _world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


def main(argv):
    world = _world()
    pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    board = None
    if pc:
        try:
            board = pc.get_battle_board_widget_for_test()
        except Exception as exc:
            print(json.dumps({"ok": False, "reason": f"board_get_error: {exc}"}))
            return
    if not board:
        print(json.dumps({"ok": False, "reason": "board_missing"}))
        return

    def walk(widget, depth, out, max_depth=4):
        if not widget or depth > max_depth:
            return
        try:
            name = widget.get_name()
        except Exception:
            name = "?"
        try:
            cls = widget.get_class().get_name()
        except Exception:
            cls = "?"
        out.append(f"{'  ' * depth}{name} [{cls}]")
        children = None
        try:
            children = widget.get_all_children()
        except Exception:
            children = None
        for child in children or []:
            walk(child, depth + 1, out, max_depth)

    lines = []
    try:
        tree = board.get_editor_property("widget_tree")
        root = tree.get_editor_property("root_widget") if tree else None
        walk(root, 0, lines)
    except Exception as exc:
        lines.append(f"tree_error: {exc}")
    # Also print any direct test seams that might expose the backdrop
    try:
        lines.append("has_backdrop_seam: " + str(hasattr(board, "get_battle_backdrop_resource_path_for_test")))
    except Exception:
        pass
    print(json.dumps({"ok": True, "tree": lines}, ensure_ascii=False))


if __name__ == "__main__":
    main(sys.argv[1:])
