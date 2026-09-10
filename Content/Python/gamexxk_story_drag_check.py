"""Inspect cached story-panel translation while a guarded live drag is exercised."""
import json
from pathlib import Path
import sys
import unreal
root=Path(__file__).resolve().parents[2];out=root/'Saved/StorySystem/FullPlaythrough'
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
pc=unreal.GameplayStatics.get_player_controller(world,0);instance=unreal.GameplayStatics.get_game_instance(world)
wb=pc.get_desktop_training_workbench_widget_for_test()
story=next(x for x in unreal.ObjectIterator(unreal.GameXXKMainStorySubsystem) if x.get_outer()==instance)
dev=next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer()==instance)
def command(name):
    r=json.loads(dev.execute_json(json.dumps({'schema':1,'command':name,'args':{}})))
    assert r['ok'],r
    return r
mode=sys.argv[1] if len(sys.argv)>1 else 'observe'
if mode=='prepare':
    before=command('snapshot.export');assert not before['session_active']
    (out/'drag-player-before.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
    command('session.begin')
    wb.open_backpack();wb.handle_desktop_action_for_test(2100)
    assert story.start_task(unreal.Name('S00-06'))
elif mode=='restore':
    command('session.restore')
    before=json.loads((out/'drag-player-before.json').read_text(encoding='utf-8'))
    after=command('snapshot.export')
    assert before['data']['state']==after['data']['state']
    print(json.dumps({'restored':True,'dev_session':False}));raise SystemExit
def vec(v):return [float(v.x),float(v.y)]
body=wb.get_desktop_body_offset_for_test()
strip=wb.get_desktop_strip_top_left_for_test()
panels=[p for p in unreal.ObjectIterator(unreal.GameXXKMainStoryPanelWidget) if p.get_owning_player()==pc and p.get_parent()]
rows=[]
import gamexxk_probe_real_play_flow as geometry
for panel in panels:
    translation=panel.get_editor_property('render_transform').translation
    rows.append({'name':panel.get_name(),'translation':vec(translation),'matches_body':abs(translation.x-body.x)<.1 and abs(translation.y-body.y)<.1,
                 'rect':geometry._screen_rect(world,panel)})
report={'body':vec(body),'strip':vec(strip),'panels':rows,'dev_session':dev.is_session_active(),'layout_builds':wb.get_programmatic_layout_build_count_for_test_blueprint()}
(out/('drag-'+mode+'.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False))
