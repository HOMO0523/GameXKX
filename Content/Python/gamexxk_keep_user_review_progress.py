"""User-approved promotion of manual play during the isolated UI review."""
import hashlib
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base

world,pc,wb=base._controller_and_widget()
assert world and wb and 'L_DesktopTrainingHUD' in world.get_path_name()
instance=unreal.GameplayStatics.get_game_instance(world)
dev=next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer()==instance)
assert dev.is_session_active(), 'Do not overwrite a session that has already left test mode'
academy=next(o for o in unreal.ObjectIterator(unreal.GameXXKAcademySubsystem) if o.get_outer()==instance)
academy.cancel_course()  # Restore actual progress if a borrowed lesson is currently open.
mvp=wb.get_mvp_subsystem()
def run(command,args=None):
    result=json.loads(dev.execute_json(json.dumps({'command':command,'args':args or {}})))
    assert result.get('ok'),result
    return result

root=Path(unreal.Paths.project_dir()).resolve()
out=root/'Saved/Codex/UIGuidanceLocalization-20260910/ui-review'
stamp=datetime.now().strftime('%Y%m%d-%H%M%S')
held=run('snapshot.export')['data']
(out/('approved-user-progress-'+stamp+'.json')).write_text(json.dumps(held,ensure_ascii=False,indent=2),encoding='utf-8')
backup=out/('player-before-keep-'+stamp);backup.mkdir()
for path in (root/'Saved/SaveGames').glob('*.sav'):shutil.copy2(path,backup/path.name)
recovery_name='GameXXK_UserUI_Keep_'+stamp.replace('-','_')
save=unreal.GameplayStatics.create_save_game_object(unreal.GameXXKSaveGame)
save.set_editor_property('save_state',unreal.GameXXKMVPRules.make_save_state(mvp.get_runtime_state_copy()))
assert unreal.GameplayStatics.save_game_to_slot(save,recovery_name,0), 'Could not write the independent recovery copy'
copy=unreal.GameplayStatics.load_game_from_slot(recovery_name,0)
assert copy and copy.save_state.runtime_state.player_gold==held['state']['playerGold']

run('session.restore')
if not mvp.load_game_from_slot(recovery_name,0):
    run('snapshot.import',{'scene':held})
    raise AssertionError('Kept progress is restored in test mode; normal load failed: '+str(mvp.get_last_save_load_error()))
after=run('snapshot.export')['data']['state']
for key in ('playerGold','playerLevel','playerXP','inventory','desktopInventory','equipmentCollection','cardRun','narrativeProgress'):
    assert after[key]==held['state'][key], 'Restored user progress differs in '+key
original=json.loads((out/'review-original.json').read_text(encoding='utf-8'))
resume=original['runtime']['data']['state']['training']['bTravelActive']
current=mvp.get_runtime_state_copy()
if resume and not current.training.travel_active and not current.training.challenge_active:
    stage=current.training.current_travel_stage_id
    if str(stage)!='None':assert mvp.start_training_travel(stage), 'Could not resume the idle stage'
assert mvp.save_current_game('GameXXK_MVP_SaveSlot_1',0), str(mvp.get_last_save_load_error())
assert not dev.is_session_active()
final=run('snapshot.export')['data']['state']
report={'approved':True,'recoverySlot':recovery_name,'backup':str(backup),'gold':final['playerGold'],
        'level':final['playerLevel'],'chests':len(final['training']['ownedChestTokens']),
        'devSession':False,'resumeIdle':resume,
        'primaryHash':hashlib.sha256((root/'Saved/SaveGames/GameXXK_MVP_SaveSlot_1.sav').read_bytes()).hexdigest()}
(out/'approved-user-progress-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=True))
