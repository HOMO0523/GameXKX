"""Visual guide inspection in an explicitly protected temporary session."""
import json
import sys
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base

world,pc,wb=base._controller_and_widget()
assert world and wb
instance=unreal.GameplayStatics.get_game_instance(world)
dev=next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer()==instance)
assert dev.is_session_active(), 'Protect the current player before visual inspection'
mode=sys.argv[1] if len(sys.argv)>1 else 'inspect'
if mode=='first':
    scene=json.loads(dev.execute_json(json.dumps({'command':'snapshot.export','args':{}})))['data']
    progress=scene['state']['guideProgress']
    progress['completedGuideStepIds']=[v for v in progress['completedGuideStepIds'] if not str(v).startswith('UI.Basics.V1.')]
    result=json.loads(dev.execute_json(json.dumps({'command':'snapshot.import','args':{'scene':scene}})))
    assert result['ok'],result
    wb.open_workbench();wb.open_backpack()
    wb.handle_desktop_action_for_test(652);wb.handle_desktop_action_for_test(662)
report=[]
for w in unreal.ObjectIterator(unreal.GameXXKInterfaceHelpWidget):
    outer=w.get_outer()
    if not outer or w.get_name()!='DesktopInterfaceHelp':continue
    g=w.get_cached_geometry();size=unreal.SlateLibrary.get_local_size(g)
    owner=w.get_owning_player()
    report.append({'path':w.get_path_name(),'owner':owner.get_path_name() if owner else None,'visible':w.is_visible(),
                   'parent':w.get_parent().get_name() if w.get_parent() else None,'size':[size.x,size.y],
                   'tickFrequency':str(w.get_editor_property('tick_frequency'))})
out=Path(unreal.Paths.project_dir())/'Saved/Codex/UIGuidanceLocalization-20260910/ui-review/fullscreen-guide-live.json'
out.write_text(json.dumps({'mode':mode,'guides':report},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'mode':mode,'guides':report},ensure_ascii=True))
