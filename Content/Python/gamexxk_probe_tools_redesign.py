"""Small, non-blocking phases for the real 2D tools acceptance session."""
import json
import sys
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base

world, controller, widget = base._controller_and_widget()
if not world or not widget or "L_DesktopTrainingHUD" not in world.get_path_name():
    raise RuntimeError("A canonical desktop PIE session is required")
instance = unreal.GameplayStatics.get_game_instance(world)
dev = next((obj for obj in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if obj.get_outer() == instance), None)
if not dev:
    raise RuntimeError("Development session guard is unavailable")
subsystem = widget.get_mvp_subsystem()
output = Path(unreal.Paths.project_dir()) / "Saved/Codex/ToolsRedesign-20260909"
output.mkdir(parents=True, exist_ok=True)

def command(name, args=None):
    result = json.loads(dev.execute_json(json.dumps({"command": name, "args": args or {}}, ensure_ascii=False)))
    if not result.get("ok"):
        raise RuntimeError(name + ": " + result.get("message", "failed"))
    return result

def field(obj, name):
    return next(key for key in obj if key.casefold() == name.casefold())

def write(name, value):
    (output / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")

def walk(node):
    if not node:
        return
    yield node
    if isinstance(node, unreal.PanelWidget):
        for index in range(node.get_children_count()):
            yield from walk(node.get_child_at(index))

def observe():
    tree = unreal.find_object(widget, "DesktopTrainingWorkbenchWidgetTree") or unreal.find_object(widget, "WidgetTree")
    canvas = unreal.find_object(tree, "DesktopTrainingReferenceCanvas") if tree else None
    controls = {}
    for node in walk(canvas):
        name = node.get_name()
        if not name.startswith("Tool"):
            continue
        row = {"visible": str(node.get_visibility()), "enabled": node.get_is_enabled()}
        slot = node.get_editor_property("slot")
        if isinstance(slot, unreal.CanvasPanelSlot):
            position, size = slot.get_position(), slot.get_size()
            row["rect"] = [position.x, position.y, size.x, size.y]
        if isinstance(node, unreal.TextBlock):
            row["text"] = str(node.get_text())
            desired = node.get_desired_size()
            row["desired_size"] = [desired.x, desired.y]
            row["font_size"] = node.get_editor_property("font").get_editor_property("size")
        elif isinstance(node, unreal.Button):
            row["text"] = " | ".join(str(child.get_text()) for child in walk(node) if isinstance(child, unreal.TextBlock))
        controls[name] = row
    snapshot = command("snapshot.export")["data"]
    state = snapshot["state"]
    collection = state[field(state, "equipmentCollection")]
    return {"world": world.get_path_name(), "session_active": dev.is_session_active(),
            "carrying": widget.is_carrying_item_for_test(),
            "tool_items": [str(widget.get_tool_slot_item_id_for_test(index)) for index in range(9)],
            "occupied": widget.get_occupied_tool_slot_count_for_test(), "controls": controls,
            "inventory": state[field(state, "inventory")],
            "equipment": collection[field(collection, "equipmentInstances")],
            "pending_reforge": collection[field(collection, "pendingReforge")]}

phase = sys.argv[1] if len(sys.argv) > 1 else "state"
result = {"phase": phase}
if phase == "fixture":
    if dev.is_session_active():
        raise RuntimeError("Another development session must be restored before this fixture")
    command("session.begin")
    write("original-runtime.json", command("snapshot.export")["data"])
    # StartNewGame respects the active development write guard, retaining player saves.
    assert subsystem.start_game()
    scene = command("snapshot.export")["data"]
    state = scene["state"]
    talents = state[field(state, "talents")]
    talents[field(talents, "minimumBackpackCapacity")] = 200
    command("snapshot.import", {"scene": scene})
    fixture = {}
    for tag, quality, quantity in (("common", 1, 9), ("legendary", 4, 9), ("reforge", 3, 1), ("socket", 10, 1)):
        fixture[tag] = command("equipment.create", {"id": "Equipment.PoJun.Accessory", "character": "Player",
            "quality": quality, "level": 10, "quantity": quantity, "equip": False, "gem": "none"})["data"]["created_ids"]
    for item, quantity in (("Item.Gem.Attack.Common", 4), ("Item.Gem.Defense.Common", 5),
                           ("Item.Gem.Attack.Rare", 2), ("Item.EnhancementStone", 20), ("Item.RefinementSand", 20)):
        command("item.give", {"id": item, "quantity": quantity})
    write("live-fixture.json", fixture)
    widget.open_backpack()
    widget.handle_desktop_action_for_test(3)
    result["fixture"] = fixture
elif phase == "action":
    assert dev.is_session_active()
    widget.handle_desktop_action_for_test(int(sys.argv[2]))
    result["action"] = int(sys.argv[2])
elif phase == "place":
    assert dev.is_session_active()
    if not widget.is_backpack_expanded_for_test():
        widget.open_backpack()
        widget.handle_desktop_action_for_test(3)
    tag, target = sys.argv[2], int(sys.argv[3])
    fixture = json.loads((output / "live-fixture.json").read_text(encoding="utf-8"))
    entry_id = fixture[tag][int(sys.argv[4]) if len(sys.argv) > 4 else 0] if tag in fixture else tag
    state = subsystem.get_runtime_state_copy()
    container = sys.argv[5] if len(sys.argv) > 5 else "backpack"
    slots = state.desktop_inventory.warehouse_slots if container == "warehouse" else state.desktop_inventory.backpack_slots
    source = next(index for index, entry in enumerate(slots) if str(entry.entry_id) == entry_id)
    if container == "warehouse":
        assert source < 20, "This fixture expects the first warehouse page"
        if not widget.is_warehouse_panel_open_for_test():
            widget.handle_desktop_action_for_test(0)
        widget.handle_desktop_action_for_test(100 + source)
        assert widget.is_carrying_item_for_test()
    else:
        assert widget.pick_up_backpack_slot_for_test(source)
    widget.handle_desktop_action_for_test(300 + target)
    result.update({"entry_id": entry_id, "source": source, "container": container, "target": target})
elif phase == "to-warehouse":
    assert dev.is_session_active() and not widget.is_carrying_item_for_test()
    if not widget.is_warehouse_panel_open_for_test():
        widget.handle_desktop_action_for_test(0)
    source = widget.find_backpack_item_slot_for_test(unreal.Name(sys.argv[2]))
    assert source >= 0 and widget.right_click_backpack_slot_for_test(source)
    result["entry_id"] = sys.argv[2]
elif phase == "restore":
    result["restore"] = command("session.restore")
elif phase == "scale":
    percent = int(sys.argv[2])
    assert percent in (50, 75)
    widget.handle_desktop_action_for_test(651 if percent == 50 else 656)
    result["scale"] = percent
elif phase == "diagnose":
    state = subsystem.get_runtime_state_copy()
    result.update({"subsystem": subsystem.get_path_name(), "widget": widget.get_path_name(),
        "expanded": widget.is_backpack_expanded_for_test(), "carry": widget.is_carrying_item_for_test(),
        "slots": [[i, str(entry.entry_id)] for i, entry in enumerate(state.desktop_inventory.backpack_slots)],
        "fixture": json.loads((output / "live-fixture.json").read_text(encoding="utf-8"))})
elif phase == "state":
    result = observe()
    filename = sys.argv[2] if len(sys.argv) > 2 else "live-state.json"
    assert Path(filename).name == filename and filename.endswith(".json")
    write(filename, result)
else:
    raise RuntimeError("Unknown phase")
print(json.dumps(result, ensure_ascii=False))
