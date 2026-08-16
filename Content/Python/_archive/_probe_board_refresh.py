"""Transient probe: force the battle board's own BlueprintCallable RefreshFromState."""

import json
import sys

import unreal


def main(argv):
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world() if editor else None
    pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    if not pc:
        print(json.dumps({"ok": False, "reason": "pc_missing"}))
        return
    try:
        board = pc.get_battle_board_widget_for_test()
        board.refresh_from_state()
        visibility = str(board.get_visibility())
    except Exception as exc:
        print(json.dumps({"ok": False, "reason": str(exc)[:200]}))
        return
    print(json.dumps({"ok": True, "visibility": visibility}, ensure_ascii=False))


if __name__ == "__main__":
    main(sys.argv[1:])
