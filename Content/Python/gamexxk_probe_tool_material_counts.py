"""Check that each material count is visible even before a target is selected."""
import json
import sys
from pathlib import Path
import unreal

game_instance = unreal.new_object(unreal.GameInstance)
subsystem = unreal.new_object(unreal.GameXXKMVPSubsystem, outer=game_instance)
assert subsystem.start_game()
widget = unreal.new_object(unreal.GameXXKDesktopTrainingWorkbenchWidget)
widget.set_mvp_subsystem(subsystem)
widget.open_backpack()
widget.handle_desktop_action_for_test(3)
checks = []
for mode, material in ((unreal.GameXXKDesktopToolMode.ENHANCE, "强化石"), (unreal.GameXXKDesktopToolMode.REFORGE, "洗炼砂")):
    widget.set_tool_mode_for_test(mode)
    tree = unreal.find_object(widget, "DesktopTrainingWorkbenchWidgetTree") or unreal.find_object(widget, "WidgetTree")
    label = unreal.find_object(tree, "ToolRecipePreview")
    assert label, "Material preview control is missing"
    text = str(label.get_text())
    checks.append({"mode": str(mode), "passed": material in text, "text": text})
result = {"checks": checks, "passed": sum(row["passed"] for row in checks)}
filename = sys.argv[1] if len(sys.argv) > 1 else "empty-material-counts.json"
assert Path(filename).name == filename and filename.endswith(".json")
output = Path(unreal.Paths.project_dir()) / "Saved/Codex/ToolsRedesign-20260909"
output.mkdir(parents=True, exist_ok=True)
(output / filename).write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(result, ensure_ascii=False))
widget.close_workbench()
del widget, subsystem, game_instance
