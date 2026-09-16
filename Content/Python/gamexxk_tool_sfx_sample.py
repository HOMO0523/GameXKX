"""Prepare reversible tool scenes and observe sound cues for the audio handoff sample."""
import argparse
import json
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
OUT = ROOT / "Saved/Codex/ToolSfxRequirementSample-20260914"
OUT.mkdir(parents=True, exist_ok=True)


def write(name, data):
    (OUT / name).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and "L_DesktopTrainingHUD" in world.get_path_name(), "Canonical 2D PIE required"
pc = unreal.GameplayStatics.get_player_controller(world, 0)
wb = pc.get_desktop_training_workbench_widget_for_test()
assert wb
mvp = wb.get_mvp_subsystem()
gi = unreal.GameplayStatics.get_game_instance(world)
dev = next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer() == gi)


def command(name, args=None):
    result = json.loads(dev.execute_json(json.dumps({"command": name, "args": args or {}}, ensure_ascii=False)))
    assert result.get("ok"), result
    return result.get("data")


def tree_walk(node):
    if node:
        yield node
        if isinstance(node, unreal.PanelWidget):
            for i in range(node.get_children_count()):
                yield from tree_walk(node.get_child_at(i))


def snapshot():
    tree = unreal.find_object(wb, "DesktopTrainingWorkbenchWidgetTree") or unreal.find_object(wb, "WidgetTree")
    canvas = unreal.find_object(tree, "DesktopTrainingReferenceCanvas")
    controls = {}
    for obj in tree_walk(canvas):
        name = obj.get_name()
        if not name.startswith("Tool"):
            continue
        row = {"visible": str(obj.get_visibility()), "enabled": obj.get_is_enabled()}
        if isinstance(obj, unreal.TextBlock):
            row["text"] = str(obj.get_text())
        elif isinstance(obj, unreal.Button):
            row["text"] = " | ".join(str(c.get_text()) for c in tree_walk(obj) if isinstance(c, unreal.TextBlock))
        controls[name] = row
    return {"wall_time": time.time(), "monotonic": time.monotonic(), "world": world.get_path_name(),
            "dev_active": dev.is_session_active(), "muted": wb.is_muted_for_test(),
            "occupied": wb.get_occupied_tool_slot_count_for_test(),
            "tool_ids": [str(wb.get_tool_slot_item_id_for_test(i)) for i in range(9)],
            "audio": json.loads(unreal.GameXXKSfxLibrary.get_diagnostics(world)), "controls": controls}


def place(entry_id):
    state = mvp.get_runtime_state_copy()
    slot = next(i for i, item in enumerate(state.desktop_inventory.backpack_slots) if str(item.entry_id) == entry_id)
    assert wb.pick_up_backpack_slot_for_test(slot)
    wb.handle_desktop_action_for_test(300)
    assert wb.get_occupied_tool_slot_count_for_test() == 1


p = argparse.ArgumentParser()
p.add_argument("phase", choices=["init", "state", "prepare", "action", "record-start", "record-stop", "restore", "capture-settings"])
p.add_argument("--scene", default="enhance", choices=["enhance", "combine", "reforge"])
p.add_argument("--id", type=int, default=309)
p.add_argument("--name", default="capture")
a = p.parse_args()
if a.phase == "init":
    assert not (OUT / "runtime-before.json").exists(), "This v001 evidence already exists; use a new evidence directory for another session"
    assert not dev.is_session_active(), "Do not replace another temporary session"
    write("ui-before.json", {"muted": wb.is_muted_for_test()})
    command("session.begin")
    write("runtime-before.json", command("snapshot.export"))
    assert mvp.start_game()
    fixture = {}
    for tag, quality, quantity in (("common", 1, 9), ("reforge", 3, 1)):
        fixture[tag] = command("equipment.create", {"id": "Equipment.PoJun.Accessory", "character": "Player",
            "quality": quality, "level": 10, "quantity": quantity, "equip": False, "gem": "none"})["created_ids"]
    for name in ("Item.EnhancementStone", "Item.RefinementSand"):
        command("item.give", {"id": name, "quantity": 30})
    write("fixture.json", fixture)
    wb.open_backpack()
    if not wb.is_tools_panel_active_for_test():
        wb.handle_desktop_action_for_test(3)
    if wb.is_muted_for_test():
        wb.handle_desktop_action_for_test(17)
