#!/usr/bin/env python3
"""Quick board probe (UFUNCTION-exposed only). Args: [--click <instance_id>] [--confirm <unit_id>] [--end-turn]"""
from __future__ import annotations

import json
import sys

import unreal

sys.path.insert(0, r"D:\UE5 demo\GameXXK\Content\Python")
import gamexxk_probe_real_play_flow as probe  # noqa: E402


def _call(obj, name, *args):
    fn = getattr(obj, name, None)
    if fn is None or not callable(fn):
        return None, "missing"
    try:
        return fn(*args), None
    except Exception as exc:
        return None, "err:" + str(exc)


def _board(world):
    pc = probe._first_player_controller(world)
    if not pc:
        return None, "no_pc"
    try:
        board = pc.get_battle_board_widget_for_test()
        if board:
            return board, None
    except Exception as exc:
        return None, str(exc)
    return None, "no_board"


def main() -> None:
    args = sys.argv[1:]
    world = probe._get_game_world()
    board, err = _board(world)
    if not board:
        print(json.dumps({"error": err}, ensure_ascii=False))
        return

    out = {"board": str(board)}
    for name in (
        "is_card_targeting_active",
        "is_targeting_battle_action_for_test",
        "is_battle_presentation_locked_for_test",
        "get_battle_presentation_queue_count_for_test",
        "is_card_targeting_for_test",
    ):
        value, error = _call(board, name)
        out[name] = error if error else value

    for name in ("get_pending_card_instance_id_for_test", "get_targeting_action_name_for_test"):
        value, error = _call(board, name)
        out[name] = error if error else str(value)

    debug_state, debug_error = _call(board, "get_battle_board_debug_state_for_test")
    out["debug_state"] = debug_error if debug_error else str(debug_state)

    slots = []
    for index in range(10):
        btn, error = _call(board, "get_hand_card_button_for_test", index)
        if error or btn is None:
            continue
        slot = {"slot": index}
        for getter in ("get_is_enabled", "is_enabled"):
            enabled, enabled_error = _call(btn, getter)
            if not enabled_error:
                slot["enabled"] = bool(enabled)
                break
            slot["enabled"] = enabled_error
        opacity, opacity_error = _call(btn, "get_render_opacity")
        slot["opacity"] = opacity_error if opacity_error else float(opacity)
        visibility, visibility_error = _call(btn, "get_visibility")
        slot["visibility"] = visibility_error if visibility_error else str(visibility)
        slots.append(slot)
    out["hand_buttons"] = slots

    if "--click" in args:
        idx = args.index("--click") + 1
        card_id = str(args[idx]) if idx < len(args) else ""
        result, error = _call(board, "click_card_in_hand", unreal.Name(card_id))
        out["click_card"] = card_id
        out["click_result"] = error if error else result
        targeting, targeting_error = _call(board, "is_card_targeting_active")
        out["targeting_after_click"] = targeting_error if targeting_error else targeting
        pending, pending_error = _call(board, "get_pending_card_instance_id_for_test")
        out["pending_after_click"] = pending_error if pending_error else str(pending)

    if "--confirm" in args:
        idx = args.index("--confirm") + 1
        unit_id = str(args[idx]) if idx < len(args) else ""
        result, error = _call(board, "confirm_targeting_unit", unreal.Name(unit_id))
        out["confirm_unit"] = unit_id
        out["confirm_result"] = error if error else result
        targeting, targeting_error = _call(board, "is_card_targeting_active")
        out["targeting_after_confirm"] = targeting_error if targeting_error else targeting

    if "--end-turn" in args:
        result, error = _call(board, "end_card_player_phase")
        out["end_turn_result"] = error if error else result

    print(json.dumps(out, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
