import json
import unreal

import gamexxk_probe_real_play_flow as probe

world = probe._get_game_world()
pc = probe._first_player_controller(world)
board = pc.get_battle_board_widget_for_test()
print(json.dumps({"tooltip": str(board.get_card_tooltip_text_for_test())}, ensure_ascii=False))