elif a.phase == "prepare":
    assert dev.is_session_active()
    if wb.is_carrying_item_for_test():
        assert wb.cancel_carried_item_for_test()
    fixture = json.loads((OUT / "fixture.json").read_text(encoding="utf-8"))
    wb.set_tool_mode_for_test(unreal.GameXXKDesktopToolMode.DISMANTLE)
    # Switching between incompatible modes releases existing reservations.
    wb.set_tool_mode_for_test(unreal.GameXXKDesktopToolMode.COMBINE)
    # Category changes explicitly release all tool reservations. Cancelling a
    # carried tool item instead returns it to the tool cell, so is not a clear.
    wb.handle_desktop_action_for_test(310)
    wb.handle_desktop_action_for_test(310)
    assert wb.get_occupied_tool_slot_count_for_test() == 0, "Tool reservations were not released"
    if a.scene == "combine":
        wb.handle_desktop_action_for_test(311)
        assert wb.get_occupied_tool_slot_count_for_test() == 9
    else:
        wb.set_tool_mode_for_test(unreal.GameXXKDesktopToolMode.ENHANCE if a.scene == "enhance" else unreal.GameXXKDesktopToolMode.REFORGE)
        if wb.get_occupied_tool_slot_count_for_test():
            assert wb.pick_up_tool_slot_for_test(0)
            assert wb.cancel_carried_item_for_test()
        place(fixture["reforge"][0])
elif a.phase == "action":
    assert dev.is_session_active()
    before = snapshot()
    wb.handle_desktop_action_for_test(a.id)
    write(a.name + "-action.json", {"action": a.id, "before": before, "after": snapshot()})
elif a.phase == "record-start":
    assert dev.is_session_active()
    unreal.GameXXKSfxLibrary.reset_diagnostics(world)
    before = time.time()
    unreal.AudioMixerLibrary.start_recording_output(world, 60.0)
    write(a.name + "-audio-start.json", {"wall_before": before, "wall_after": time.time(), "state": snapshot()})
elif a.phase == "record-stop":
    unreal.AudioMixerLibrary.stop_recording_output(world, unreal.AudioRecordingExportType.WAV_FILE, a.name, str(OUT))
    write(a.name + "-audio-stop.json", snapshot())
elif a.phase == "restore":
    command("session.restore")
    ui = json.loads((OUT / "ui-before.json").read_text(encoding="utf-8"))
    if ui["muted"] != wb.is_muted_for_test():
        wb.handle_desktop_action_for_test(17)
    if (OUT / "capture-settings.json").exists():
        saved = json.loads((OUT / "capture-settings.json").read_text(encoding="utf-8"))
        unreal.SystemLibrary.execute_console_command(world, "au.NeverDisableSubmixes " + str(saved["never_disable_submixes"]))
elif a.phase == "capture-settings":
    assert dev.is_session_active()
    if not (OUT / "capture-settings.json").exists():
        write("capture-settings.json", {"never_disable_submixes": unreal.SystemLibrary.get_console_variable_int_value("au.NeverDisableSubmixes")})
    unreal.SystemLibrary.execute_console_command(world, "au.NeverDisableSubmixes 1")
    scene = command("snapshot.export")
    scene["state"]["training"]["bTravelActive"] = False
    scene["state"]["training"]["activeTravelEncounterIndex"] = -1
    command("snapshot.import", {"scene": scene})
result = snapshot()
write(a.phase + "-latest.json", result)
print(json.dumps(result, ensure_ascii=False))
