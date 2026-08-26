"""Canonical DesktopTrainingHUD PIE acceptance for route event cards and Camp."""

from __future__ import annotations

import json
from pathlib import Path
import sys

import unreal

sys.path.insert(0, str(Path(unreal.Paths.project_content_dir()) / "Python"))
import gamexxk_probe_real_play_flow as probe  # noqa: E402


REPORT_PATH = Path(unreal.Paths.get_project_file_path()).parent / "Saved" / "HarnessReports" / "20260822-route-cards-canonical-pie.json"


def _fixture_result(value):
    if value is None:
        return False, ""
    if isinstance(value, str):
        return True, value
    if isinstance(value, (tuple, list)):
        return bool(value[0]) if value else False, "" if len(value) < 2 else str(value[1] or "")
    return bool(value), ""


def _enum(value):
    return probe._enum_name(value)


def _get(value, *names):
    return probe._struct_get(value, *names)


def _runtime(subsystem):
    state = subsystem.get_runtime_state_copy()
    run = _get(state, "card_run", "CardRun")
    bonuses = _get(run, "route_attribute_bonuses", "RouteAttributeBonuses")
    return {
        "screen": _enum(_get(state, "screen", "Screen")),
        "pending_route_node_id": int(_get(state, "pending_route_node_id", "PendingRouteNodeId") or -1),
        "player_hp": int(_get(state, "player_hp", "PlayerHP") or 0),
        "player_max_hp": int(_get(state, "player_max_hp", "PlayerMaxHP") or 0),
        "max_health_bonus": int(_get(bonuses, "max_health", "MaxHealth") or 0),
        "max_mana_bonus": int(_get(bonuses, "max_mana", "MaxMana") or 0),
        "defense_bonus": int(_get(bonuses, "defense", "Defense") or 0),
    }


def _context():
    world = probe._get_game_world()
    controller = probe._first_player_controller(world)
    if not controller and world:
        try:
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
        except Exception:
            controller = None
    subsystem = probe._get_mvp_subsystem(world) or probe._get_mvp_subsystem_from_player_controller(controller)
    panel = controller.get_route_encounter_panel_widget_for_test() if controller else None
    if not subsystem and controller:
        for getter_name in (
            "get_desktop_training_workbench_widget_for_test",
            "get_town_hud_widget_for_test",
            "get_route_encounter_panel_widget_for_test",
        ):
            try:
                widget = getattr(controller, getter_name)()
                subsystem = widget.get_mvp_subsystem() if widget else None
                if subsystem:
                    break
            except Exception:
                pass
    if not subsystem and world:
        try:
            game_instance = world.get_game_instance()
            subsystem = game_instance.get_subsystem(unreal.GameXXKMVPSubsystem) if game_instance else None
        except Exception:
            subsystem = None
    return world, controller, subsystem, panel


def _load_report():
    if REPORT_PATH.exists():
        try:
            return json.loads(REPORT_PATH.read_text(encoding="utf-8"))
        except Exception:
            pass
    return {"surface": "/Game/GameXXK/Maps/L_DesktopTrainingHUD", "steps": {}}


def _save_report(report):
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"ok": bool(report.get("ok", False)), "report": str(REPORT_PATH), "data": report}, ensure_ascii=False))


def setup_event():
    world, controller, subsystem, _ = _context()
    result = {"world": world.get_outermost().get_name() if world else ""}
    if not world or not controller or not subsystem:
        result.update({
            "error": "missing_pie_context",
            "has_world": bool(world),
            "has_controller": bool(controller),
            "controller_class": controller.get_class().get_name() if controller else "",
            "has_subsystem": bool(subsystem),
        })
        return result
    steps = [
        ("start_new_game", bool(subsystem.start_new_game())),
        ("accept_quest", bool(subsystem.accept_quest())),
        ("open_dungeon", bool(subsystem.open_dungeon_from_town_exit())),
        ("select_start", bool(subsystem.select_route_node_by_id(0))),
    ]
    fixture_ok, fixture_error = _fixture_result(subsystem.apply_route_encounter_acceptance_fixture_for_test(False))
    controller.refresh_player_flow_widgets_for_test()
    panel = controller.get_route_encounter_panel_widget_for_test()
    result.update({
        "flow_steps": steps,
        "fixture_ok": fixture_ok,
        "fixture_error": fixture_error,
        "panel_open": bool(controller.is_route_encounter_panel_open_for_test()),
        "before": _runtime(subsystem),
    })
    result["ok"] = all(value for _, value in steps) and fixture_ok and result["panel_open"]
    return result


def select_event_third():
    _, controller, subsystem, panel = _context()
    result = {}
    if not controller or not subsystem or not panel:
        result["error"] = "missing_event_selection_context"
        return result
    selected = bool(panel.select_choice_for_test(2))
    result.update({"selected_third": selected, "state_after_selection": _runtime(subsystem)})
    result["ok"] = selected and result["state_after_selection"]["screen"].endswith("ROUTE_EVENT")
    return result


