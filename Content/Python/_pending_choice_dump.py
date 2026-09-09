import json
import sys

import unreal

sys.path.insert(0, r"D:\UE5 demo\GameXXK\Content\Python")
import gamexxk_probe_real_play_flow as probe  # noqa: E402


def _struct_get(value, *names):
    return probe._struct_get(value, *names)


def main():
    world = probe._get_game_world()
    pc = probe._first_player_controller(world)
    board = pc.get_battle_board_widget_for_test()
    subsystem = board.get_mvp_subsystem()
    raw = subsystem.get_runtime_state_copy()
    card_run = _struct_get(raw, "card_run", "CardRun")
    active = _struct_get(card_run, "active_battle", "ActiveBattle")
    deck = _struct_get(active, "deck", "Deck")
    pending = _struct_get(deck, "pending_choice", "PendingChoice")
    out = {"phase": probe._enum_name(_struct_get(active, "phase", "Phase"))}
    if pending is None:
        out["pending_choice"] = None
    else:
        candidates = _struct_get(pending, "candidates", "Candidates")
        cand_list = []
        for candidate in list(candidates or []):
            cand_list.append({
                "instance_id": str(_struct_get(candidate, "instance_id", "InstanceId") or ""),
                "card_id": str(_struct_get(candidate, "card_id", "CardId") or ""),
                "owner": str(_struct_get(candidate, "owner_unit_id", "OwnerUnitId") or ""),
            })
        out["pending_choice"] = {
            "kind": probe._enum_name(_struct_get(pending, "kind", "Kind")),
            "required_count": _struct_get(pending, "required_count", "RequiredCount"),
            "required_discard_count": _struct_get(pending, "required_discard_count", "RequiredDiscardCount"),
            "b_can_cancel": bool(_struct_get(pending, "b_can_cancel", "bCanCancel", "CanCancel")),
            "candidates": cand_list,
        }
    hand = _struct_get(deck, "hand", "Hand")
    out["hand"] = [str(_struct_get(c, "instance_id", "InstanceId") or "") for c in list(hand or [])]
    draw_pile = _struct_get(deck, "draw_pile", "DrawPile")
    discard_pile = _struct_get(deck, "discard_pile", "DiscardPile")
    out["draw_pile_count"] = len(list(draw_pile or []))
    out["discard_pile_count"] = len(list(discard_pile or []))
    print(json.dumps(out, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
