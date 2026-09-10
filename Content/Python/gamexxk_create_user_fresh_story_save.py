"""Create the user-requested fresh save, retaining the previous game in a free manual slot."""
import hashlib
import json
from pathlib import Path
import shutil
import unreal

root=Path(__file__).resolve().parents[2]
out=root/'Saved/StorySystem/JourneyFlowFix'
normal=root/'Saved/SaveGames'
isolated=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))/'SaveGames'
assert isolated.resolve()!=normal.resolve(), 'This preparation must use an isolated user directory'
isolated.mkdir(parents=True,exist_ok=True)
gi=unreal.new_object(unreal.GameInstance)
mvp=unreal.new_object(unreal.GameXXKMVPSubsystem,outer=gi)
assert mvp.start_game()
fresh=mvp.get_runtime_state_copy()
assert fresh.player_level==1 and not fresh.narrative_progress.task_progress_by_id
assert not fresh.training.challenge_active and not fresh.card_run.has_active_card_battle
temp_slot='GameXXK_UserFreshStory_Handoff'
assert mvp.save_current_game(temp_slot,0),str(mvp.get_last_save_load_error())
assert mvp.load_game_from_slot(temp_slot,0),str(mvp.get_last_save_load_error())
new_bytes=(isolated/(temp_slot+'.sav')).read_bytes()

free_slots=[unreal.GameXXKMVPSubsystem.get_manual_save_slot_name(i)
    for i in range(1,unreal.GameXXKMVPSubsystem.get_manual_save_slot_count())
    if not (normal/(unreal.GameXXKMVPSubsystem.get_manual_save_slot_name(i)+'.sav')).exists()]
assert free_slots,'No free manual slot for preserving the previous game'
old_candidates=[normal/'GameXXK_DesktopTraining_Checkpoint.sav',normal/'GameXXK_MVP_SaveSlot_1.sav']
old_slot=free_slots[0];previous=None
for candidate in sorted([p for p in old_candidates if p.exists()],key=lambda p:p.stat().st_mtime,reverse=True):
    shutil.copy2(candidate,isolated/'GameXXK_PreviousUserProgress.sav')
    if mvp.load_game_from_slot('GameXXK_PreviousUserProgress',0):
        previous=mvp.get_runtime_state_copy()
        shutil.copy2(candidate,normal/(old_slot+'.sav'))
        assert (normal/(old_slot+'.sav')).read_bytes()==candidate.read_bytes()
        break
assert previous is not None,'Previous player progress could not be validated'
for name in ('GameXXK_MVP_SaveSlot_1','GameXXK_DesktopTraining_Checkpoint'):
    target=normal/(name+'.sav');staging=normal/(name+'.fresh-staging')
    assert not staging.exists()
    staging.write_bytes(new_bytes);staging.replace(target)
    assert target.read_bytes()==new_bytes
report={'created':True,'new_slot':'GameXXK_MVP_SaveSlot_1','previous_slot':old_slot,
    'new_level':fresh.player_level,'new_gold':fresh.player_gold,'new_task_count':len(fresh.narrative_progress.task_progress_by_id),
    'previous_gold':previous.player_gold,'previous_level':previous.player_level,
    'fresh_sha256':hashlib.sha256(new_bytes).hexdigest(),'new_save_roundtrip_verified':True,
    'story_not_advanced':True,'ready_for_user_verification':True,
    'backup':json.loads((out/'fresh-backup-pointer.json').read_text(encoding='utf-8'))}
(out/'user-fresh-save.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False))
