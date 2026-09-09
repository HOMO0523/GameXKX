"""One-shot diagnostic: rebuild/slate state around Travel bar freeze."""
from __future__ import annotations

import json

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
    size = widget.get_travel_hero_health_bar_slate_size_for_test()
    rect = widget.get_travel_hero_health_bar_rect_for_test()
    out.update(
        {
            "ok": True,
            "build_count": int(widget.get_programmatic_layout_build_count_for_test_blueprint()),
            "pending_layout": bool(widget.has_pending_layout_refresh_for_test_blueprint()),
            "hero_slate_size": {"x": float(size.x), "y": float(size.y)},
            "hero_slate_valid": bool(widget.is_travel_hero_health_bar_slate_valid_for_test()),
            "hero_rect": {"x": float(rect.x), "y": float(rect.y), "w": float(rect.z), "h": float(rect.w)},
            "hero_percent": round(float(widget.get_travel_hero_health_bar_percent_for_test()), 4),
            "hero_visual": round(float(widget.get_travel_visual_hero_health_fraction_for_test()), 4),
            "logical_phase": str(subsystem_phase(widget)),
        }
    )
    print(json.dumps(out, ensure_ascii=False))


def subsystem_phase(widget):
    subsystem = widget.get_mvp_subsystem()
    travel = subsystem.get_training_travel_runtime_copy()
    try:
        return str(getattr(travel, "phase", None))
    except Exception:
        return ""


if __name__ == "__main__":
    main()
