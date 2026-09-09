"""One-shot diagnostic: force hero bar percent and report pointer value."""
from __future__ import annotations

import json
import sys

import unreal


def main():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world() if editor else None
    out = {"ok": False}
    if not world:
        print(json.dumps(out, ensure_ascii=False))
        return
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    controller.set_desktop_training_workbench_enabled_for_test(True)
    widget = controller.get_desktop_training_workbench_widget_for_test()
    percent = float(sys.argv[1]) if len(sys.argv) > 1 else 0.25
    widget.set_travel_hero_health_bar_percent_for_test(percent)
    out.update(
        {
            "ok": True,
            "set": percent,
            "percent": round(float(widget.get_travel_hero_health_bar_percent_for_test()), 4),
            "visual_fraction": round(float(widget.get_travel_visual_hero_health_fraction_for_test()), 4),
        }
    )
    print(json.dumps(out, ensure_ascii=False))


if __name__ == "__main__":
    main()
