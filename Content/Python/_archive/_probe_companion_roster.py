"""Transient probe: companion roster roles + active companion in PIE."""

import json
import sys

import unreal


def _world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


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


def main(argv):
    world = _world()
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
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if pc:
            try:
                board = pc.get_battle_board_widget_for_test()
                subsystem = board.get_mvp_subsystem() if board else None
            except Exception:
                subsystem = None
    if not subsystem:
        print(json.dumps({"ok": False, "reason": "subsystem_missing"}))
        return
    state = subsystem.get_runtime_state_copy()
    result = {"ok": True, "screen": str(_property(state, "screen", "Screen"))}
    state = subsystem.get_runtime_state_copy()
    run = _property(state, "card_run", "CardRun")
    roster = _property(run, "companion_roster", "CompanionRoster")
    companions = _property(roster, "permanent_companions", "PermanentCompanions") or []
    rows = []
    for comp in companions:
        rows.append(
            {
                "instance_id": _name(_property(comp, "instance_id", "InstanceId")),
                "role": _name(_property(comp, "role", "Role")),
                "template": _name(_property(comp, "template", "Template")),
                "active": bool(_property(comp, "b_is_active", "bIsActive")),
            }
        )
    pending = _property(roster, "pending_recruitment", "PendingRecruitment")
    pending_row = None
    if pending and bool(_property(pending, "b_has_pending_recruitment", "bHasPendingRecruitment")):
        candidate = _property(pending, "pending_companion", "PendingCompanion")
        pending_row = {
            "role": _name(_property(candidate, "role", "Role")),
            "instance_id": _name(_property(candidate, "instance_id", "InstanceId")),
        }
    print(
        json.dumps(
            {
                "ok": True,
                "screen": str(_property(state, "screen", "Screen")),
                "companions": rows,
                "pending_recruitment": pending_row,
            },
            ensure_ascii=False,
        )
    )


if __name__ == "__main__":
    main(sys.argv[1:])
