"""Transient probe: make the Formation Master companion the active party member."""

import json
import sys

import unreal


def _world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


def main(argv):
    world = _world()
    pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
    if not pc:
        print(json.dumps({"ok": False, "reason": "pc_missing"}))
        return
    roster = None
    try:
        roster = pc.get_companion_roster_widget_for_test()
    except Exception as exc:
        print(json.dumps({"ok": False, "reason": f"roster_widget_error: {exc}"[:200]}))
        return
    if not roster:
        print(json.dumps({"ok": False, "reason": "roster_missing"}))
        return

    board = None
    try:
        board = pc.get_battle_board_widget_for_test()
    except Exception:
        board = None
    subsystem = board.get_mvp_subsystem() if board else None
    if not subsystem:
        print(json.dumps({"ok": False, "reason": "subsystem_missing"}))
        return

    state = subsystem.get_runtime_state_copy()
    target = None
    try:
        views = subsystem.get_permanent_companion_views()
        for view in views:
            role = str(getattr(view, "role", ""))
            if "FORMATION_MASTER" in role.upper():
                target = str(getattr(view, "instance_id", ""))
                break
    except Exception as exc:
        print(json.dumps({"ok": False, "reason": f"views_error: {exc}"[:200]}))
        return
    if not target:
        print(json.dumps({"ok": False, "reason": "no_formation_master"}))
        return

    steps = {}
    try:
        steps["select"] = bool(roster.select_companion(unreal.Name(target)))
    except Exception as exc:
        steps["select_error"] = str(exc)[:150]
    try:
        steps["set_active"] = bool(roster.set_selected_companion_as_active())
    except Exception as exc:
        steps["set_active_error"] = str(exc)[:150]
    print(json.dumps({"ok": True, "instance_id": target, "steps": steps}, ensure_ascii=False))


if __name__ == "__main__":
    main(sys.argv[1:])
