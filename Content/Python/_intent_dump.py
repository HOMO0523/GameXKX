import json
import unreal

import gamexxk_probe_real_play_flow as probe

world = probe._get_game_world()
pc = probe._first_player_controller(world)
board = pc.get_battle_board_widget_for_test()
out = {}
for index in range(3):
    try:
        out[str(index)] = str(board.get_enemy_intent_slot_label_for_test(index))
    except Exception as exc:
        out[str(index)] = "err:" + str(exc)[:60]
print(json.dumps(out, ensure_ascii=False))
