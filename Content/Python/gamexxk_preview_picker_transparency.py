"""Preview or inspect the real picker layering without editing source art."""
import json
import sys
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base

world,controller,workbench=base._controller_and_widget()
if not world or not workbench:raise RuntimeError('Desktop PIE is required')
tree=unreal.find_object(workbench,'DesktopTrainingWorkbenchWidgetTree') or unreal.find_object(workbench,'WidgetTree')
root=unreal.find_object(tree,'DesktopTrainingReferenceCanvas')
def walk(widget):
    yield widget
    if isinstance(widget,unreal.PanelWidget):
        for i in range(widget.get_children_count()):yield from walk(widget.get_child_at(i))
active={w.get_name():w for w in walk(root)}
paper=active.get('CharacterPickerPage')
inventory=active.get('EmbeddedApprovedBackpack')
mode=sys.argv[1] if len(sys.argv)>1 else 'inspect'
if mode=='preview':
    if not paper:raise RuntimeError('Open partner or NPC selection first')
    inventory.set_color_and_opacity(unreal.LinearColor(.50,.45,.36,1.0))
    inventory.set_render_opacity(.72)
    paper.set_render_opacity(1.0)
    host=active['BackpackPanel'].get_editor_property('slot')
    slot=paper.get_editor_property('slot')
    slot.set_position(host.get_position())
    slot.set_size(host.get_size())
slot=(paper or active['BackpackPanel']).get_editor_property('slot');p,s=slot.get_position(),slot.get_size()
result={'mode':mode,'map':world.get_name(),'picker_open':bool(paper),'input_guard_opacity':paper.get_render_opacity() if paper else None,'backpack_opacity':inventory.get_render_opacity(),'extra_paper_children':paper.get_children_count() if paper else 0,'panel_rect':[p.x,p.y,s.x,s.y],
    'backpack_tint':str(inventory.get_editor_property('color_and_opacity')),'controls':[],'portraits':[]}
for name,w in active.items():
    if name.startswith('CharacterRoster') and name.endswith('Button') or name.startswith('CharacterRosterPortraitButton_') or name=='CharacterPickerBack':
        ws=w.get_editor_property('slot')
        if isinstance(ws,unreal.CanvasPanelSlot):
            q,z=ws.get_position(),ws.get_size()
            contained=q.x>=p.x and q.y>=p.y and q.x+z.x<=p.x+s.x+.01 and q.y+z.y<=p.y+s.y+.01
            result['controls'].append({'name':name,'rect':[q.x,q.y,z.x,z.y],'inside_translucent_panel':contained})
    if name.startswith('CharacterRosterMemberPortrait_'):
        ws=w.get_editor_property('slot');q,z=ws.get_position(),ws.get_size()
        result['portraits'].append({'name':name,'rect':[q.x,q.y,z.x,z.y]})
out=Path(unreal.Paths.get_project_file_path()).resolve().parent/'Saved/Codex/PickerTransparency-20260906'
(out/(mode+'-state.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False))
