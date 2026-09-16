"""Read-only verification of restored travel-to-1-2 progression and visible guide text."""
import json
from pathlib import Path
import unreal

out=Path(unreal.Paths.project_dir())/'Saved/TravelTo12-20260916'
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if not world:
    print(json.dumps({'pie':False}))
    raise SystemExit(0)
assert 'L_DesktopTrainingHUD' in world.get_name()
gi=unreal.GameplayStatics.get_game_instance(world)
m=next(o for o in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if o.get_outer()==gi)
dev=next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer()==gi)
r=json.loads(dev.execute_json(json.dumps({'command':'snapshot.export'})))
assert r['ok'],r
s=r['data']['state'];t=s['training']
guides=[o for o in unreal.ObjectIterator(unreal.GameXXKInterfaceHelpWidget) if o.get_visibility() not in (unreal.SlateVisibility.COLLAPSED,unreal.SlateVisibility.HIDDEN)]
texts=[]
for block in unreal.ObjectIterator(unreal.TextBlock):
    outer=block.get_outer()
    if outer and outer.get_outer() in guides:
        text=str(block.get_text())
        if text:texts.append(text)
info={'pie':True,'profile':unreal.Paths.project_saved_dir(),'world':world.get_name(),'travelVictories':t['travelVictories'],
      'partyProgressionStep':t['partyProgressionStep'],'selectedStage':t['selectedStageId'],'challengeActive':t['bChallengeActive'],
      'challengeStage':t['activeChallengeStageId'],'travelActive':t['bTravelActive'],'gold':s['playerGold'],
      'stage12Tooltip':str(m.build_training_stage_tooltip('Training.Normal.1-2')),'guideText':texts,
      'guideEvents':[x for x in s['guideProgress']['completedGuideStepIds'] if x.startswith('UI.Progression.V1.')],
      'preference':s['guideProgress']['preference'],'error':str(m.get_last_save_load_error())}
name='live-challenge12.json' if t['bChallengeActive'] and t['activeChallengeStageId']=='Training.Normal.1-2' else 'live-desktop.json'
(out/name).write_text(json.dumps({'summary':info,'scene':r['data']},ensure_ascii=False,indent=2),encoding='utf8')
print(json.dumps(info,ensure_ascii=False))
