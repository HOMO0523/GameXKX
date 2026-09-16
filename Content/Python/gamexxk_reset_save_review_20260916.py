"""Lifecycle helpers are safe on the current editor; reset probes require an isolated profile."""
import unreal
import sys
import json
from pathlib import Path
action=sys.argv[1]
if action=='quit':
    unreal.SystemLibrary.quit_editor()
elif action=='save':
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if world:
        gi=unreal.GameplayStatics.get_game_instance(world)
        m=next((o for o in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if o.get_outer()==gi),None)
        if m:
            assert m.save_current_game(), 'Could not save gameplay before build'
else:
    assert 'ResetSave-20260916' in unreal.Paths.project_saved_dir(), 'Reset tests only use the dedicated isolated profile'
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    assert world and 'L_DesktopTrainingHUD' in world.get_name()
    gi=unreal.GameplayStatics.get_game_instance(world)
    m=next(o for o in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if o.get_outer()==gi)
    dev=next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer()==gi)
    pc=unreal.GameplayStatics.get_player_controller(world,0)
    host=next(o for o in unreal.ObjectIterator(unreal.GameXXKDesktopTrainingWorkbenchWidget) if o.get_owning_player()==pc)
    def command(name,args=None):
        r=json.loads(dev.execute_json(json.dumps({'command':name,'args':args or {}})))
        assert r['ok'],r
        return r
    if action=='prepare':
        assert m.start_game()
        command('item.give',{'id':'Currency.Gold','quantity':55555})
        command('character.level',{'level':50})
        command('progress.unlock_tasks')
        dev.toggle_panel()
    elif action=='reset':
        command('save.reset')
        assert not dev.is_session_active()
        if dev.is_panel_open(): dev.toggle_panel()
    elif action=='battle':
        command('battle.start',{'stage':'Training.Normal.1-1','encounter':1,'seed':20260916})
    elif action=='reload':
        assert m.continue_game_from_slot('GameXXK_MVP_SaveSlot_1',0)
    elif action=='panel': dev.toggle_panel()
    elif action=='capture':
        out=Path(unreal.Paths.project_dir())/'Saved/ResetSave-20260916'
        assert unreal.GameXXKEditorCaptureAutomationLibrary.capture_live_game_widget(host,str(out/'reset-live.png'),1920,1080)
    elif action!='observe': raise ValueError(action)
    scene=command('snapshot.export')['data']
    out=Path(unreal.Paths.project_dir())/'Saved/ResetSave-20260916'
    (out/(action+'.json')).write_text(json.dumps(scene,ensure_ascii=False,indent=2),encoding='utf8')
    s=scene['state']
    p=s['guideProgress']['teachingChests']
    if action in ('reset','reload'):
        assert s['playerLevel']==1 and s['training']['partyProgressionStep']==0
        assert not s['training'].get('bDevelopmentUnlockAllStages',False)
        assert not s['narrativeProgress']['mainStory'].get('bDevelopmentUnlockAllTasks',False)
        assert p['stage']==1 and not p['bOpened'] and not p['bDismissed']
        assert len(s['training']['ownedChestTokens'])==1
    print(json.dumps({'action':action,'level':s['playerLevel'],'gold':s['playerGold'],'teaching':p['stage'],'record':str(out/(action+'.json'))},ensure_ascii=False))
