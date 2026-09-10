import json
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pc=unreal.GameplayStatics.get_player_controller(world,0) if world else None
records=[]
for w in unreal.ObjectIterator(unreal.GameXXKDesktopTrainingWorkbenchWidget):
    try:
        m=w.get_mvp_subsystem();s=m.get_runtime_state_copy() if m else None
        records.append({'widget':w.get_path_name(),'player':w.get_owning_player().get_path_name() if w.get_owning_player() else None,
            'visible':w.is_visible(),'tool':w.is_tools_panel_active_for_test(),'mvp':m.get_path_name() if m else None,'gold':s.player_gold if s else None})
    except Exception as error:records.append({'error':str(error)})
print(json.dumps({'world':world.get_path_name() if world else None,'pc':pc.get_path_name() if pc else None,'widgets':records},ensure_ascii=True))
