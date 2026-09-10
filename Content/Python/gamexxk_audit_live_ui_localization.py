"""Read-only text/font snapshot of the current player's UMG widgets."""
import json
import re
from pathlib import Path
import unreal

world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world,'A current game world is required; this probe never starts PIE'
controller=unreal.GameplayStatics.get_player_controller(world,0)
rows=[]
for widget in unreal.ObjectIterator(unreal.TextBlock):
    owner=widget.get_outer()
    belongs=False
    while owner:
        if isinstance(owner,unreal.UserWidget) and owner.get_owning_player()==controller:
            belongs=True
            break
        owner=owner.get_outer()
    if not belongs:continue
    text=str(widget.get_text())
    if not text:continue
    visible=widget.is_visible()
    parent=widget.get_parent()
    while parent:
        visible=visible and parent.is_visible()
        parent=parent.get_parent()
    font=widget.get_editor_property('font')
    resource=font.get_editor_property('font_object')
    rows.append({'name':widget.get_name(),'owner':owner.get_name(),'text':text,'cjk':bool(re.search(r'[\u3400-\u9fff]',text)),
                 'visible':visible,'font':resource.get_path_name() if resource else '',
                 'font_size':font.get_editor_property('size')})
out=Path(unreal.Paths.project_dir())/'Saved/Codex/UIGuidanceLocalization-20260910/ui-review/live-text-fonts.json'
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps({'world':world.get_name(),'texts':rows},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'path':str(out),'texts':len(rows),'visible_cjk':sum(r['visible'] and r['cjk'] for r in rows)},ensure_ascii=True))
