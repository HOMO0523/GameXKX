"""Opt-in PIE probe for the desktop-training scrolling visual MVP."""

from __future__ import annotations

import json
import time

import unreal


def _call(obj, name, *args):
    fn = getattr(obj, name, None) if obj is not None else None
    if not callable(fn):
        return None
    try:
        return fn(*args)
    except Exception as exc:  # pragma: no cover - executed inside UE Python
        return f"ERR:{exc}"


def _world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


def _subsystem(world):
    game_instance = _call(world, "get_game_instance")
    subsystem_type = getattr(unreal, "GameXXKMVPSubsystem", None)
    if game_instance is not None and subsystem_type is not None:
        return _call(game_instance, "get_subsystem", subsystem_type)
    return None


def main():
    world = _world()
    if not world:
        print(json.dumps({"ok": False, "reason": "no_pie_world"}, ensure_ascii=False))
        return

    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    # The TDD pipeline starts at L_Main's menu. Move through the approved
    # Start/New Game path so the town controller owns the opt-in workbench.
    menu = _call(controller, "get_main_menu_widget_for_test")
    if menu is not None and not isinstance(menu, str):
        _call(menu, "start_game")
        for _ in range(40):
            time.sleep(0.25)
            world = _world()
            if world:
                candidate = unreal.GameplayStatics.get_player_controller(world, 0)
                if candidate:
                    controller = candidate
                    break
        if not world:
            print(json.dumps({"ok": False, "reason": "no_pie_world_after_start"}, ensure_ascii=False))
            return
    _call(controller, "set_desktop_training_workbench_enabled_for_test", True)
    widget = _call(controller, "get_desktop_training_workbench_widget_for_test")
    if widget is None or isinstance(widget, str):
        print(json.dumps({"ok": False, "reason": "workbench_missing", "detail": widget}, ensure_ascii=False))
        return

    _call(widget, "open_backpack")
    selected = bool(_call(widget, "select_stage_for_test", "Training.Normal.1-1"))
    travel_started = bool(_call(widget, "click_travel_for_test"))
    subsystem = _call(widget, "get_mvp_subsystem") or _subsystem(world)
    travel_state = _call(subsystem, "get_runtime_state_copy")
    travel_runtime = _call(subsystem, "get_training_travel_runtime_copy")
    # Tick the presentation before advancing the authoritative runner into
    # combat; this captures a non-zero walk offset and then leaves the strip
    # visibly paused at the encounter boundary.
    # Let the actual UUserWidget NativeTick run in PIE. TickForTest is a C++
    # automation helper rather than a reflected Blueprint/Python function.
    time.sleep(0.6)
    walking_snapshot = {
        "scroll_offset": float(_call(widget, "get_travel_visual_scroll_offset_for_test") or 0.0),
        "walk_frame": int(_call(widget, "get_travel_visual_walk_frame_for_test") or 0),
    }
    advance_result = _call(widget, "advance_travel_for_test", 1)
    _call(widget, "tick_for_test", 0.5)
    time.sleep(0.05)

    snapshot = {
        "ok": True,
        "selected_stage": selected,
        "travel_started": travel_started,
        "advance_result": advance_result,
        "travel_runtime": str(travel_runtime),
        "strip": bool(_call(widget, "has_travel_visual_strip_for_test")),
        "walking_snapshot": walking_snapshot,
        "scroll_offset": float(_call(widget, "get_travel_visual_scroll_offset_for_test") or 0.0),
        "walk_frame": int(_call(widget, "get_travel_visual_walk_frame_for_test") or 0),
        "loop_count": int(_call(widget, "get_travel_visual_completed_loop_count_for_test") or 0),
        "native_tick_count": int(_call(widget, "get_travel_visual_native_tick_count_for_test") or 0),
        "atlas": str(_call(widget, "get_travel_visual_atlas_resource_path_for_test") or ""),
        "background": str(_call(widget, "get_travel_visual_background_resource_path_for_test") or ""),
    }
    unreal.SystemLibrary.execute_console_command(world, "HighResShot 1600x900")
    print(json.dumps(snapshot, ensure_ascii=False))


if __name__ == "__main__":
    main()
