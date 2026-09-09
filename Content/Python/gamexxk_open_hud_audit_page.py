import sys,unreal
import gamexxk_probe_real_play_flow as base
w=base._first_player_controller(base._get_game_world()).get_desktop_training_workbench_widget_for_test()
mode=sys.argv[1]
if mode=='backpack':w.open_backpack()
else:
    tree=unreal.find_object(w,'WidgetTree') or unreal.find_object(w,'DesktopTrainingWorkbenchWidgetTree')
    button=unreal.find_object(tree,mode)
    if not button:raise RuntimeError('Missing visible control: '+mode)
    button.call_method('HandleClicked')
print(mode)
