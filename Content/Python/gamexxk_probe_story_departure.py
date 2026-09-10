"""Reproduce the pre-dialogue route jump in a restored, write-suppressed Dev session."""
import copy
import datetime
import hashlib
import json
from pathlib import Path
import unreal

root=Path(__file__).resolve().parents[2]
out=root/'Saved/StorySystem/JourneyFlowFix'
out.mkdir(parents=True,exist_ok=True)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
instance=unreal.GameplayStatics.get_game_instance(world)
mvp=next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer()==instance)
story=next(x for x in unreal.ObjectIterator(unreal.GameXXKMainStorySubsystem) if x.get_outer()==instance)
dev=next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer()==instance)
def command(name,args=None):
    r=json.loads(dev.execute_json(json.dumps({'schema':1,'command':name,'args':args or {}})))
    assert r['ok'],r
    return r
before=command('snapshot.export')
assert not before['session_active']
assert before['data']['state']['narrativeProgress']['mainStory']['activeNodeId']=='S00-04'
(out/'original-player-snapshot.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
assert mvp.save_current_game('',0),str(mvp.get_last_save_load_error())
save_paths=list((root/'Saved/SaveGames').glob('*.sav'))
hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in save_paths}
(out/'original-save-hashes.json').write_text(json.dumps(hashes,indent=2),encoding='utf-8')
report={'before':json.loads(story.get_progress_json()),'real_progress_saved':True}
try:
    command('snapshot.import',{'scene':copy.deepcopy(before['data'])})
    assert dev.is_session_active()
    assert mvp.cancel_training_challenge_to_workbench()
    scene=command('snapshot.export')['data']
    state=scene['state']
    main=state['narrativeProgress']['mainStory']
    main['activeNodeId']='None';main['phase']='None';main['lineIndex']=0
    tasks=state['narrativeProgress']['taskProgressById']
    task=tasks[next(key for key in tasks if key.lower()=='s00-04')]
    counts=task['objectiveCounts']
    line_key=next((key for key in counts if key.lower()=='mainstory.dialogueline'),'MainStory.DialogueLine')
    counts[line_key]=0
    command('snapshot.import',{'scene':scene})
    assert story.start_task(unreal.Name('S00-04'))
    actual=mvp.get_runtime_state_copy()
    report['after_start_task']={'challenge_active':actual.training.challenge_active,
        'screen':str(actual.screen),'story':json.loads(story.get_progress_json())}
    report['reproduced_premature_route_entry']=bool(actual.training.challenge_active)
finally:
    report['restore']=command('session.restore')
    after=command('snapshot.export')
    report['player_state_restored_exactly']=after['data']['state']==before['data']['state']
    report['save_hashes_unchanged']=all(Path(p).exists() and hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in hashes.items())
    (out/'pre-fix-reproduction.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    assert report['player_state_restored_exactly'] and report['save_hashes_unchanged'],report
print(json.dumps(report,ensure_ascii=False))
