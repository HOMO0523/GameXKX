"""First normal-challenge guide checks; mutations require the isolated review profile."""
import json
import sys
from pathlib import Path
import unreal

action = sys.argv[1] if len(sys.argv) > 1 else 'observe'
out = Path(unreal.Paths.project_dir()) / 'Saved/FirstBattleFollowup-20260916'
out.mkdir(exist_ok=True)
if action == 'quit':
    unreal.SystemLibrary.quit_editor()
    raise SystemExit(0)
assert 'FirstBattleFollowup-20260916' in unreal.Paths.project_saved_dir(), 'Review requires isolated profile'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
gi = unreal.GameplayStatics.get_game_instance(world)
mvp = next(o for o in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if o.get_outer() == gi)
dev = next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer() == gi)
pc = unreal.GameplayStatics.get_player_controller(world, 0)

def snapshot():
    result = json.loads(dev.execute_json(json.dumps({'command': 'snapshot.export'})))
    assert result['ok'], result
    return result['data']

def board():
    return next(o for o in unreal.ObjectIterator(unreal.GameXXKBattleBoardWidget) if o.get_owning_player() == pc)

if action == 'prepare-first':
    assert mvp.start_game()
    assert mvp.start_training_challenge('Training.Normal.1-1')
    pc.refresh_player_flow_widgets_for_test()
elif action == 'prepare':
    assert mvp.start_game()
    pc.refresh_player_flow_widgets_for_test()
elif action == 'challenge':
    assert mvp.start_training_challenge('Training.Normal.1-1')
    pc.refresh_player_flow_widgets_for_test()
elif action == 'monster':
    s = snapshot()['state']
    node = next(n for n in s['routeMapNodes'] if n['nodeKind'] == 'Battle' and n['nodeId'] in s['reachableRouteNodeIds'])
    route = next(o for o in unreal.ObjectIterator(unreal.GameXXKOneGameRouteMapWidget) if o.get_owning_player() == pc)
    assert route.select_route_node_with_feedback(node['nodeId'])
elif action == 'select':
    card = next(c for c in snapshot()['state']['cardRun']['activeBattle']['deck']['hand'] if c['cardId'] == sys.argv[2])
    assert board().click_card_in_hand(card['instanceId'])
elif action == 'target':
    assert board().confirm_targeting_unit(sys.argv[2])
elif action == 'end-turn':
    assert board().end_card_player_phase()
elif action == 'auto':
    assert board().set_auto_battle_enabled(sys.argv[2] == 'true')
elif action == 'save':
    assert mvp.save_current_game()
elif action == 'reload':
    assert mvp.continue_game_from_slot('GameXXK_MVP_SaveSlot_1', 0)
    pc.refresh_player_flow_widgets_for_test()
elif action == 'capture':
    label = sys.argv[2] if len(sys.argv) > 2 else str(board().get_first_battle_guide_step_for_test())
    assert unreal.GameXXKEditorCaptureAutomationLibrary.capture_live_game_widget(board(), str(out / (label + '.png')), 1920, 1080)
elif action != 'observe':
    raise ValueError(action)

scene = snapshot()
s = scene['state']
b = s['cardRun'].get('activeBattle', {})
guide_board = next((o for o in unreal.ObjectIterator(unreal.GameXXKBattleBoardWidget) if o.get_owning_player() == pc), None)
info = {
    'action': action, 'screen': s['screen'], 'map': s['currentMapId'],
    'round': b.get('roundNumber'), 'phase': b.get('phase'),
    'hand': [c['cardId'] for c in b.get('deck', {}).get('hand', [])],
    'guideStep': str(guide_board.get_first_battle_guide_step_for_test()) if guide_board else None,
    'guideProgress': {k:s['guideProgress'].get(k) for k in ('bFirstBattleGuideEnabled','preference','completedGuideStepIds')}, 'deckGuidance': b.get('deck', {}).get('bFirstBattleGuidance'),
    'drawPhase': b.get('deck', {}).get('firstBattleDrawPhase'),
    'units': [{k: u.get(k) for k in ('unitId', 'side', 'hP', 'maxHP', 'armor', 'settlementHealingReceived', 'settlementArmorGenerated')} for u in b.get('units', [])],
    'saveError': str(mvp.get_last_save_load_error()), 'session': dev.is_session_active(),
}
(out / ('live-' + action + '-' + str(info['guideStep']) + '-r' + str(info['round']) + '.json')).write_text(json.dumps({'summary': info, 'scene': scene}, ensure_ascii=False, indent=2), encoding='utf8')
print(json.dumps(info, ensure_ascii=False))
