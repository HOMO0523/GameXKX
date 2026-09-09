"""Reproduce tool presentation gaps on detached transient objects, without PIE or saves."""
import json
import sys
from pathlib import Path
import unreal

game_instance = unreal.new_object(unreal.GameInstance)
subsystem = unreal.new_object(unreal.GameXXKMVPSubsystem, outer=game_instance)
assert subsystem.start_game(), "Detached fixture could not start"
widget = unreal.new_object(unreal.GameXXKDesktopTrainingWorkbenchWidget)
widget.set_mvp_subsystem(subsystem)
widget.open_backpack()
widget.handle_desktop_action_for_test(3)

def named(name):
    tree = unreal.find_object(widget, "DesktopTrainingWorkbenchWidgetTree") or unreal.find_object(widget, "WidgetTree")
    return unreal.find_object(tree, name) if tree else None

result = {"fixture": "detached transient objects; no PIE, assets, or save writes", "checks": []}
def check(name, passed, actual):
    result["checks"].append({"name": name, "passed": bool(passed), "actual": actual})

overlay = named("ToolsTalentLockedPanel")
check("tools default open", overlay is None, str(overlay.get_visibility()) if overlay else "absent")
widget.set_tool_mode_for_test(unreal.GameXXKDesktopToolMode.DISMANTLE)
check("dismantle exposes auto-place", named("ToolAutoFill") is not None, "present" if named("ToolAutoFill") else "absent")
widget.set_tool_mode_for_test(unreal.GameXXKDesktopToolMode.ENHANCE)
enabled = [index for index in range(9) if named("ToolInputSlot_" + str(index)) and named("ToolInputSlot_" + str(index)).get_is_enabled()]
check("enhance uses only first cell", enabled == [0], enabled)
check("enhance hides crafting level", named("ToolCraftLevelText") is None, "present" if named("ToolCraftLevelText") else "absent")
result["passed"] = sum(check["passed"] for check in result["checks"])
result["failed"] = len(result["checks"]) - result["passed"]
out = Path(unreal.Paths.project_dir()) / "Saved/Codex/ToolsRedesign-20260909"
out.mkdir(parents=True, exist_ok=True)
filename = sys.argv[1] if len(sys.argv) > 1 else "baseline-reproduction.json"
assert Path(filename).name == filename and filename.endswith(".json")
(out / filename).write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(result, ensure_ascii=False))
widget.close_workbench()
del widget, subsystem, game_instance
