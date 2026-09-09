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
    out["targeting"] = bool(board.is_card_targeting_active())

    p1 = unreal.Vector2D(300.0, 400.0)
    p2 = unreal.Vector2D(340.0, 400.0)
    try:
        board.update_targeting_pointer_from_slate_absolute_position(p1)
        out["stored_p1"] = _vec(board.get_targeting_pointer_position_for_test())
        board.update_targeting_pointer_from_slate_absolute_position(p2)
        out["stored_p2"] = _vec(board.get_targeting_pointer_position_for_test())
    except Exception as exc:
        out["error"] = str(exc)
    print(json.dumps(out, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
