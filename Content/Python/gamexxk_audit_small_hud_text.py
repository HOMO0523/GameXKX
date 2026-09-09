"""Read visible text styles from the live desktop widget, without mutating game state."""
import json,sys
from pathlib import Path
import unreal
import gamexxk_probe_real_play_flow as base
world=base._get_game_world();pc=base._first_player_controller(world);w=pc.get_desktop_training_workbench_widget_for_test()
if len(sys.argv)>2 and sys.argv[2]=='open':w.open_backpack()
tree=unreal.find_object(w,'WidgetTree') or unreal.find_object(w,'DesktopTrainingWorkbenchWidgetTree')
try:root=tree.get_editor_property('root_widget')
except Exception:root=unreal.find_object(tree,'DesktopTrainingOverlayRoot')
rows=[];seen=set()
def walk(obj):
    if not obj or obj.get_path_name() in seen:return
    seen.add(obj.get_path_name())
    if obj.get_visibility() in (unreal.SlateVisibility.COLLAPSED,unreal.SlateVisibility.HIDDEN) or obj.get_render_opacity()<=.01:return
    if isinstance(obj,unreal.TextBlock):
        text=str(obj.get_text()).strip()
        if text:
            font=obj.get_editor_property('font')
            rows.append({'name':obj.get_name(),'text':text,'nominal_font_size':font.get_editor_property('size'),'font':str(font.get_editor_property('font_object'))})
    if isinstance(obj,unreal.PanelWidget):
        for i in range(obj.get_children_count()):walk(obj.get_child_at(i))
    if isinstance(obj,unreal.UserWidget):
        inner=unreal.find_object(obj,'WidgetTree')
        if inner:
            try:walk(inner.get_editor_property('root_widget'))
            except Exception:
                for name in ('InventoryRootCanvas','InventoryWindowRoot','CardTooltipFixedWidth'):walk(unreal.find_object(inner,name))
walk(root)
label=sys.argv[1] if len(sys.argv)>1 else 'current'
out=Path(unreal.Paths.project_dir())/'Saved/Codex/Hud50Readability';out.mkdir(exist_ok=True)
(out/f'{label}.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'page':label,'expanded':w.is_backpack_expanded_for_test(),'texts':len(rows),'below20':sum(r['nominal_font_size']<20 for r in rows),'file':str(out/f'{label}.json')},ensure_ascii=False))
