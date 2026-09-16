"""Read current challenge state; write evidence without changing player progress."""
import json
from pathlib import Path
import unreal
import sys
action=sys.argv[1] if len(sys.argv)>1 else 'observe'
out=Path(unreal.Paths.project_dir())/'Saved/ChallengeFlow-20260916'
if action=='quit':
    unreal.SystemLibrary.quit_editor()
    raise SystemExit(0)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world
gi=unreal.GameplayStatics.get_game_instance(world)
mvp=next(o for o in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if o.get_outer()==gi)
dev=next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer()==gi)
pc=unreal.GameplayStatics.get_player_controller(world,0)
def command(name,args=None):
    result=json.loads(dev.execute_json(json.dumps({'command':name,'args':args or {}})))
    assert result['ok'],result
    return result
if action not in ('observe','repair-current'):
    assert 'ChallengeFlow-20260916' in unreal.Paths.project_saved_dir(), 'Mutation probes require isolated profile'
if action=='repair-current':
    current=command('snapshot.export')['data']
    state=current['state']
    if not dev.is_session_active() and state['training']['bChallengeActive'] and state['training']['activeChallengeStageId']=='Training.Normal.1-1' and state['cardRun']['activeBattle']['phase']=='Victory':
        (out/'before-user-recovery.json').write_text(json.dumps(current,ensure_ascii=False,indent=2),encoding='utf8')
        mvp.advance_training_challenge_encounter()
        assert mvp.has_pending_training_settlement(),str(mvp.get_last_save_load_error())
        pc.refresh_player_flow_widgets_for_test()
    else:
        print('Current player state has changed; no recovery mutation performed.')
elif action=='replay-victory':
    command('snapshot.import',{'scene':json.loads((out/'observed-user.json').read_text(encoding='utf8'))})
    mvp.advance_training_challenge_encounter()
    assert mvp.has_pending_training_settlement(), str(mvp.get_last_save_load_error())
    pc.refresh_player_flow_widgets_for_test()
elif action=='prepare-merchant':
    if dev.is_session_active():command('session.restore')
    assert mvp.start_game()
    assert mvp.start_training_challenge('Training.Normal.1-1')
    scene=command('snapshot.export')['data']
    shop=next(n for n in scene['state']['routeMapNodes'] if n['nodeKind']=='Merchant' and n['outgoingNodeIds'])
    scene['state']['reachableRouteNodeIds']=[shop['nodeId']]
    command('snapshot.import',{'scene':scene})
    pc.refresh_player_flow_widgets_for_test()
    route=next(o for o in unreal.ObjectIterator(unreal.GameXXKOneGameRouteMapWidget) if o.get_owning_player()==pc)
    assert route.select_route_node_with_feedback(shop['nodeId'])
elif action=='leave-merchant':
    shop=next(o for o in unreal.ObjectIterator(unreal.GameXXKRouteMerchantWidget) if o.get_owning_player()==pc)
    assert shop.leave_merchant()
elif action=='confirm':
    receipt=mvp.get_pending_training_settlement_copy()
    assert mvp.confirm_training_settlement(receipt.receipt_id)
    assert mvp.start_training_challenge('Training.Normal.1-2')
    pc.refresh_player_flow_widgets_for_test()
elif action.startswith('capture-'):
    target_class=unreal.GameXXKTrainingSettlementWidget if action=='capture-settlement' else unreal.GameXXKOneGameRouteMapWidget
    target=next(o for o in unreal.ObjectIterator(target_class) if o.get_owning_player()==pc)
    assert unreal.GameXXKEditorCaptureAutomationLibrary.capture_live_game_widget(target,str(out/(action+'.png')),1920,1080)
elif action=='save':
    assert mvp.save_current_game()
elif action!='observe':
    raise ValueError(action)
r=json.loads(dev.execute_json(json.dumps({'command':'snapshot.export'})))
assert r['ok'],r
out.mkdir(exist_ok=True)
(out/(action+'.json')).write_text(json.dumps(r['data'],ensure_ascii=False,indent=2),encoding='utf8')
s=r['data']['state'];c=s['cardRun'];t=s['training']
print(json.dumps({'world':world.get_name(),'screen':s['screen'],'map':s['currentMapId'],'session':dev.is_session_active(),
    'challenge':t.get('bChallengeActive'),'stage':t.get('activeChallengeStageId'),'encounter':t.get('activeChallengeEncounterIndex'),
    'cleared':t.get('clearedStageIds'),'progression':t.get('partyProgressionStep'),'battleActive':c.get('bHasActiveCardBattle'),
    'phase':c.get('activeBattle',{}).get('phase'),'settlement':t.get('pendingSettlement'),'currentNode':s.get('currentRouteNodeId'),
    'reachable':s.get('reachableRouteNodeIds'),'visited':s.get('visitedRouteNodeIds'),
    'saveError':str(mvp.get_last_save_load_error()) if hasattr(mvp,'get_last_save_load_error') else None},ensure_ascii=False))
