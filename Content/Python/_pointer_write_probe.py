import json
import sys

import unreal

sys.path.insert(0, r"D:\UE5 demo\GameXXK\Content\Python")
import gamexxk_probe_real_play_flow as probe  # noqa: E402


def _vec(value):
    return [float(value.x), float(value.y)]


def main():
    world = probe._get_game_world()
    pc = probe._first_player_controller(world)
    board = pc.get_battle_board_widget_for_test()
    out = {}
    try:
        board.update_targeting_pointer_from_slate_absolute_position(unreal.Vector2D(500.0, 500.0))
        out["pointer_after_direct_set"] = _vec(board.get_targeting_pointer_position_for_test())
        out["debug"] = str(board.get_battle_board_debug_state_for_test())
    except Exception as exc:
        out["error"] = str(exc)
    print(json.dumps(out, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
