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
        out["source"] = _vec(board.get_targeting_source_position_for_test())
    except Exception as exc:
        out["source"] = "err:" + str(exc)

    p1 = unreal.Vector2D(200.0, 550.0)
    p2 = unreal.Vector2D(240.0, 550.0)
    try:
        board.update_targeting_pointer_from_slate_absolute_position(p1)
        out["pointer_after_p1"] = _vec(board.get_targeting_pointer_position_for_test())
        out["preview_visible_after_p1"] = bool(board.is_card_outcome_preview_visible_for_test())
        out["preview_target_after_p1"] = str(board.get_card_outcome_preview_target_unit_id_for_test())
    except Exception as exc:
        out["p1_error"] = str(exc)
    try:
        board.update_targeting_pointer_from_slate_absolute_position(p2)
        out["pointer_after_p2"] = _vec(board.get_targeting_pointer_position_for_test())
        out["preview_visible_after_p2"] = bool(board.is_card_outcome_preview_visible_for_test())
    except Exception as exc:
        out["p2_error"] = str(exc)

    print(json.dumps(out, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
