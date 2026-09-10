"""Read-only snapshot of the actual Academy UI and battle, with no synthetic clicks."""
import json
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base

world,pc,wb=base._controller_and_widget()
assert world and wb
instance=unreal.GameplayStatics.get_game_instance(world)
dev=next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer()==instance)
export=json.loads(dev.execute_json(json.dumps({'command':'snapshot.export','args':{}})))
assert export['ok'],export
scene=export['data'];state=scene['state'];battle=state['cardRun']['activeBattle']
text=[]
for widget in unreal.ObjectIterator(unreal.TextBlock):
    if widget.get_name() not in ('AcademyLessonTitle','AcademyMechanism','AcademyAction','AcademyObjective'):continue
    owner=widget.get_outer()
    if not isinstance(owner,unreal.GameXXKBattleBoardWidget) or owner.get_owning_player()!=pc:continue
    if not widget.get_parent():continue
    g=widget.get_cached_geometry();p=unreal.SlateLibrary.local_to_absolute(g,unreal.Vector2D(0,0));s=unreal.SlateLibrary.get_local_size(g)
    text.append({'name':widget.get_name(),'text':str(widget.get_text()),'visible':widget.is_visible(),'position':[p.x,p.y],'size':[s.x,s.y]})
record={'map':state['currentMapId'],'screen':state['screen'],'devSession':dev.is_session_active(),
        'activeBattle':state['cardRun']['bHasActiveCardBattle'],'texts':text,'battle':battle,
        'guideProgress':state['guideProgress'],'scene':scene}
out=Path(unreal.Paths.project_dir())/'Saved/Codex/UIGuidanceLocalization-20260910/ui-review/academy-live-probe.json'
out.write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:record[k] for k in ['map','screen','devSession','activeBattle','texts']},ensure_ascii=True))
