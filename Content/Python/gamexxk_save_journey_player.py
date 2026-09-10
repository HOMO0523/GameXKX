"""Save the latest legitimate user state before the coordinated story-flow rebuild."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import unreal

root=Path(__file__).resolve().parents[2]
out=root/'Saved/StorySystem/JourneyFlowFix'
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
instance=unreal.GameplayStatics.get_game_instance(world)
mvp=next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer()==instance)
dev=next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer()==instance)
assert not dev.is_session_active()
assert mvp.save_current_game('',0),str(mvp.get_last_save_load_error())
snapshot=json.loads(dev.execute_json(json.dumps({'schema':1,'command':'snapshot.export','args':{}})))
assert snapshot['ok'] and not snapshot['session_active']
(out/'latest-player-before-build.json').write_text(json.dumps(snapshot,ensure_ascii=False,indent=2),encoding='utf-8')
backup=out/('PlayerSaveBackup-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
backup.mkdir(parents=True,exist_ok=False)
hashes={}
for path in (root/'Saved/SaveGames').glob('*.sav'):
    shutil.copy2(path,backup/path.name)
    hashes[path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
state=mvp.get_runtime_state_copy()
report={'gold':state.player_gold,'level':state.player_level,'hp':state.player_hp,
        'backup':str(backup),'save_files':hashes,'dev_session':False}
(out/'latest-player-backup.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'saved':True,'gold':state.player_gold,'level':state.player_level,'hp':state.player_hp,'backup':str(backup)}))
