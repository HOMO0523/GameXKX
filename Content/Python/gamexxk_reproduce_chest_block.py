"""Compare the captured chest scene with a warehouse-free control in a guarded session."""
import copy
import hashlib
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
out = root / 'Saved/StorySystem/ChestBlock'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
instance = unreal.GameplayStatics.get_game_instance(world)
mvp = next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer() == instance)
dev = next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer() == instance)
assert not dev.is_session_active()
assert mvp.save_current_game('', 0), str(mvp.get_last_save_load_error())

def run(command, args=None):
    result = json.loads(dev.execute_json(json.dumps({'schema': 1, 'command': command, 'args': args or {}})))
    assert result['ok'], result
    return result

original = run('snapshot.export')['data']
(out / 'player-before-repro.json').write_text(json.dumps(original, ensure_ascii=False, indent=2), encoding='utf-8')
disk_before = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'Saved/SaveGames').glob('*.sav')}
captured = json.loads((out / 'live-snapshot.json').read_text(encoding='utf-8'))['data']
report = []
run('session.begin')
try:
    for control in [False, True]:
        scene = copy.deepcopy(captured)
        if control:
            scene['state']['desktopInventory']['warehouseItems'] = {}
            scene['state']['desktopInventory']['warehouseSlots'] = []
        run('snapshot.import', {'scene': scene})
        before = run('snapshot.export')['data']['state']
        for tier in [unreal.GameXXKTrainingRewardTier.NORMAL_CHEST, unreal.GameXXKTrainingRewardTier.ADVANCED_CHEST]:
            count = mvp.get_training_chest_count(tier)
            if not count:
                continue
            result = mvp.open_all_training_chests(tier)
            after = run('snapshot.export')['data']['state']
            report.append({'control_without_warehouse_stacks': control, 'tier': str(tier),
                           'before_count': count, 'succeeded': result is not None,
                           'result': str(result), 'after_count': mvp.get_training_chest_count(tier),
                           'inventory': after['inventory'], 'warehouse_items': after['desktopInventory']['warehouseItems'],
                           'unchanged_on_failure': before == after if result is None else None,
                           'save_error': str(mvp.get_last_save_load_error())})
            before = after
    for seed in range(1, 65):
        scene = copy.deepcopy(captured)
        scene['state']['training']['challengeRewardSeed'] = seed
        run('snapshot.import', {'scene': scene})
        before = run('snapshot.export')['data']['state']
        result = mvp.open_all_training_chests(unreal.GameXXKTrainingRewardTier.NORMAL_CHEST)
        if result is None:
            after = run('snapshot.export')['data']['state']
            scene['state']['desktopInventory']['warehouseItems'] = {}
            scene['state']['desktopInventory']['warehouseSlots'] = []
            run('snapshot.import', {'scene': scene})
            control_result = mvp.open_all_training_chests(unreal.GameXXKTrainingRewardTier.NORMAL_CHEST)
            report.append({'seed': seed, 'failed_with_warehouse': True,
                           'unchanged_on_failure': before == after,
                           'warehouse_free_control': str(control_result)})
            break
finally:
    run('session.restore')
    assert not dev.is_session_active()
    assert run('snapshot.export')['data']['state'] == original['state'], 'real player restoration mismatch'
    assert {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'Saved/SaveGames').glob('*.sav')} == disk_before
(out / 'reproduction.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'cases': report, 'player_restored': True, 'save_files_unchanged': True}, ensure_ascii=False))
