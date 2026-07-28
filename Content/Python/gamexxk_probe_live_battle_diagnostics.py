"""Read-only diagnostics for an already-running GameXXK battle PIE session.

This helper intentionally does not invoke gameplay actions, save packages, or
modify runtime state.  It exists to expose enough reflected state to diagnose
a rejected UMG end-turn call without stopping the player's current PIE run.
"""

from __future__ import annotations

import json

import unreal


def _call(target, name, *args):
    if target is None:
        return {"ok": False, "error": "target_missing"}
    try:
        return {"ok": True, "value": getattr(target, name)(*args)}
    except Exception as error:  # Unreal Python exposes reflection failures as exceptions.
        return {"ok": False, "error": str(error)}


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


def _enum(value):
    try:
        return str(value.name)
    except Exception:
        return _name(value)


def _safe(value):
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, (list, tuple)):
        return [_safe(item) for item in value]
    try:
        return str(value)
    except Exception:
        return repr(value)


def _state_summary(state):
    run = _property(state, "card_run", "CardRun")
    battle = _property(run, "active_battle", "ActiveBattle")
    deck = _property(battle, "deck", "Deck")
    pending_choice = _property(deck, "pending_choice", "PendingChoice")
    candidates = _property(pending_choice, "candidates", "Candidates") or []
    return {
        "screen": _enum(_property(state, "screen", "Screen")),
        "has_active_card_battle": bool(_property(run, "b_has_active_card_battle", "bHasActiveCardBattle")),
        "phase": _enum(_property(battle, "phase", "Phase")),
        "round": int(_property(battle, "round_number", "RoundNumber") or 0),
        "deck": {
            "shared_energy": int(_property(deck, "shared_energy", "SharedEnergy") or 0),
            "hand_count": len(_property(deck, "hand", "Hand") or []),
            "draw_count": len(_property(deck, "draw_pile", "DrawPile") or []),
            "discard_count": len(_property(deck, "discard_pile", "DiscardPile") or []),
            "exhaust_count": len(_property(deck, "exhaust_pile", "ExhaustPile") or []),
            "pending_choice_kind": _enum(_property(pending_choice, "kind", "Kind")),
            "pending_choice_required_count": int(_property(pending_choice, "required_count", "RequiredCount") or 0),
            "pending_choice_can_cancel": bool(_property(pending_choice, "b_can_cancel", "bCanCancel")),
            "pending_choice_candidates": [
                {
                    "instance_id": _name(_property(candidate, "instance_id", "InstanceId")),
                    "card_id": _name(_property(candidate, "card_id", "CardId")),
                    "owner_unit_id": _name(_property(candidate, "owner_unit_id", "OwnerUnitId")),
                }
                for candidate in candidates
            ],
        },
    }


def main():
    result = {"ok": False}
    try:
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = editor.get_game_world() if editor else None
        controller = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        board_result = _call(controller, "get_battle_board_widget_for_test")
        board = board_result.get("value") if board_result.get("ok") else None
        try:
            game_instance = world.get_game_instance() if world else None
        except Exception:
            game_instance = unreal.GameplayStatics.get_game_instance(world) if world else None
        subsystem_type = getattr(unreal, "GameXXKMVPSubsystem", None)
        try:
            subsystem = game_instance.get_subsystem(subsystem_type) if game_instance and subsystem_type else None
        except Exception:
            subsystem_result = _call(board, "get_mvp_subsystem")
            subsystem = subsystem_result.get("value") if subsystem_result.get("ok") else None
        state_result = _call(subsystem, "get_runtime_state_copy")
        state = state_result.get("value") if state_result.get("ok") else None
        result = {
            "ok": world is not None and controller is not None and board is not None and state is not None,
            "board_lookup": {key: _safe(value) for key, value in board_result.items() if key != "value"},
            "state_lookup": {key: _safe(value) for key, value in state_result.items() if key != "value"},
            "state": _state_summary(state),
            "board_calls": {
                "is_card_targeting_active": {key: _safe(value) for key, value in _call(board, "is_card_targeting_active").items()},
                "is_battle_board_visible": {key: _safe(value) for key, value in _call(board, "is_battle_board_visible").items()},
                "get_battle_status_text_for_test": {key: _safe(value) for key, value in _call(board, "get_battle_status_text_for_test").items()},
                "end_card_player_phase_callable": hasattr(board, "end_card_player_phase"),
            },
        }
    except Exception as error:
        result = {"ok": False, "error": str(error)}
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
