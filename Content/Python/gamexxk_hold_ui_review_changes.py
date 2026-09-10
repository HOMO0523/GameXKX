import json
from datetime import datetime
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base
world,pc,wb=base._controller_and_widget()
instance=unreal.GameplayStatics.get_game_instance(world)
dev=next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer()==instance)
assert dev.is_session_active(), 'Expected the protected review session'
result=json.loads(dev.execute_json(json.dumps({'command':'snapshot.export'})))
assert result.get('ok'),result
out=Path(unreal.Paths.project_dir())/'Saved/Codex/UIGuidanceLocalization-20260910/ui-review'
original=json.loads((out/'review-original.json').read_text(encoding='utf-8'))
path=out/('review-user-actions-'+datetime.now().strftime('%Y%m%d-%H%M%S')+'.json')
path.write_text(json.dumps({'scene':result['data'],'original':original['runtime']['data'],'note':'User activity during UI review; preserve before any restore.'},ensure_ascii=False,indent=2),encoding='utf-8')
s=result['data']['state'];before=original['runtime']['data']['state']
print(json.dumps({'path':str(path),'goldBefore':before['playerGold'],'goldNow':s['playerGold'],
                 'chestsBefore':len(before['training']['ownedChestTokens']),'chestsNow':len(s['training']['ownedChestTokens'])},ensure_ascii=True))
