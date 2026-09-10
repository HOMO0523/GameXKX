import json
import unreal
import gamexxk_probe_training_visual_mvp as base
world,pc,wb=base._controller_and_widget()
state=wb.get_mvp_subsystem().get_runtime_state_copy()
tree=unreal.find_object(wb,'DesktopTrainingWorkbenchWidgetTree') or unreal.find_object(wb,'WidgetTree')
button=unreal.find_object(tree,'ToolInputSlot_0')
result={'toolActive':wb.is_tools_panel_active_for_test(),'occupied':wb.get_occupied_tool_slot_count_for_test(),
        'firstToolItem':str(wb.get_tool_slot_item_id_for_test(0)),
        'firstBag':{'id':str(state.desktop_inventory.backpack_slots[0].entry_id),'equipment':state.desktop_inventory.backpack_slots[0].equipment_instance}}
if button:
    g=button.get_cached_geometry();size=unreal.SlateLibrary.get_local_size(g)
    a=unreal.SlateLibrary.local_to_absolute(g,unreal.Vector2D(0,0));b=unreal.SlateLibrary.local_to_absolute(g,size)
    result.update(buttonEnabled=button.get_is_enabled(),buttonVisible=button.is_visible(),bounds=[a.x,a.y,b.x,b.y])
print(json.dumps(result,ensure_ascii=True))
