"""Read-only battle/foreground snapshot after the Burn intent fix."""
import json
from pathlib import Path
import unreal

world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
if not world:
    print(json.dumps({'pie':False}))
else:
    assert 'L_DesktopTrainingHUD' in world.get_name()
    gi=unreal.GameplayStatics.get_game_instance(world)
    dev=next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer()==gi)
    result=json.loads(dev.execute_json(json.dumps({'command':'snapshot.export'})))
    assert result['ok'],result
    s=result['data']['state'];b=s['cardRun']['activeBattle']
    cards=[o for o in unreal.ObjectIterator(unreal.Button) if o.get_name()=='BattleEnemyIntentShowcaseCard']
    layers=[o.slot.get_z_order() for o in cards if isinstance(o.slot,unreal.CanvasPanelSlot)]
    info={'pie':True,'screen':s['screen'],'stage':s['training']['activeChallengeStageId'],'phase':b['phase'],'round':b['roundNumber'],
          'intentIndex':s['cardRun']['nextEnemyIntentIndex'],'showcaseLayers':layers,'session':dev.is_session_active(),
          'enemies':[{'id':u['unitId'],'hp':u['hP'],'armor':u['armor'],
                      'burn':sum(x['stacks'] for x in u['statuses'] if x['status']=='Burn'),
                      'healthLost':u['settlementHealthLost']} for u in b['units'] if u['side']=='Enemy']}
    out=Path(unreal.Paths.project_dir())/'Saved/EnemyIntentFix-20260916'
    (out/('live-r'+str(info['round'])+'-'+info['phase']+'.json')).write_text(json.dumps({'summary':info,'scene':result['data']},ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(info,ensure_ascii=False))
