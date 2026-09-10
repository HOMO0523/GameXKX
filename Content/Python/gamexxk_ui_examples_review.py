"""Bounded fixtures within the UI review's already protected Dev session."""
import json
import sys
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base
world,pc,wb=base._controller_and_widget()
instance=unreal.GameplayStatics.get_game_instance(world)
dev=next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer()==instance)
assert dev.is_session_active(), 'Start the restorable UI review first'
def command(name,args=None):
    result=json.loads(dev.execute_json(json.dumps({'command':name,'args':args or {}})))
    assert result.get('ok'),result
    return result
mode=sys.argv[1]
if mode=='items':
    created=command('equipment.create',{'id':'Equipment.PoJun.Shoes','character':'Player','level':1,'quality':2,'quantity':1,'equip':False,'gem':'none'})
    wb.open_backpack();wb.handle_desktop_action_for_test(64)
    state=wb.get_mvp_subsystem().get_runtime_state_copy()
    rows=[{'index':i,'id':str(entry.entry_id),'equipment':entry.equipment_instance} for i,entry in enumerate(state.desktop_inventory.backpack_slots) if str(entry.entry_id)!='None']
    output={'created':created,'slots':rows}
elif mode=='battle':
    output=command('battle.start',{'stage':'Training.Normal.1-1','encounter':1,'seed':20260910})
    pc.refresh_player_flow_widgets_from_state()
elif mode=='return':
    output=command('battle.return')
    pc.refresh_player_flow_widgets_from_state()
elif mode in ('equipment','deck'):
    wb.open_backpack();wb.handle_desktop_action_for_test(64);wb.handle_desktop_action_for_test(63)
    selected=[]
    for inventory in list(unreal.ObjectIterator(unreal.GameXXKInventoryWindowWidget)):
        if inventory.get_parent() and inventory.get_owning_player()==pc:
            tab=unreal.GameXXKCharacterBackpackTab.DECK if mode=='deck' else unreal.GameXXKCharacterBackpackTab.EQUIPMENT
            selected.append({'widget':inventory.get_path_name(),'opened':inventory.open_character_backpack_tab_for_test(tab)})
    assert len(selected)==1 and selected[0]['opened'],selected
    output={'inventory':selected}
elif mode=='deck-buttons':
    output={'buttons':[],'cardTitles':[]}
    for inventory in list(unreal.ObjectIterator(unreal.GameXXKInventoryWindowWidget)):
        if not inventory.get_parent() or inventory.get_owning_player()!=pc:continue
        tree=unreal.find_object(inventory,'WidgetTree')
        if not tree:continue
        for name in ('InventoryDeckDensityButton','InventoryDeckExpandButton','InventoryDeckCollapseButton'):
            button=unreal.find_object(tree,name)
            if not button or not button.is_visible():continue
            g=button.get_cached_geometry();size=unreal.SlateLibrary.get_local_size(g)
            center=unreal.SlateLibrary.local_to_absolute(g,unreal.Vector2D(size.x/2,size.y/2))
            output['buttons'].append({'name':name,'center':[center.x,center.y],'size':[size.x,size.y]})
        names={r['en'] for r in json.loads((Path(__file__).resolve().parents[2]/'Content/Localization/GameXXK/card-short-names.json').read_text(encoding='utf-8'))['cards']}
        for text in unreal.ObjectIterator(unreal.TextBlock):
            if text.get_outer()!=tree or str(text.get_text()) not in names:continue
            font=text.get_editor_property('font');fill=font.get_editor_property('font_material');outline=font.get_editor_property('outline_settings').get_editor_property('outline_material')
            output['cardTitles'].append({'name':str(text.get_text()),'fontSize':font.get_editor_property('size'),
                'fill':fill.get_path_name() if fill else '', 'outline':outline.get_path_name() if outline else ''})
elif mode=='snapshot':output=command('snapshot.export')
else:raise ValueError(mode)
path=Path(unreal.Paths.project_dir())/'Saved/Codex/UIGuidanceLocalization-20260910/ui-review'/('examples-'+mode+'.json')
path.write_text(json.dumps(output,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'mode':mode,'path':str(path)}))
