"""Guarded setup/observe/restore for real right-click verification."""
import copy
import hashlib
import json
import sys
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base

world, pc, wb = base._controller_and_widget()
assert world and wb and 'L_DesktopTrainingHUD' in world.get_path_name()
root = Path(unreal.Paths.project_dir())
out = root / 'Saved/StorySystem/ChestBlock'
instance = unreal.GameplayStatics.get_game_instance(world)
dev = next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer() == instance)
mvp = wb.get_mvp_subsystem()

def run(command, args=None):
    result = json.loads(dev.execute_json(json.dumps({'schema': 1, 'command': command, 'args': args or {}})))
    assert result['ok'], result
    return result

def hashes():
    return {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in (root/'Saved/SaveGames').glob('*.sav')}

def prepare_chests():
    scene = copy.deepcopy(json.loads((out / 'live-snapshot.json').read_text(encoding='utf-8'))['data'])
    scene['state']['training']['bTravelActive'] = False
    scene['state']['training']['activeTravelEncounterIndex'] = -1
    scene['travel'] = {}
    for seed in [3] + list(range(1, 65)):
        scene['state']['training']['challengeRewardSeed'] = seed
        run('snapshot.import', {'scene': scene})
        preview = mvp.open_all_training_chests(unreal.GameXXKTrainingRewardTier.NORMAL_CHEST)
        drops = {str(k): v for k, v in preview.item_deltas.items() if str(k).lower() != 'item.travelmoney'} if preview else {}
        if not drops:
            continue
        expected = {}
        inventory = scene['state']['inventory']
        warehouse = scene['state']['desktopInventory']['warehouseItems']
        for item_id, quantity in drops.items():
            total = 0
            for items in [inventory, warehouse]:
                for key in list(items):
                    if key.lower() == item_id.lower():
                        total += items.pop(key)
            warehouse[item_id] = max(1, total)
            expected[item_id.lower()] = warehouse[item_id] + quantity
        run('snapshot.import', {'scene': scene})
        (out / 'ui-expected.json').write_text(json.dumps({'seed': seed, 'stored': expected}, indent=2), encoding='utf-8')
        wb.open_workbench()
        wb.open_backpack()
        return
    raise AssertionError('No stack drop found for UI verification')

mode = sys.argv[1]
if mode == 'begin':
    assert not dev.is_session_active()
    assert mvp.save_current_game('', 0), str(mvp.get_last_save_load_error())
    original = {'snapshot': run('snapshot.export')['data'], 'hashes': hashes(),
                'language': unreal.GameXXKLocalizationLibrary.get_language()}
    (out / 'ui-original.json').write_text(json.dumps(original, ensure_ascii=False, indent=2), encoding='utf-8')
    run('session.begin')
    prepare_chests()
elif mode == 'reset':
    assert dev.is_session_active()
    prepare_chests()
elif mode == 'restore':
    assert dev.is_session_active()
    original = json.loads((out / 'ui-original.json').read_text(encoding='utf-8'))
    run('session.restore')
    unreal.GameXXKLocalizationLibrary.set_language(original['language'], False)
    assert run('snapshot.export')['data']['state'] == original['snapshot']['state']
    assert hashes() == original['hashes']
    assert not dev.is_session_active()
    wb.open_backpack()
elif mode == 'verify':
    assert dev.is_session_active()
    state = run('snapshot.export')['data']['state']
    assert mvp.get_training_chest_count(unreal.GameXXKTrainingRewardTier.NORMAL_CHEST) == 0
    stored = {k.lower(): v for k, v in state['desktopInventory']['warehouseItems'].items()}
    expected = json.loads((out / 'ui-expected.json').read_text(encoding='utf-8'))['stored']
    for key, count in expected.items():
        assert stored[key] == count, (key, count, stored)
        assert key not in {k.lower() for k in state['inventory']}
    assert str(mvp.get_last_save_load_error()) == ''
    (out / 'right-click-after.json').write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding='utf-8')
elif mode == 'counts':
    assert dev.is_session_active()
    scene = run('snapshot.export')['data']
    for items in [scene['state']['inventory'], scene['state']['desktopInventory']['warehouseItems']]:
        key = next((k for k in items if k.lower() == 'item.travelmoney'), 'Item.TravelMoney')
        items[key] = 20
    run('snapshot.import', {'scene': scene})
    wb.open_backpack()
    if not wb.is_warehouse_panel_open_for_test():
        wb.handle_desktop_action_for_test(0)
elif mode == 'language':
    assert dev.is_session_active()
    unreal.GameXXKLocalizationLibrary.set_language(sys.argv[2], False)
elif mode != 'observe':
    raise AssertionError(mode)
print(json.dumps({'mode': mode, 'dev_session': dev.is_session_active(),
                  'warehouse_open': wb.is_warehouse_panel_open_for_test(),
                  'normal_chests': mvp.get_training_chest_count(unreal.GameXXKTrainingRewardTier.NORMAL_CHEST),
                  'save_error': str(mvp.get_last_save_load_error())}, ensure_ascii=False))
