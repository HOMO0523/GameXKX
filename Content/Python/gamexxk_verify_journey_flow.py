"""Bounded UI phases for departure/battle verification without changing player progress."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import unreal

root=Path(__file__).resolve().parents[2];out=root/'Saved/StorySystem/JourneyFlowFix'
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
instance=unreal.GameplayStatics.get_game_instance(world)
pc=unreal.GameplayStatics.get_player_controller(world,0)
wb=pc.get_desktop_training_workbench_widget_for_test()
mvp=next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer()==instance)
story=next(x for x in unreal.ObjectIterator(unreal.GameXXKMainStorySubsystem) if x.get_outer()==instance)
dev=next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer()==instance)
mode=sys.argv[1] if len(sys.argv)>1 else 'observe'
def command(name,args=None):
    r=json.loads(dev.execute_json(json.dumps({'schema':1,'command':name,'args':args or {}})))
    assert r['ok'],r
    return r
if mode=='prepare':
    assert not dev.is_session_active()
    before=command('snapshot.export')
    assert not mvp.get_runtime_state_copy().training.challenge_active
    (out/'ui-player-baseline.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
    hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'Saved/SaveGames').glob('*.sav')}
    (out/'ui-save-hashes.json').write_text(json.dumps(hashes,indent=2),encoding='utf-8')
    scene=copy.deepcopy(before['data']);state=scene['state'];n=state['narrativeProgress']
    tasks=n['taskProgressById'];task=tasks[next(k for k in tasks if k.lower()=='s00-04')]
    counts=task['objectiveCounts'];key=next((k for k in counts if k.lower()=='mainstory.dialogueline'),'MainStory.DialogueLine');counts[key]=0
    n['mainStory']['activeNodeId']='None';n['mainStory']['phase']='None';n['mainStory']['lineIndex']=0
    command('snapshot.import',{'scene':scene});assert dev.is_session_active()
    wb.open_backpack();wb.handle_desktop_action_for_test(2100)
elif mode=='scroll':
    assert dev.is_session_active()
    for scroll in unreal.ObjectIterator(unreal.ScrollBox):
        if scroll.get_name().startswith('MainStoryTreeScroll') and scroll.get_parent():
            scroll.set_scroll_offset(float(sys.argv[2]))
elif mode=='select':
    assert dev.is_session_active()
    node_id=int(sys.argv[2])
    route=next(x for x in unreal.ObjectIterator(unreal.GameXXKOneGameRouteMapWidget) if x.get_owning_player()==pc and x.is_in_viewport())
    assert route.select_route_node_with_feedback(node_id)
elif mode=='choice':
    assert dev.is_session_active()
    assert mvp.resolve_route_encounter_choice(int(sys.argv[2]))
    pc.refresh_player_flow_widgets_from_state()
elif mode=='restore':
    result=command('session.restore');assert not dev.is_session_active()
    before=json.loads((out/'ui-player-baseline.json').read_text(encoding='utf-8'))
    after=command('snapshot.export')
    hashes=json.loads((out/'ui-save-hashes.json').read_text(encoding='utf-8'))
    report={'restored':after['data']['state']==before['data']['state'],
        'save_files_unchanged':all(Path(p).exists() and hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items()),
        'dev_session':False}
    (out/'ui-restoration.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    assert report['restored'] and report['save_files_unchanged'],report
state=mvp.get_runtime_state_copy();progress=json.loads(story.get_progress_json());progress.pop('nodes',None)
data={'dev_session':dev.is_session_active(),'world':world.get_name(),'screen':str(state.screen),
      'challenge_active':state.training.challenge_active,'card_battle':state.card_run.has_active_card_battle,
      'story':progress,'line_index':state.narrative_progress.main_story.line_index,
      'gold':state.player_gold,'reachable':list(state.reachable_route_node_ids),'visited':list(state.visited_route_node_ids),
      'route':[{'id':n.node_id,'layer':n.layer_index,'kind':str(n.node_kind),'out':list(n.outgoing_node_ids)} for n in state.route_map_nodes]}
if state.card_run.has_active_card_battle:
    data['battle_source']=state.card_run.active_battle_source_node_id
    data['battle_phase']=str(state.card_run.active_battle.phase)
    data['units']=[{'id':str(u.unit_id),'hp':u.hp,'max_hp':u.max_hp} for u in state.card_run.active_battle.units]
(out/('ui-'+mode+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(data,ensure_ascii=False))
