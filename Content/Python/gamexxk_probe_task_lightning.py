import builtins,json
from pathlib import Path
import unreal
s=getattr(builtins,'_partner_task_lightning_record',{})
rows=s.get('rows',[])
Path(unreal.Paths.project_dir(),'Saved/Codex/partner-task-lightning-live.json').write_text(json.dumps(rows,indent=2))
print(json.dumps({'samples':len(rows),'ultimate_seen':any('HIT_TEST_INVISIBLE' in r['ultimate_visible'] for r in rows),'last':rows[-1] if rows else None}))
