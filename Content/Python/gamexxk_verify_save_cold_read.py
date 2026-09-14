"""Read an isolated captured save in a fresh Unreal process; never use the player profile."""
import json
from pathlib import Path
import unreal

saved = str(unreal.Paths.project_saved_dir()).replace('\\', '/')
if 'SaveHardening/PreviewUserV42' not in saved:
    raise RuntimeError('Expected isolated PreviewUserV42')
slot = 'GameXXK_MVP_SaveSlot_1'
raw = unreal.GameplayStatics.load_game_from_slot(slot, 0)
if not raw or raw.save_state.save_version != 42:
    raise RuntimeError('Missing v42 source fixture')
expected_gold = raw.save_state.runtime_state.player_gold
instance = unreal.new_object(unreal.GameInstance)
mvp = unreal.new_object(unreal.GameXXKMVPSubsystem, outer=instance)
ok = mvp.load_game_from_slot(slot, 0)
actual_gold = mvp.get_runtime_state_copy().player_gold
result = {'ok': bool(ok and actual_gold == expected_gold), 'version': raw.save_state.save_version,
          'expected_gold': expected_gold, 'actual_gold': actual_gold,
          'error': str(mvp.get_last_save_load_error()), 'user_dir': saved,
          'verification': 'fresh process, storage-integrity and migration load through MVP subsystem'}
target = Path(unreal.Paths.project_dir()).resolve() / 'Saved/SaveHardening/cold-read-v42.json'
target.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(result, ensure_ascii=False))
if not result['ok']:
    raise RuntimeError('Cold save read failed')
