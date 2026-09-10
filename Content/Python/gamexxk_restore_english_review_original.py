"""Recover the latest genuine snapshot after a coordinated editor restart.

The only direct serializer write is a unique recovery slot. The player slot and
checkpoint are updated only through the normal validated MVP load/save path.
"""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import unreal

ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Saved/StorySystem/Localization'
snapshot=json.loads((OUT/'ui-original.json').read_text(encoding='utf-8'))
assert snapshot['ok'] and not snapshot['session_active']
scene=snapshot['data'];source=scene['state']
assert len(source['narrativeProgress']['taskProgressById'])<61
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
instance=unreal.GameplayStatics.get_game_instance(world)
mvp=next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer()==instance)
dev=next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer()==instance)
assert not dev.is_session_active()
current=mvp.get_runtime_state_copy()
assert current.player_level<=source['playerLevel'] and current.player_gold<=source['playerGold'], 'A newer live player state requires comparison before recovery'
stamp=datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
backup=OUT/('before-original-recovery-'+stamp);backup.mkdir()
hashes={}
for path in (ROOT/'Saved/SaveGames').glob('*.sav'):
    shutil.copy2(path,backup/path.name);hashes[path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
(backup/'hashes.json').write_text(json.dumps(hashes,indent=2),encoding='utf-8')
def command(name,args=None):
    result=json.loads(dev.execute_json(json.dumps({'schema':1,'command':name,'args':args or {}})))
    assert result['ok'],result
    return result

# The export contains already-earned live rewards through its capture time.
# Do not count the preceding live period again as offline time on normal load.
captured=int(datetime.datetime.fromisoformat(scene['created_at'].replace('Z','+00:00')).timestamp())
source['training']['travelLastUpdatedUnixSeconds']=max(source['training']['travelLastUpdatedUnixSeconds'],captured)
command('snapshot.import',{'scene':scene})
slot='GameXXK_EnglishReview_Recovery_'+stamp
try:
    state=mvp.get_runtime_state_copy()
    assert state.player_gold==source['playerGold'] and state.player_level==source['playerLevel']
    assert not unreal.GameplayStatics.does_save_game_exist(slot,0)
    save=unreal.GameplayStatics.create_save_game_object(unreal.GameXXKSaveGame)
    save.set_editor_property('save_state',unreal.GameXXKMVPRules.make_save_state(state))
    assert unreal.GameplayStatics.save_game_to_slot(save,slot,0)
finally:
    command('session.restore')
assert not dev.is_session_active()
assert mvp.load_game_from_slot(slot,0), str(mvp.get_last_save_load_error())
assert mvp.save_current_game('GameXXK_MVP_SaveSlot_1',0), str(mvp.get_last_save_load_error())
restored=command('snapshot.export')
after=restored['data']['state']
for key in ('narrativeProgress','equipmentInventory','talents'):
    if key in source:assert source[key]==after[key], ('Restoration mismatch',key)
assert after['playerGold']>=source['playerGold'] and after['playerLevel']>=source['playerLevel']
report={'recovered':True,'dev_session':False,'slot':slot,'backup':str(backup),'source_gold':source['playerGold'],
        'restored_gold':after['playerGold'],'restored_level':after['playerLevel'],
        'real_tasks':len(after['narrativeProgress']['taskProgressById'])}
(OUT/'original-restoration.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
(OUT/'restored-real-player.json').write_text(json.dumps(restored,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.GameplayStatics.get_player_controller(world,0).refresh_player_flow_widgets_from_state()
print(json.dumps(report))
