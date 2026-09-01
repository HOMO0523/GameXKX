"""Read-only live PIE proof for the six-partner/six-NPC formation contract."""

from __future__ import annotations

import json

import unreal


def _get(value: object, name: str, default=None):
    if value is None:
        return default
    try:
        return value.get_editor_property(name)
    except Exception:
        return getattr(value, name, default)


def _names(values: object) -> list[str]:
    try:
        return [str(value) for value in values]
    except Exception:
        return []


def probe() -> dict:
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world() if editor else None
    if world is None:
        raise RuntimeError("no active PIE world")
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    workbench = (
        controller.get_desktop_training_workbench_widget_for_test()
        if controller
        else None
    )
    subsystem = workbench.get_mvp_subsystem() if workbench else None
    state = subsystem.get_runtime_state_copy() if subsystem else None
    card_run = _get(state, "card_run")
    roster = _get(card_run, "companion_roster")
    selection = _get(card_run, "party_selection")
    formation = _get(card_run, "ordered_formation")
    companions = list(_get(roster, "permanent_companions", []) or [])
    npc_loadouts = _get(selection, "quest_npc_card_loadouts", {}) or {}
    npc_progressions = _get(selection, "quest_npc_progressions", {}) or {}
    members = list(_get(formation, "members", []) or [])
    report = {
        "ok": world is not None and workbench is not None and subsystem is not None,
        "map": str(world.get_outermost().get_name()),
        "screen": str(_get(state, "screen", "")),
        "permanentCompanionCount": len(companions),
        "permanentCompanionIds": _names(
            _get(companion, "instance_id") for companion in companions
        ),
        "ownedNpcLoadoutCount": len(npc_loadouts),
        "ownedNpcProgressionCount": len(npc_progressions),
        "formationMemberCount": len(members),
        "formationMemberIds": _names(
            _get(member, "member_id") for member in members
        ),
        "workbenchCompanionCandidates": _names(
            workbench.get_companion_character_ids_for_test() if workbench else []
        ),
        "workbenchNpcCandidates": _names(
            workbench.get_npc_character_ids_for_test() if workbench else []
        ),
    }
    report["ok"] = bool(
        report["ok"]
        and report["permanentCompanionCount"] == 6
        and report["ownedNpcLoadoutCount"] == 6
        and report["ownedNpcProgressionCount"] == 6
        and report["formationMemberCount"] == 3
        and len(report["workbenchCompanionCandidates"]) == 6
        and len(report["workbenchNpcCandidates"]) == 6
    )
    if not report["ok"]:
        raise RuntimeError(json.dumps(report, ensure_ascii=False))
    return report


if __name__ == "__main__":
    print(json.dumps(probe(), ensure_ascii=False, indent=2))
