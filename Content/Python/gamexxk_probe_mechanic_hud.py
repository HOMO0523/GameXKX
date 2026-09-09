"""Read the live mechanic HUD without advancing combat."""
import json,sys,unreal
import gamexxk_probe_real_play_flow as base
world=base._get_game_world()
pc=base._first_player_controller(world) if world else None
board=pc.get_battle_board_widget_for_test() if pc else None
if not board:raise RuntimeError('A desktop training battle must be visible')
rows=[]
for unit_id in sys.argv[1:]:
 hud=board.get_projected_unit_hud_for_test(unit_id)
 mechanic=hud.get_mechanics_widget_for_test() if hud else None
 if not mechanic:
  rows.append({'unit':unit_id,'missing':True});continue
 count=mechanic.get_task_card_count_for_test();formulas=mechanic.get_formula_count_for_test()
 rows.append({'unit':unit_id,'task_count':count,'element':str(mechanic.get_task_element_for_test()),
  'orders':[mechanic.get_task_card_order_for_test(i) for i in range(count)],
  'task_tooltip':str(mechanic.get_task_tooltip_for_test()),'formula_count':formulas,
  'formula_tooltips':[str(mechanic.get_formula_tooltip_for_test(i)) for i in range(formulas)]})
print(json.dumps({'rows':rows,'board':board.get_battle_board_debug_state_for_test()},ensure_ascii=False))
