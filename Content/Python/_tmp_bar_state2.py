"""One-shot diagnostic: full Travel bar state for long-run monitoring."""
from __future__ import annotations

import json

import unreal


def _get(value, name, default=None):
    try:
        result = getattr(value, name)
        return default if result is None else result
    except Exception:
        return default


def _vec4(value):
    if value is None:
        return None
    return {"x": float(value.x), "y": float(value.y), "w": float(value.z), "h": float(value.w)}


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
    subsystem = widget.get_mvp_subsystem()
    travel = subsystem.get_training_travel_runtime_copy()
    enemies = _get(travel, "enemies", []) or []
    party = _get(travel, "party_units", []) or []
    out.update(
        {
            "ok": True,
            "logical_phase": str(_get(travel, "phase", None)),
            "visual_phase": str(widget.get_travel_visual_phase_name_for_test()),
            "travel_active": bool(_get(_get(subsystem.get_runtime_state_copy(), "training", None), "travel_active", False)),
            "enemies": [
                {
                    "id": str(_get(enemy, "enemy_definition_id", "")),
                    "hp": int(_get(enemy, "hp", -1)),
                    "max": int(_get(enemy, "max_hp", -1)),
                }
                for enemy in enemies
            ],
            "party": [int(_get(unit, "hp", -1)) for unit in party],
            "fractions": {
                "hero": round(float(widget.get_travel_visual_hero_health_fraction_for_test()), 4),
                "comp0": round(float(widget.get_travel_visual_party_health_fraction_for_test(1)), 4),
                "comp1": round(float(widget.get_travel_visual_party_health_fraction_for_test(2)), 4),
                "enemy": round(float(widget.get_travel_visual_enemy_health_fraction_for_test()), 4),
            },
            "bar_percents": {
                "hero": round(float(widget.get_travel_hero_health_bar_percent_for_test()), 4),
                "comp0": round(float(widget.get_travel_companion_health_bar_percent_for_test(0)), 4),
                "comp1": round(float(widget.get_travel_companion_health_bar_percent_for_test(1)), 4),
                "enemy0": round(float(widget.get_travel_enemy_health_bar_percent_for_test(0)), 4),
                "enemy1": round(float(widget.get_travel_enemy_health_bar_percent_for_test(1)), 4),
                "enemy2": round(float(widget.get_travel_enemy_health_bar_percent_for_test(2)), 4),
            },
            "rects": {
                "hero": _vec4(widget.get_travel_hero_health_bar_rect_for_test()),
                "comp0": _vec4(widget.get_travel_companion_health_bar_rect_for_test(0)),
                "comp1": _vec4(widget.get_travel_companion_health_bar_rect_for_test(1)),
                "enemy0": _vec4(widget.get_travel_enemy_health_bar_rect_for_test(0)),
                "enemy1": _vec4(widget.get_travel_enemy_health_bar_rect_for_test(1)),
                "enemy2": _vec4(widget.get_travel_enemy_health_bar_rect_for_test(2)),
            },
        }
    )
    print(json.dumps(out, ensure_ascii=False))


if __name__ == "__main__":
    main()
