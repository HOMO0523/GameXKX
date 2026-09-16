"""Isolated teaching UI probe. Sixth-stage setup is a fixture, not played progress."""
import json
from pathlib import Path
import sys
import unreal

assert 'TeachingChests-20260915' in unreal.Paths.project_saved_dir()
if len(sys.argv)>1 and sys.argv[1]=='quit-editor':
    unreal.SystemLibrary.quit_editor()
    raise SystemExit(0)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
instance = unreal.GameplayStatics.get_game_instance(world)
pc = unreal.GameplayStatics.get_player_controller(world, 0)
mvp = next(o for o in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if o.get_outer() == instance)
dev = next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer() == instance)
host = next(o for o in unreal.ObjectIterator(unreal.GameXXKDesktopTrainingWorkbenchWidget) if o.get_owning_player() == pc)
action = sys.argv[1] if len(sys.argv) > 1 else 'observe'
if action == 'open-backpack':
    host.open_backpack()
elif action == 'save':
    assert mvp.save_current_game()
elif action in ('fixture-sixth', 'fixture-third', 'fixture-fifth'):
    assert 'SoftUser' in unreal.Paths.project_saved_dir(), 'Keep earlier player-operated profiles intact'
    mvp.start_game()
    mvp.open_teaching_chest(False, False)
    scene = json.loads(dev.execute_json(json.dumps({'command': 'snapshot.export', 'args': {}})))['data']
    p = scene['state']['guideProgress']['teachingChests']
    stage = 6 if action == 'fixture-sixth' else 5 if action == 'fixture-fifth' else 3
    p.update(stage=stage, completedStages=stage-1, bOpened=False, bDismissed=False)
    training = scene['state']['training']
    training['ownedChestTokens'] = [t for t in training['ownedChestTokens'] if t.get('fixedDropId', 'None') == 'None']
    training['nextChestAcquisitionOrdinal'] += 1
    training['ownedChestTokens'].append({'tier': 'NormalChest', 'sourceStageId': 'Training.Normal.1-1',
        'sourceItemLevel': 1, 'acquisitionOrdinal': training['nextChestAcquisitionOrdinal'],
        'fixedDropId': 'TeachingChest.Stage.' + str(stage)})
    result = json.loads(dev.execute_json(json.dumps({'command': 'snapshot.import', 'args': {'scene': scene}})))
    assert result['ok'], result
    host.open_backpack()
elif action != 'observe':
    raise ValueError(action)
scene = json.loads(dev.execute_json(json.dumps({'command': 'snapshot.export', 'args': {}})))
assert scene['ok'], scene
state = scene['data']['state']
texts = []
for widget in unreal.ObjectIterator(unreal.TextBlock):
    if not widget.is_visible() or not widget.get_parent():
        continue
    owner = widget
    while owner:
        if isinstance(owner, unreal.UserWidget) and owner.get_owning_player() == pc:
            if str(widget.get_text()):
                texts.append({'name': widget.get_name(), 'text': str(widget.get_text())})
            break
        owner = owner.get_outer()
label = sys.argv[2] if len(sys.argv) > 2 else action
assert label.replace('-', '').replace('_', '').isalnum()
out = Path(unreal.Paths.project_dir()) / 'Saved/TeachingChests-20260915'
(out / (label + '.json')).write_text(json.dumps({'action': action, 'world': world.get_name(), 'scene': scene['data'], 'texts': texts}, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'record': label, 'teaching': state['guideProgress']['teachingChests'], 'travel': state['training']['bTravelActive']}, ensure_ascii=False))
