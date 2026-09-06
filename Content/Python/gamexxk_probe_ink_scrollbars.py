"""Read live scrollbar geometry and the offsets of their actual visible containers."""
import json
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base

world, controller, workbench=base._controller_and_widget()
if not world or not workbench:raise RuntimeError('Desktop PIE is required')
tree=unreal.find_object(workbench,'DesktopTrainingWorkbenchWidgetTree') or unreal.find_object(workbench,'WidgetTree')
root=unreal.find_object(tree,'DesktopTrainingReferenceCanvas')

def walk(widget):
    yield widget
    if isinstance(widget,unreal.PanelWidget):
        for i in range(widget.get_children_count()):yield from walk(widget.get_child_at(i))

widgets={w.get_name():w for w in walk(root)}
for name,tree_name,root_name in [('PermanentTalentTreeWidget','TalentWidgetTree','TalentTreeRoot'),('EmbeddedApprovedBackpack','InventoryWindowWidgetTree','InventoryWindowRoot')]:
    owner=widgets.get(name)
    if owner:
        inner=unreal.find_object(owner,tree_name) or unreal.find_object(owner,'WidgetTree')
        inner_root=unreal.find_object(inner,root_name)
        if inner_root:widgets.update({w.get_name():w for w in walk(inner_root)})
result={}
for name,w in widgets.items():
    if not isinstance(w,(unreal.ScrollBar,unreal.ScrollBox)):continue
    row={'class':w.get_class().get_name(),'visibility':str(w.get_visibility())}
    if isinstance(w,unreal.ScrollBox):row.update(offset=w.get_scroll_offset(),end=w.get_scroll_offset_of_end())
    else:
        g=w.get_cached_geometry();p=unreal.SlateLibrary.local_to_absolute(g,unreal.Vector2D(0,0));s=unreal.SlateLibrary.get_absolute_size(g)
        row.update(position=[p.x,p.y],size=[s.x,s.y])
    result[name]=row
out=Path(unreal.Paths.get_project_file_path()).resolve().parent/'Saved/Codex/BackpackCardRow-20260906'
(out/'last-ink-state.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False))
