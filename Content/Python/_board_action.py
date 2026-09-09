import json
import sys

import unreal

sys.path.insert(0, r"D:\UE5 demo\GameXXK\Content\Python")
import gamexxk_probe_real_play_flow as probe  # noqa: E402


def main():
    args = sys.argv[1:]
    world = probe._get_game_world()
    pc = probe._first_player_controller(world)
    board = pc.get_battle_board_widget_for_test()
    out = {}
    if "--discard" in args:
        idx = args.index("--discard") + 1
        card_id = str(args[idx]) if idx < len(args) else ""
        try:
            out["discard_result"] = board.submit_pending_forced_discard(unreal.Name(card_id))
        except Exception as exc:
            out["discard_error"] = str(exc)
    for name in ("is_card_targeting_active", "submit_pending_insight_choice", "cancel_pending_insight_choice"):
        pass
    print(json.dumps(out, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
