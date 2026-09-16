"""Temporary-session review of task returns and actual reward-button presentation."""
import copy
import hashlib
import json
import sys
from pathlib import Path
import unreal

action=sys.argv[1] if len(sys.argv)>1 else 'observe'
if action not in ('observe','debug'):
    assert any(x in unreal.Paths.project_saved_dir() for x in ('FirstBattleFollowup-20260916','StoryRewardFlow-20260916')), 'Mutations require the dedicated review profile'
out=Path(unreal.Paths.project_dir())/'Saved/StoryRewardFlow-20260916'
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
gi=unreal.GameplayStatics.get_game_instance(world)
m=next(o for o in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if o.get_outer()==gi)
dev=next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer()==gi)
pc=unreal.GameplayStatics.get_player_controller(world,0)
host=pc.get_desktop_training_workbench_widget_for_test()
trees={t.get_outer().get_path_name():t for t in unreal.ObjectIterator(unreal.WidgetTree) if t.get_outer()}
roots={}
for widget in unreal.ObjectIterator(unreal.Widget):
    outer=widget.get_outer()
    if isinstance(outer,unreal.WidgetTree) and not widget.get_parent():roots.setdefault(outer.get_path_name(),[]).append(widget)

def command(name,args=None):
    r=json.loads(dev.execute_json(json.dumps({'command':name,'args':args or {}})))
    assert r['ok'],r
    return r.get('data')

def snapshot():return command('snapshot.export')

def walk(w,seen=None):
    if not w:return
    seen=seen if seen is not None else set()
    if w.get_path_name() in seen:return
    seen.add(w.get_path_name());yield w
    if isinstance(w,unreal.UserWidget):
        tree=trees.get(w.get_path_name())
        if tree:
            root=unreal.find_object(tree,'DesktopTrainingOverlayRoot') or unreal.find_object(tree,'MainStoryDesignCanvas')
            if root:yield from walk(root,seen)
            else:
                for child in roots.get(tree.get_path_name(),[]):yield from walk(child,seen)
    elif isinstance(w,unreal.PanelWidget):
        for child in w.get_all_children():yield from walk(child,seen)

def find(name):return next((w for w in walk(host) if w.get_name()==name or w.get_name().startswith(name+'_')),None)

def click(name,method='HandleClicked'):
    w=find(name);assert w,(name,[x.get_name() for x in walk(host) if 'Story' in x.get_name()][:30])
    assert w.get_is_enabled(),name
    w.call_method(method)

if action=='begin':
    assert not dev.is_session_active()
    assert m.save_current_game()
    original=snapshot();(out/'original-scene.json').write_text(json.dumps(original,ensure_ascii=False,indent=2),encoding='utf8')
    save_dir=Path(unreal.Paths.project_saved_dir())/'SaveGames'
    baseline={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in save_dir.glob('*') if p.is_file()}
    (out/'review-save-baseline.json').write_text(json.dumps(baseline,indent=2),encoding='utf8')
    command('session.begin')
    candidate=copy.deepcopy(original)
    task=candidate['state']['narrativeProgress']['taskProgressById']['s00-04']
    task['state']='Completed';task['bRewardCommitted']=False
    command('snapshot.import',{'scene':candidate})
elif action=='rearm':
    assert dev.is_session_active()
    candidate=json.loads((out/'original-scene.json').read_text(encoding='utf8'))
    task=candidate['state']['narrativeProgress']['taskProgressById']['s00-04'];task['state']='Completed';task['bRewardCommitted']=False
    command('snapshot.import',{'scene':candidate})
elif action=='open':
    assert dev.is_session_active()
    host.open_backpack()
    if not find('MainStoryChapter_0'):click('StoryQuestButton')
    click('MainStoryChapter_0')
    back=find('StoryResultBack') or find('StoryBackToTree')
    if back:back.call_method('Clicked')
elif action=='select':
    click('StoryNode_'+sys.argv[2],'Clicked')
elif action=='claim':
    assert dev.is_session_active()
    before=snapshot();click('StoryClaimReward','Clicked');after=snapshot()
    (out/('claim-'+sys.argv[2]+'.json')).write_text(json.dumps({'before':before,'after':after},ensure_ascii=False,indent=2),encoding='utf8')
elif action=='battle-receipt':
    assert dev.is_session_active()
    command('battle.start',{'stage':'Training.Normal.1-1','encounter':6,'seed':916})
    candidate=snapshot();b=candidate['state']['cardRun']['activeBattle']
    for u in b['units']:
        if u['side']=='Enemy':u['hP']=0;u['bLiving']=False
    b['phase']='Victory';command('snapshot.import',{'scene':candidate})
    m.advance_training_challenge_encounter();assert m.has_pending_training_settlement()
    pc.refresh_player_flow_widgets_for_test()
elif action=='confirm':
    assert dev.is_session_active()
    page=next(w for w in unreal.ObjectIterator(unreal.GameXXKTrainingSettlementWidget) if w.get_owning_player()==pc)
    assert page.confirm_for_test()
elif action=='restore':
    assert dev.is_session_active()
    baseline=json.loads((out/'review-save-baseline.json').read_text(encoding='utf8'))
    changed=[p for p,h in baseline.items() if not Path(p).exists() or hashlib.sha256(Path(p).read_bytes()).hexdigest()!=h]
    (out/'review-saves-write-audit.json').write_text(json.dumps({'files':len(baseline),'changed':changed}),encoding='utf8')
    command('session.restore')
    assert m.save_current_game()
elif action=='audit-restored':
    assert not dev.is_session_active()
    for _ in range(3):assert m.save_current_game()
    audit=[]
    for base in ['GameXXK_MVP_SaveSlot_1','GameXXK_DesktopTraining_Checkpoint']:
        for suffix in ['', '.Previous1', '.Previous2', '.Previous3']:
            saved=unreal.GameplayStatics.load_game_from_slot(base+suffix,0)
            assert saved,base+suffix
            state=saved.save_state.runtime_state
            task_states={str(k).lower():str(v.state) for k,v in state.narrative_progress.task_progress_by_id.items()}
            audit.append({'slot':base+suffix,'gold':state.player_gold,'chests':len(state.training.owned_chest_tokens),'task04':task_states.get('s00-04'),'task07':task_states.get('s00-07')})
    (out/'restored-save-audit.json').write_text(json.dumps(audit,indent=2),encoding='utf8')
    print(json.dumps(audit))
elif action=='debug':
    print('HOST',host.get_path_name())
    for t in unreal.ObjectIterator(unreal.WidgetTree):
        if t.get_outer()==host:print('TREE',t.get_path_name())
    print('WALK',[x.get_name() for x in walk(host) if 'Story' in x.get_name() or 'Quest' in x.get_name()])
elif action!='observe':raise ValueError(action)

scene=snapshot();s=scene['state'];f=find('RewardFlightEffects')
info={'action':action,'screen':s['screen'],'gold':s['playerGold'],'chests':len(s['training']['ownedChestTokens']),
      'story':{k:s['narrativeProgress']['mainStory'].get(k) for k in ('activeNodeId','journeyNodeId','phase','bBattleWon')},
      'flightCount':f.get_flight_count_for_test() if f else 0,'flightEvents':f.get_reward_event_count_for_test() if f else 0,
      'session':dev.is_session_active(),'error':str(m.get_last_save_load_error())}
(out/('live-'+action+'.json')).write_text(json.dumps({'summary':info,'scene':scene},ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(info,ensure_ascii=False))
