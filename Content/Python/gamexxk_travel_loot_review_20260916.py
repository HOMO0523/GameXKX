"""Read/operate only this task's isolated review profile on the canonical map."""
import json
from pathlib import Path
import sys
import unreal
import builtins

assert 'TravelLoot-20260916' in unreal.Paths.project_saved_dir()
action=sys.argv[1] if len(sys.argv)>1 else 'observe'
if action=='quit':
    unreal.SystemLibrary.quit_editor()
    raise SystemExit(0)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
instance=unreal.GameplayStatics.get_game_instance(world)
pc=unreal.GameplayStatics.get_player_controller(world,0)
mvp=next(o for o in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if o.get_outer()==instance)
host=next(o for o in unreal.ObjectIterator(unreal.GameXXKDesktopTrainingWorkbenchWidget) if o.get_owning_player()==pc)
dev=next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer()==instance)
if action=='start-game': mvp.start_game()
elif action=='open': host.open_backpack()
elif action=='save': assert mvp.save_current_game()
elif action=='capture':
    out=Path(unreal.Paths.project_dir())/'Saved/TravelLoot-20260916'
    label=sys.argv[2] if len(sys.argv)>2 else 'live'
    assert label.replace('-','').isalnum()
    assert unreal.GameXXKEditorCaptureAutomationLibrary.capture_live_game_widget(host,str(out/(label+'.png')),1920,1080)
elif action in ('capture-death','capture-chest'):
    if hasattr(builtins,'travel_loot_capture'):
        unreal.unregister_slate_post_tick_callback(builtins.travel_loot_capture)
    def totals():
        s=mvp.get_runtime_state_copy()
        return s.player_gold, sum(mvp.get_training_chest_count(t) for t in (unreal.GameXXKTrainingRewardTier.NORMAL_CHEST,unreal.GameXXKTrainingRewardTier.ADVANCED_CHEST,unreal.GameXXKTrainingRewardTier.HUNT_CHEST))
    tracker={'active':False,'age':0.0,'frame':0,'wait':0.0,'totals':totals(),'paid_chest':False}
    out=Path(unreal.Paths.project_dir())/'Saved/TravelLoot-20260916'
    def capture_tick(delta):
        tracker['wait']+=delta
        current=totals()
        if current[0]>tracker['totals'][0] and current[1]>tracker['totals'][1]: tracker['paid_chest']=True
        tracker['totals']=current
        if not tracker['active'] and (action=='capture-death' or tracker['paid_chest']) and host.get_travel_visual_phase_name_for_test()=='EnemyDeath':
            tracker['active']=True
        if tracker['active']:
            tracker['age']+=delta
            times=[0.10,0.40,0.70,1.10]
            i=tracker['frame']
            if i<len(times) and tracker['age']>=times[i]:
                prefix='chest' if action=='capture-chest' else 'death-final'
                unreal.GameXXKEditorCaptureAutomationLibrary.capture_live_game_widget(host,str(out/(prefix+'-'+str(i)+'.png')),1920,1080)
                tracker['frame']+=1
        if tracker['frame']>=4 or tracker['wait']>90:
            unreal.unregister_slate_post_tick_callback(builtins.travel_loot_capture)
            del builtins.travel_loot_capture
            (out/'capture-timing.json').write_text(json.dumps(tracker),encoding='utf8')
    builtins.travel_loot_capture=unreal.register_slate_post_tick_callback(capture_tick)
elif action!='observe': raise ValueError(action)
scene=json.loads(dev.execute_json(json.dumps({'command':'snapshot.export','args':{}})))
assert scene['ok'],scene
out=Path(unreal.Paths.project_dir())/'Saved/TravelLoot-20260916'
(out/'review-state.json').write_text(json.dumps(scene['data'],ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps({'action':action,'phase':host.get_travel_visual_phase_name_for_test(),'state_file':str(out/'review-state.json')},ensure_ascii=False))
