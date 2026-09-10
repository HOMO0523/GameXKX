"""Public-action story aftermath check inside an explicitly temporary Dev session."""
import json
from pathlib import Path
import sys
import unreal

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'Saved/StorySystem/BattleAftermath'
OUT.mkdir(parents=True, exist_ok=True)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
instance = unreal.GameplayStatics.get_game_instance(world)
pc = unreal.GameplayStatics.get_player_controller(world, 0)
mvp = next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer() == instance)
story = next(x for x in unreal.ObjectIterator(unreal.GameXXKMainStorySubsystem) if x.get_outer() == instance)
dev = next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer() == instance)
nodes = {n['id']: n for c in json.loads((ROOT/'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))['chapters'] for n in c['nodes']}

def command(name, args=None):
    result = json.loads(dev.execute_json(json.dumps({'schema': 1, 'command': name, 'args': args or {}})))
    assert result['ok'], result
    return result

mode = sys.argv[1] if len(sys.argv) > 1 else 'observe'
if mode == 'prepare':
    assert not dev.is_session_active(), 'Do not replace another temporary test session'
    before = command('snapshot.export')
    (OUT/'ui-original.json').write_text(json.dumps(before, ensure_ascii=False, indent=2), encoding='utf-8')
    command('session.begin')
    assert mvp.start_game()
    command('character.level', {'character': 'Player', 'level': 50})
    for node_id in ('S00-01', 'S00-02', 'S00-03'):
        assert story.start_task(unreal.Name(node_id)), str(story.feedback())
        for _ in nodes[node_id]['lines']:
            assert story.advance_dialogue(), str(story.feedback())
        if nodes[node_id]['options']:
            correct = next(i for i, o in enumerate(nodes[node_id]['options']) if o['correct'])
            assert story.choose_answer(correct)
    assert story.start_task(unreal.Name('S00-04'))
    for _ in nodes['S00-04']['lines']:
        assert story.advance_dialogue()
    pc.refresh_player_flow_widgets_from_state()
    wb = pc.get_desktop_training_workbench_widget_for_test()
    wb.open_backpack()
    wb.handle_desktop_action_for_test(2100)
    assert story.start_task(unreal.Name('S00-04'))
elif mode == 'enter':
    assert dev.is_session_active()
    assert story.begin_task_journey()
    gate = list(mvp.get_runtime_state_copy().narrative_progress.main_story.gate_node_ids)[0]
    assert story.enter_journey_gate(gate)
elif mode == 'resolve':
    assert dev.is_session_active()
    steps = 0
    while mvp.get_runtime_state_copy().card_run.has_active_card_battle and steps < 512:
        result = mvp.advance_training_challenge_encounter()
        # UE Python exposes the out parameters on success: stage-cleared=False
        # is correct for tasks. Failed bool calls with outs return None.
        assert result is not None, repr(result)
        steps += 1
    assert steps < 512
    assert mvp.get_runtime_state_copy().narrative_progress.main_story.battle_won
    pc.refresh_player_flow_widgets_from_state()
elif mode == 'pause-exit':
    assert dev.is_session_active()
    assert str(mvp.get_runtime_state_copy().narrative_progress.main_story.active_node_id).upper() == 'S00-04'
    assert mvp.get_runtime_state_copy().narrative_progress.main_story.line_index == 0
    assert story.advance_dialogue()
    assert mvp.get_runtime_state_copy().narrative_progress.main_story.line_index == 1
    story.pause_activity()
    assert mvp.cancel_training_challenge_to_workbench()
    pc.refresh_player_flow_widgets_from_state()
elif mode == 'resume':
    assert dev.is_session_active()
    assert story.start_task(unreal.Name('S00-04'))
    assert mvp.get_runtime_state_copy().narrative_progress.main_story.line_index == 1
    assert not mvp.get_runtime_state_copy().training.challenge_active
    pc.get_desktop_training_workbench_widget_for_test().open_backpack()
    pc.get_desktop_training_workbench_widget_for_test().handle_desktop_action_for_test(2100)
    assert story.start_task(unreal.Name('S00-04'))
elif mode == 'finish':
    assert dev.is_session_active()
    for _ in range(1, len(nodes['S00-04']['after_battle_lines'])):
        assert story.advance_dialogue()
    assert story.claim_reward(unreal.Name('S00-04'))
elif mode == 'detail':
    assert dev.is_session_active()
    pc.get_desktop_training_workbench_widget_for_test().handle_desktop_action_for_test(2100)
elif mode == 'restore':
    command('session.restore')
    before = json.loads((OUT/'ui-original.json').read_text(encoding='utf-8'))
    after = command('snapshot.export')
    assert before['data']['state'] == after['data']['state']
    assert not dev.is_session_active()
    pc.refresh_player_flow_widgets_from_state()

state = mvp.get_runtime_state_copy()
report = {'mode': mode, 'dev_session': dev.is_session_active(), 'story': json.loads(story.get_progress_json()),
          'line': state.narrative_progress.main_story.line_index, 'route': state.training.challenge_active,
          'battle': state.card_run.has_active_card_battle, 'gold': state.player_gold,
          'enemies': [str(e.enemy_definition_id) for e in state.active_battle_enemies]}
(OUT/('ui-'+mode+'.json')).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k != 'story'}, ensure_ascii=False))
