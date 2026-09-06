"""Inspect the current mounted backpack, picker and ink-scroll surfaces in real PIE."""
import json
import sys
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base

world, controller, workbench = base._controller_and_widget()
if not world or not workbench:
    raise RuntimeError('A real desktop PIE world is required')
tree=unreal.find_object(workbench,'DesktopTrainingWorkbenchWidgetTree') or unreal.find_object(workbench,'WidgetTree')
root=unreal.find_object(tree,'DesktopTrainingReferenceCanvas')

def walk(widget):
    yield widget
    if isinstance(widget,unreal.PanelWidget):
        for i in range(widget.get_children_count()):yield from walk(widget.get_child_at(i))

active={w.get_name():w for w in walk(root)}
inventory=active.get('EmbeddedApprovedBackpack')
mode=sys.argv[1] if len(sys.argv)>1 else 'inspect'
if mode in ('attributes','equipment','deck'):
    tab={'attributes':unreal.GameXXKCharacterBackpackTab.ATTRIBUTES,'equipment':unreal.GameXXKCharacterBackpackTab.EQUIPMENT,'deck':unreal.GameXXKCharacterBackpackTab.DECK}[mode]
    inventory.open_character_backpack_tab_for_test(tab)
inner_tree=unreal.find_object(inventory,'InventoryWindowWidgetTree') or unreal.find_object(inventory,'WidgetTree')
inner_root=unreal.find_object(inner_tree,'InventoryWindowRoot')
inner={w.get_name():w for w in walk(inner_root)}

def metrics(widget):
    result={'name':widget.get_name(),'class':widget.get_class().get_name(),'visibility':str(widget.get_visibility())}
    if isinstance(widget,unreal.TextBlock):result['text']=str(widget.get_text())
    slot=widget.get_editor_property('slot')
    if isinstance(slot,unreal.CanvasPanelSlot):
        p,s=slot.get_position(),slot.get_size();result.update(position=[p.x,p.y],size=[s.x,s.y])
    try:
        g=widget.get_cached_geometry();p=unreal.SlateLibrary.local_to_absolute(g,unreal.Vector2D(0,0));s=unreal.SlateLibrary.get_absolute_size(g)
        result.update(absolute_position=[p.x,p.y],absolute_size=[s.x,s.y],center_x=p.x+s.x*.5)
    except Exception as exc:result['geometry_error']=str(exc)
    return result

result={'mode':mode,'world':world.get_name(),'owner':str(inventory.get_configured_character_id_for_test()),
    'tab':str(inventory.get_active_character_backpack_tab_for_test()),'summary':str(inventory.get_character_tab_body_text_for_test()),'widgets':{}}
names=['BackpackPanel','CharacterRosterHeroButton','CharacterRosterCompanionButton','CharacterRosterNpcButton','CharacterPickerBack','CharacterPickerPage','WarehouseSlotScrollBox','BackpackSortButton']
for name in names:
    if name in active:result['widgets'][name]=metrics(active[name])
for name in ('InventoryCharacterTab_0','InventoryCharacterTab_1','InventoryCharacterTab_2','InventoryCharacterAttributeTitle','InventoryCharacterLevelText','InventoryCharacterIdentityText','InventoryApplyHeroDeckButton','InventoryBackpackScrollbarThumb'):
    if name in inner:result['widgets'][name]=metrics(inner[name])
result['cards']=[metrics(w) for name,w in active.items() if name.startswith('CharacterRosterPortraitButton_')]
panel=active['BackpackPanel'].get_editor_property('slot')
origin,host=panel.get_position(),panel.get_size()
reference=active['EmbeddedBackpackPaperReference']
rw,rh=reference.get_editor_property('width_override'),reference.get_editor_property('height_override')
scale=min(host.x/rw,host.y/rh)
offset=inventory.get_editor_property('slot').get_position()
result['tab_alignment']=[]
for i,name in enumerate(['CharacterRosterHeroButton','CharacterRosterCompanionButton','CharacterRosterNpcButton']):
    top=inner['InventoryCharacterTab_'+str(i)].get_editor_property('slot')
    bottom=active[name].get_editor_property('slot')
    tp,ts,bp,bs=top.get_position(),top.get_size(),bottom.get_position(),bottom.get_size()
    upper=origin.x+(host.x-rw*scale)*.5+(offset.x+tp.x+ts.x*.5)*scale
    lower=bp.x+bs.x*.5
    result['tab_alignment'].append({'column':i,'top_center_in_reference':upper,'bottom_center_in_reference':lower,'delta':upper-lower})
    assert abs(upper-lower)<.1,'Backpack tab centers differ'
result['scrolls']={}
for name,w in {**active,**inner}.items():
    if isinstance(w,unreal.ScrollBox):result['scrolls'][name]={'offset':w.get_scroll_offset(),'end':w.get_scroll_offset_of_end(),'clipping':str(w.get_editor_property('clipping')),'visibility':str(w.get_visibility())}
grid=inner.get('InventoryHeroDeckGrid')
if grid:
    result['deck_cells']=[metrics(grid.get_child_at(i)) for i in range(grid.get_children_count()) if grid.get_child_at(i).get_visibility()!=unreal.SlateVisibility.COLLAPSED]
out=Path(unreal.Paths.get_project_file_path()).resolve().parent/'Saved/Codex/BackpackCardRow-20260906'
out.mkdir(exist_ok=True);(out/'last-layout-state.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False))
