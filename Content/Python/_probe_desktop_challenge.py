"""Read-only live probe for the desktop Training challenge viewport."""

from __future__ import annotations

import json

import unreal

import gamexxk_probe_training_visual_mvp as visual_probe


def call(obj, name: str, *args):
    fn = getattr(obj, name, None)
    if fn is None or not callable(fn):
        return None, "missing"
    try:
        return fn(*args), None
    except Exception as exc:  # pragma: no cover - live probe diagnostics
        return None, f"error:{exc}"


def main() -> None:
    world = visual_probe._get_game_world()
    controller = visual_probe._first_player_controller(world)
    widget, widget_error = call(controller, "get_desktop_training_workbench_widget_for_test")
    if widget is None:
        print(json.dumps({"ok": False, "reason": widget_error or "workbench_missing"}, ensure_ascii=False))
        return

    stage = unreal.Name("Training.Normal.1-2")
    out = {"ok": True, "stage": str(stage)}
    for name in (
        "get_selected_stage_id_for_test",
        "get_training_stage_button_count_for_test",
        "get_challenge_viewport_rect_for_test",
        "get_challenge_combat_strip_rect_for_test",
        "get_challenge_battle_board_rect_for_test",
        "get_challenge_combat_slot_count_for_test",
        "get_stage_tooltip_for_test",
    ):
        args = (stage,) if name == "get_stage_tooltip_for_test" else ()
        value, error = call(widget, name, *args)
        out[name] = error if error else str(value)

    selected, selected_error = call(widget, "select_stage_for_test", stage)
    challenged, challenge_error = call(widget, "click_challenge_for_test") if selected else (False, "stage_not_selected")
    out.update(
        {
            "selected": selected_error if selected_error else selected,
            "challenge_started": challenge_error if challenge_error else challenged,
        }
    )
    for name in (
        "is_challenge_viewport_active_for_test",
        "are_challenge_side_panels_read_only_for_test",
        "is_auto_battle_visible_for_test",
        "is_retry_visible_for_test",
        "get_selected_stage_id_for_test",
    ):
        value, error = call(widget, name)
        out[name] = error if error else str(value)
    print(json.dumps(out, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