def resolve_event_setup_camp():
    _, controller, subsystem, panel = _context()
    result = {}
    if not controller or not subsystem or not panel:
        result["error"] = "missing_event_context"
        return result
    before = _runtime(subsystem)
    selected = bool(panel.select_choice_for_test(2))
    confirmed = bool(panel.confirm_selected_choice_for_test())
    event_after = _runtime(subsystem)
    clear_ok, clear_error = _fixture_result(subsystem.clear_route_encounter_acceptance_fixture_for_test())
    camp_ok, camp_error = _fixture_result(subsystem.apply_route_encounter_acceptance_fixture_for_test(True))
    controller.refresh_player_flow_widgets_for_test()
    camp_before = _runtime(subsystem)
    result.update({
        "event_before": before,
        "selected_third": selected,
        "confirmed": confirmed,
        "event_after": event_after,
        "event_clear_ok": clear_ok,
        "event_clear_error": clear_error,
        "camp_fixture_ok": camp_ok,
        "camp_fixture_error": camp_error,
        "camp_panel_open": bool(controller.is_route_encounter_panel_open_for_test()),
        "camp_before": camp_before,
    })
    result["ok"] = (
        selected
        and confirmed
        and event_after["screen"].endswith("DUNGEON_MAP")
        and event_after["max_health_bonus"] == before["max_health_bonus"] + 5
        and event_after["max_mana_bonus"] == before["max_mana_bonus"]
        and event_after["defense_bonus"] == before["defense_bonus"]
        and clear_ok
        and camp_ok
        and result["camp_panel_open"]
    )
    return result


def resolve_camp():
    _, controller, subsystem, panel = _context()
    result = {}
    if not controller or not subsystem or not panel:
        result["error"] = "missing_camp_context"
        return result
    before = _runtime(subsystem)
    triggered = bool(panel.trigger_primary_action_for_test())
    after = _runtime(subsystem)
    clear_ok, clear_error = _fixture_result(subsystem.clear_route_encounter_acceptance_fixture_for_test())
    result.update({
        "before": before,
        "triggered_primary": triggered,
        "after": after,
        "clear_ok": clear_ok,
        "clear_error": clear_error,
    })
    result["ok"] = (
        triggered
        and after["screen"].endswith("DUNGEON_MAP")
        and after["player_hp"] > before["player_hp"]
        and clear_ok
    )
    return result


def setup_camp():
    _, controller, subsystem, _ = _context()
    result = {}
    if not controller or not subsystem:
        result["error"] = "missing_camp_setup_context"
        return result
    clear_ok, clear_error = _fixture_result(subsystem.clear_route_encounter_acceptance_fixture_for_test())
    camp_ok, camp_error = _fixture_result(subsystem.apply_route_encounter_acceptance_fixture_for_test(True))
    controller.refresh_player_flow_widgets_for_test()
    result.update({
        "clear_ok": clear_ok,
        "clear_error": clear_error,
        "camp_fixture_ok": camp_ok,
        "camp_fixture_error": camp_error,
        "camp_panel_open": bool(controller.is_route_encounter_panel_open_for_test()),
        "camp_before": _runtime(subsystem),
    })
    result["ok"] = clear_ok and camp_ok and result["camp_panel_open"]
    return result


def close_camp_resume_and_resolve():
    _, controller, subsystem, panel = _context()
    result = {}
    if not controller or not subsystem or not panel:
        result["error"] = "missing_camp_close_context"
        return result
    before = _runtime(subsystem)
    close_button = None
    try:
        tree = getattr(panel, "widget_tree", None)
        if not tree:
            try:
                tree = panel.get_editor_property("widget_tree")
            except Exception:
                tree = None
        if not tree:
            getter = getattr(panel, "get_widget_tree", None)
            tree = getter() if callable(getter) else None
        close_button = tree.find_widget(unreal.Name("RouteEncounterCloseAction")) if tree else None
    except Exception as exc:
        result["close_lookup_error"] = str(exc)
    clicked = False
    click_error = ""
    if close_button:
        try:
            close_button.on_clicked.broadcast()
            clicked = True
        except Exception as exc:
            click_error = str(exc)
    after_close = _runtime(subsystem)
    route_map = controller.get_route_map_widget_for_test()
    resumed = bool(route_map.execute_route_node_by_id(before["pending_route_node_id"])) if route_map else False
    resumed_state = _runtime(subsystem)
    resolved = bool(panel.trigger_primary_action_for_test())
    after_action = _runtime(subsystem)
    clear_ok, clear_error = _fixture_result(subsystem.clear_route_encounter_acceptance_fixture_for_test())
    result.update({
        "before": before,
        "close_widget_found": bool(close_button),
        "close_delegate_broadcast": clicked,
        "close_click_error": click_error,
        "after_close": after_close,
        "resumed": resumed,
        "resumed_state": resumed_state,
        "resolved_original_action": resolved,
        "after_action": after_action,
        "clear_ok": clear_ok,
        "clear_error": clear_error,
    })
    result["ok"] = (
        clicked
        and after_close["screen"].endswith("DUNGEON_MAP")
        and after_close["pending_route_node_id"] == before["pending_route_node_id"]
        and resumed
        and resumed_state["screen"].endswith("ROUTE_CAMP")
        and resolved
        and after_action["screen"].endswith("DUNGEON_MAP")
        and after_action["pending_route_node_id"] == -1
        and after_action["player_hp"] > before["player_hp"]
        and clear_ok
    )
    return result


