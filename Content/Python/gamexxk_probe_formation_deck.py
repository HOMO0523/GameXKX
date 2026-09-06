"""Navigate the real desktop formation/shared deck UI for visual verification."""
import json
import sys
import unreal
import gamexxk_probe_training_visual_mvp as base

world, controller, workbench = base._controller_and_widget()
if not world or not workbench:
    raise RuntimeError("Start desktop PIE first")

def owned(widget):
    owner = widget.get_outer()
    while owner and owner != workbench:
        owner = owner.get_outer()
    return owner == workbench

mode = sys.argv[1] if len(sys.argv) > 1 else "inspect"
if mode == "formation":
    button = next(x for x in unreal.ObjectIterator(unreal.Button)
                  if owned(x) and x.get_name() == "BottomNavigationButton_1")
    button.call_method("HandleClicked")
elif mode == "action":
    button = next(x for x in unreal.ObjectIterator(unreal.Button) if owned(x) and x.get_name()==sys.argv[2])
    button.call_method("HandleClicked")
elif mode == "formation-deck":
    workbench.open_formation_deck_for_test(sys.argv[2] if len(sys.argv)>2 else "Player")
elif mode in ("density", "expand", "collapse"):
    inventory = next(x for x in unreal.ObjectIterator(unreal.GameXXKInventoryWindowWidget) if owned(x))
    if mode == "density": inventory.toggle_deck_density()
    else: inventory.set_deck_expanded(mode=="expand")
elif mode == "backpack-deck":
    workbench.open_backpack()
    inventory = next(x for x in unreal.ObjectIterator(unreal.GameXXKInventoryWindowWidget) if owned(x))
    inventory.open_character_backpack_tab_for_test(unreal.GameXXKCharacterBackpackTab.DECK)
result={"ok": True, "mode": mode,"page": str(workbench.get_active_center_page_for_test())}
tree=unreal.find_object(workbench,'DesktopTrainingWorkbenchWidgetTree') or unreal.find_object(workbench,'WidgetTree')
inventory=unreal.find_object(tree,'EmbeddedApprovedBackpack') if tree else None
if inventory:
    result['expanded']=inventory.is_deck_expanded_for_test()
    result['tab']=str(inventory.get_active_character_backpack_tab_for_test())
    inner=unreal.find_object(inventory,'InventoryWindowWidgetTree') or unreal.find_object(inventory,'WidgetTree')
    result['widgets']={}
    for name in ('InventoryWindowFrame','InventoryWindowFrameCanvas','InventoryHeroDeckPanel','InventoryDeckExpandedBackdrop'):
        widget=unreal.find_object(inner,name) if inner else None
        if widget:
            result['widgets'][name]={'opacity':widget.get_render_opacity(),'visibility':str(widget.get_visibility()),'parent':widget.get_parent().get_name() if widget.get_parent() else None}
print(json.dumps(result,ensure_ascii=False))
