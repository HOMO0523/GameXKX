"""Transient probe: inspect/clear the battle HUD fixture in PIE."""

import json
import sys
import unreal


def _world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


def main():
    world = _world()
    pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    board = pc.get_battle_board_widget_for_test() if pc else None
    result = {"ok": False}
    if not board:
        result["reason"] = "board_missing"
        print(json.dumps(result, ensure_ascii=False))
        return
    subsystem = board.get_mvp_subsystem()
    if not subsystem:
        result["reason"] = "subsystem_missing"
        print(json.dumps(result, ensure_ascii=False))
        return
    result["fixture_active"] = bool(subsystem.is_battle_hud_fixture_active_for_test())
    result["target_outcome_active"] = bool(subsystem.is_target_outcome_fixture_active_for_test())
    if len(sys.argv) > 1 and sys.argv[1] == "--clear":
        try:
            subsystem.clear_battle_hud_fixture_for_test()
            result["cleared"] = True
        except Exception as exc:
            result["cleared"] = f"ERR:{exc}"
        result["fixture_active_after"] = bool(subsystem.is_battle_hud_fixture_active_for_test())
    result["ok"] = True
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
