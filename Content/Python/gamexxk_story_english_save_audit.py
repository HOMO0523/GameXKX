"""Read-only comparison after another task restarted the shared editor."""
import hashlib
import json
from pathlib import Path
import unreal
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'Saved/StorySystem/Localization'
original=json.loads((OUT/'ui-original.json').read_text(encoding='utf-8'))['data']['state']
def summary(state):
    return {'gold':state.player_gold,'level':state.player_level,'xp':state.player_xp,'hp':state.player_hp,
            'chests':len(state.training.owned_chest_tokens),
            'tasks':{str(k):str(v.state) for k,v in state.narrative_progress.task_progress_by_id.items()}}
report={'original':{k:original.get(k) for k in ('playerGold','playerLevel','playerXP','playerHP')},'slots':{}}
for slot in ('GameXXK_MVP_SaveSlot_1','GameXXK_DesktopTraining_Checkpoint'):
    loaded=unreal.GameplayStatics.load_game_from_slot(slot,0)
    if not loaded:
        report['slots'][slot]=None;continue
    state=loaded.get_editor_property('save_state').runtime_state
    report['slots'][slot]=summary(state)
    path=ROOT/'Saved/SaveGames'/(slot+'.sav')
    report['slots'][slot]['sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if world:
    instance=unreal.GameplayStatics.get_game_instance(world)
    mvp=next((x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer()==instance),None)
    dev=next((x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer()==instance),None)
    if mvp:report['live']=summary(mvp.get_runtime_state_copy())
    report['dev_session']=dev.is_session_active() if dev else None
(OUT/'save-audit-after-restart.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False))
