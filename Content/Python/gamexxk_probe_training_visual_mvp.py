"""Phase-based PIE probe for the desktop-training scrolling visual MVP.

This file always returns control to Unreal immediately. Any observation delay
must happen in the external MCP runner so the UE game thread can keep ticking.
"""

from __future__ import annotations

import argparse
import json

import unreal


PHASE_PREPARE_MAP = "prepare-map"
PHASE_START_TRAVEL = "start-travel"
PHASE_OBSERVE = "observe"
PHASE_ADVANCE = "advance"
HUD_MAP = "/Game/GameXXK/Maps/L_DesktopTrainingHUD"
DEFAULT_STAGE = "Training.Normal.1-1"


def _call(obj, name, *args):
    fn = getattr(obj, name, None) if obj is not None else None
    if not callable(fn):
        return None
    try:
        return fn(*args)
    except Exception as exc:  # pragma: no cover - executed inside UE Python
        return f"ERR:{exc}"


def _emit(payload):
    print(json.dumps(payload, ensure_ascii=False, sort_keys=True))
    return payload


def _world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


def _controller_and_widget():
    world = _world()
    if not world:
        return None, None, None
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    widget = _call(controller, "get_desktop_training_workbench_widget_for_test")
    if widget is None or isinstance(widget, str):
        _call(controller, "set_desktop_training_workbench_enabled_for_test", True)
        widget = _call(controller, "get_desktop_training_workbench_widget_for_test")
    return world, controller, widget


def _party_snapshot(world, widget):
    try:
        subsystem = _call(widget, "get_mvp_subsystem")
        state = _call(subsystem, "get_runtime_state_copy")
        card_run = getattr(state, "card_run", None)
        selection = getattr(card_run, "party_selection", None)
        roster = getattr(card_run, "companion_roster", None)
        companions = []
        for companion in list(getattr(roster, "permanent_companions", []) or []):
            companions.append(
                {
                    "instance_id": str(getattr(companion, "instance_id", "")),
                    "recruit_template_id": str(getattr(companion, "recruit_template_id", "")),
                    "active": bool(getattr(companion, "is_active", False)),
                }
            )
        quest_selection = getattr(selection, "quest_npc", None)
        return {
            "active_permanent_instance_id": str(
                getattr(selection, "active_permanent_companion_instance_id", "")
            ),
            "active_temporary_quest_npc_id": str(
                getattr(card_run, "active_temporary_quest_npc_id", "")
            ),
            "selected_quest_npc_id": str(getattr(quest_selection, "npc_id", "")),
            "permanent_companions": companions,
        }
    except Exception as exc:  # pragma: no cover - executed inside UE Python
        return {"error": str(exc)}


def _travel_runtime_snapshot(widget):
    try:
        subsystem = _call(widget, "get_mvp_subsystem")
        runtime = _call(subsystem, "get_training_travel_runtime_copy")
        enemies = []
        for slot_index, enemy in enumerate(list(getattr(runtime, "enemies", []) or [])):
            enemies.append(
                {
                    "slot": slot_index,
                    "enemy_id": str(getattr(enemy, "enemy_definition_id", "")),
                    "hp": int(getattr(enemy, "hp", 0)),
                    "max_hp": int(getattr(enemy, "max_hp", 0)),
                    "attack": int(getattr(enemy, "attack", 0)),
                }
            )
        return {
            "encounter_index": int(getattr(runtime, "encounter_index", -1)),
            "active_enemy_index": int(getattr(runtime, "active_enemy_index", -1)),
            "enemies": enemies,
        }
    except Exception as exc:  # pragma: no cover - executed inside UE Python
        return {"error": str(exc)}