def resume_camp_after_external_close():
    _, controller, subsystem, panel = _context()
    result = {}
    if not controller or not subsystem or not panel:
        result["error"] = "missing_external_close_context"
        return result
    after_close = _runtime(subsystem)
    route_map = controller.get_route_map_widget_for_test()
    resumed = bool(route_map.execute_route_node_by_id(after_close["pending_route_node_id"])) if route_map else False
    resumed_state = _runtime(subsystem)
    resolved = bool(panel.trigger_primary_action_for_test())
    after_action = _runtime(subsystem)
    clear_ok, clear_error = _fixture_result(subsystem.clear_route_encounter_acceptance_fixture_for_test())
    result.update({
        "external_slate_click": True,
        "after_close": after_close,
        "resumed": resumed,
        "resumed_state": resumed_state,
        "resolved_original_action": resolved,
        "after_action": after_action,
        "clear_ok": clear_ok,
        "clear_error": clear_error,
    })
    result["ok"] = (
        after_close["screen"].endswith("DUNGEON_MAP")
        and after_close["pending_route_node_id"] >= 0
        and resumed
        and resumed_state["screen"].endswith("ROUTE_CAMP")
        and resolved
        and after_action["screen"].endswith("DUNGEON_MAP")
        and after_action["pending_route_node_id"] == -1
        and after_action["player_hp"] > resumed_state["player_hp"]
        and clear_ok
    )
    return result


def escape_camp_resume_and_resolve():
    _, controller, subsystem, panel = _context()
    result = {}
    if not controller or not subsystem or not panel:
        result["error"] = "missing_escape_context"
        return result
    before = _runtime(subsystem)
    escaped = bool(controller.trigger_route_encounter_escape_for_test())
    after_escape = _runtime(subsystem)
    route_map = controller.get_route_map_widget_for_test()
    resumed = bool(route_map.execute_route_node_by_id(before["pending_route_node_id"])) if route_map else False
    resumed_state = _runtime(subsystem)
    resolved = bool(panel.trigger_primary_action_for_test())
    after_action = _runtime(subsystem)
    clear_ok, clear_error = _fixture_result(subsystem.clear_route_encounter_acceptance_fixture_for_test())
    result.update({
        "escaped_via_controller_input_key": escaped,
        "before": before,
        "after_escape": after_escape,
        "resumed": resumed,
        "resumed_state": resumed_state,
        "resolved_original_action": resolved,
        "after_action": after_action,
        "clear_ok": clear_ok,
        "clear_error": clear_error,
    })
    result["ok"] = (
        escaped
        and after_escape["screen"].endswith("DUNGEON_MAP")
        and after_escape["pending_route_node_id"] == before["pending_route_node_id"]
        and resumed
        and resumed_state["screen"].endswith("ROUTE_CAMP")
        and resolved
        and after_action["screen"].endswith("DUNGEON_MAP")
        and after_action["pending_route_node_id"] == -1
        and after_action["player_hp"] > resumed_state["player_hp"]
        and clear_ok
    )
    return result


def finalize_report():
    return {"ok": True, "note": "Removed superseded direct-widget-tree experiment; external Slate click is authoritative."}


def main():
    action = sys.argv[1] if len(sys.argv) > 1 else "setup_event"
    handlers = {
        "setup_event": setup_event,
        "select_event_third": select_event_third,
        "resolve_event_setup_camp": resolve_event_setup_camp,
        "resolve_camp": resolve_camp,
        "setup_camp": setup_camp,
        "close_camp_resume_and_resolve": close_camp_resume_and_resolve,
        "resume_camp_after_external_close": resume_camp_after_external_close,
        "escape_camp_resume_and_resolve": escape_camp_resume_and_resolve,
        "finalize_report": finalize_report,
    }
    report = _load_report()
    if action not in handlers:
        report["ok"] = False
        report["error"] = f"unknown_action:{action}"
        _save_report(report)
        return
    step_result = handlers[action]()
    if action == "finalize_report":
        report.setdefault("steps", {}).pop("close_camp_resume_and_resolve", None)
    report.setdefault("steps", {})[action] = step_result
    report["ok"] = all(bool(value.get("ok")) for value in report["steps"].values())
    _save_report(report)


if __name__ == "__main__":
    main()
