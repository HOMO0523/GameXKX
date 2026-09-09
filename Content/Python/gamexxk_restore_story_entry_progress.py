"""Load the real prebuild recovery slot through the repaired normal save path."""
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
folder = root / 'Saved/StorySystem/TaskEntryBug'
recovery = json.loads((folder / 'recovery-slot.json').read_text(encoding='utf-8'))
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())).resolve() == (root / 'Saved').resolve()
instance = unreal.GameplayStatics.get_game_instance(world)
mvp = next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer() == instance)
dev = next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer() == instance)

def export():
    result = json.loads(dev.execute_json(json.dumps({'schema': 1, 'command': 'snapshot.export', 'args': {}})))
    assert result['ok'] and not result['session_active'], result
    return result

before = export()
(folder / 'postbuild-before-recovery.json').write_text(json.dumps(before, ensure_ascii=False, indent=2), encoding='utf-8')
before_state = before['data']['state']
assert before_state['playerLevel'] <= recovery['level'] and before_state['playerGold'] <= recovery['gold'], 'Reassess: newer player progress exists'
assert mvp.load_game_from_slot(recovery['slot'], recovery['user_index']), str(mvp.get_last_save_load_error())
restored = export()
state = restored['data']['state']
source = json.loads((folder / 'prebuild-live-snapshot.json').read_text(encoding='utf-8'))['data']['state']
for key in ('playerLevel', 'playerXP', 'playerGold', 'playerHP', 'playerMaxHP', 'talents', 'inventory', 'equipmentCollection', 'narrativeProgress'):
    assert state[key] == source[key], (key, state[key], source[key])
assert mvp.save_current_game('', 0), str(mvp.get_last_save_load_error())
default = unreal.GameplayStatics.load_game_from_slot(unreal.GameXXKMVPSubsystem.get_default_save_slot_name(), 0)
assert default
saved = default.get_editor_property('save_state').runtime_state
assert saved.player_gold == recovery['gold'] and saved.player_hp == recovery['hp'] and saved.player_level == recovery['level']
report = {'ok': True, 'recovery_slot': recovery['slot'], 'normal_save_verified': True,
    'fixture_session_active': False, 'gold': saved.player_gold, 'level': saved.player_level,
    'xp': saved.player_xp, 'hp': saved.player_hp, 'base_max_hp': saved.player_max_hp,
    'inventory_equipment_talents_story_preserved': True, 'world': world.get_name()}
(folder / 'restored-live-snapshot.json').write_text(json.dumps(restored, ensure_ascii=False, indent=2), encoding='utf-8')
(folder / 'recovery-verified.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False))