def _snapshot(phase, *, extra=None):
    world, controller, widget = _controller_and_widget()
    if not world:
        return {"ok": False, "phase": phase, "reason": "no_pie_world"}
    if widget is None or isinstance(widget, str):
        return {
            "ok": False,
            "phase": phase,
            "reason": "workbench_missing",
            "detail": widget,
        }

    payload = {
        "ok": True,
        "phase": phase,
        "world": str(_call(world, "get_name") or ""),
        "controller": str(_call(controller, "get_name") or ""),
        "strip": bool(_call(widget, "has_travel_visual_strip_for_test")),
        "scroll_offset": float(_call(widget, "get_travel_visual_scroll_offset_for_test") or 0.0),
        "scroll_velocity": float(_call(widget, "get_travel_visual_scroll_velocity_for_test") or 0.0),
        "walk_frame": int(_call(widget, "get_travel_visual_walk_frame_for_test") or 0),
        "hero_rendered_frame": int(_call(widget, "get_travel_visual_hero_rendered_frame_for_test") or 0),
        "enemy_rendered_frame": int(_call(widget, "get_travel_visual_enemy_rendered_frame_for_test") or 0),
        "loop_count": int(_call(widget, "get_travel_visual_completed_loop_count_for_test") or 0),
        "native_tick_count": int(_call(widget, "get_travel_visual_native_tick_count_for_test") or 0),
        "logical_phase": str(_call(widget, "get_travel_logical_phase_name_for_test") or ""),
        "visual_phase": str(_call(widget, "get_travel_visual_phase_name_for_test") or ""),
        "hero_action": str(_call(widget, "get_travel_visual_hero_action_name_for_test") or ""),
        "party_actions": [
            str(_call(widget, "get_travel_visual_party_action_name_for_test", party_index) or "")
            for party_index in range(3)
        ],
        "enemy_action": str(_call(widget, "get_travel_visual_enemy_action_name_for_test") or ""),
        "enemy_id": str(_call(widget, "get_travel_visual_enemy_definition_id_for_test") or ""),
        "enemy_visible": bool(_call(widget, "is_travel_visual_enemy_visible_for_test")),
        "hero_hp_fraction": float(_call(widget, "get_travel_visual_hero_health_fraction_for_test") or 0.0),
        "party_hp_fractions": [
            float(_call(widget, "get_travel_visual_party_health_fraction_for_test", party_index) or 0.0)
            for party_index in range(3)
        ],
        "enemy_hp_fraction": float(_call(widget, "get_travel_visual_enemy_health_fraction_for_test") or 0.0),
        "atlas": str(_call(widget, "get_travel_visual_atlas_resource_path_for_test") or ""),
        "background": str(_call(widget, "get_travel_visual_background_resource_path_for_test") or ""),
        "party": _party_snapshot(world, widget),
        "travel_runtime": _travel_runtime_snapshot(widget),
    }
    if extra:
        payload.update(extra)
    return payload


def _prepare_map():
    if _world():
        return _emit({"ok": False, "phase": PHASE_PREPARE_MAP, "reason": "pie_is_running"})
    loaded_world = unreal.EditorLoadingAndSavingUtils.load_map(HUD_MAP)
    return _emit(
        {
            "ok": bool(loaded_world),
            "phase": PHASE_PREPARE_MAP,
            "map": HUD_MAP,
            "world": str(_call(loaded_world, "get_name") or ""),
        }
    )


def _start_travel(stage):
    world, controller, widget = _controller_and_widget()
    if not world:
        return _emit({"ok": False, "phase": PHASE_START_TRAVEL, "reason": "no_pie_world"})
    if widget is None or isinstance(widget, str):
        return _emit(
            {
                "ok": False,
                "phase": PHASE_START_TRAVEL,
                "reason": "workbench_missing",
                "detail": widget,
            }
        )

    _call(controller, "set_desktop_training_workbench_enabled_for_test", True)
    _call(widget, "open_backpack")
    selected = bool(_call(widget, "select_stage_for_test", stage))
    travel_started = bool(_call(widget, "click_travel_for_test"))
    return _emit(
        _snapshot(
            PHASE_START_TRAVEL,
            extra={
                "selected_stage": selected,
                "travel_started": travel_started,
                "stage": stage,
            },
        )
    )


def _observe(capture):
    payload = _snapshot(PHASE_OBSERVE)
    world = _world()
    if capture and payload.get("ok") and world:
        unreal.SystemLibrary.execute_console_command(world, "HighResShot 1600x900")
        payload["capture_requested"] = True
    return _emit(payload)


def _advance():
    _world_value, _controller, widget = _controller_and_widget()
    if widget is None or isinstance(widget, str):
        return _emit({"ok": False, "phase": PHASE_ADVANCE, "reason": "workbench_missing"})
    result = bool(_call(widget, "advance_travel_for_test", 1))
    return _emit(_snapshot(PHASE_ADVANCE, extra={"advance_result": result}))


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--phase",
        choices=(PHASE_PREPARE_MAP, PHASE_START_TRAVEL, PHASE_OBSERVE, PHASE_ADVANCE),
        required=True,
    )
    parser.add_argument("--stage", default=DEFAULT_STAGE)
    parser.add_argument("--capture", action="store_true")
    args = parser.parse_args(argv)

    if args.phase == PHASE_PREPARE_MAP:
        return _prepare_map()
    if args.phase == PHASE_START_TRAVEL:
        return _start_travel(args.stage)
    if args.phase == PHASE_ADVANCE:
        return _advance()
    return _observe(args.capture)


if __name__ == "__main__":
    main()
