import sys,json,unreal
import gamexxk_probe_real_play_flow as base
world=base._get_game_world();pc=base._first_player_controller(world);w=pc.get_desktop_training_workbench_widget_for_test()
mode=sys.argv[1] if len(sys.argv)>1 else 'inspect'
if mode=='open':
    print('open',w.open_backpack())
    tree=unreal.find_object(w,'WidgetTree') or unreal.find_object(w,'DesktopTrainingWorkbenchWidgetTree')
    unreal.find_object(tree,'BottomNavigationButton_1').call_method('HandleClicked')
if mode=='apply':
    print('selected',w.select_formation_candidate_for_test(sys.argv[2]))
    print('applied',w.apply_formation_candidate_for_test())
tree=unreal.find_object(w,'WidgetTree')
if not tree:
    tree=unreal.find_object(w,'DesktopTrainingWorkbenchWidgetTree')
if mode=='picker':unreal.find_object(tree,'FormationCompanionRosterButton').call_method('HandleClicked')
if mode=='click':unreal.find_object(tree,sys.argv[2]).call_method('HandleClicked')
print('widget',w.get_path_name(),'tree',str(tree))
if tree:
    for name in ('FormationApplyButton','FormationCandidateButton_0','FormationPickerLayer'):
        obj=unreal.find_object(tree,name)
        print(name,str(obj),str(obj.get_visibility()) if obj else None,obj.get_is_enabled() if obj else None)
print('formation_props',[n for n in dir(w) if 'formation' in n or 'notice' in n])
