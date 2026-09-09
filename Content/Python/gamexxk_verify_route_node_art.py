"""Restorable desktop route-art inspection; player saves stay write-suppressed."""
import json
from pathlib import Path
import sys
import time
import unreal
import gamexxk_probe_training_visual_mvp as base

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "Saved/RouteNodeArt"


def context():
    world, controller, workbench = base._controller_and_widget()
    assert world and "L_DesktopTrainingHUD" in world.get_name(), "Use the canonical desktop PIE"
    instance = unreal.GameplayStatics.get_game_instance(world)
    dev = next(obj for obj in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if obj.get_outer() == instance)
    return world, controller, workbench.get_mvp_subsystem(), dev


def command(dev, name, args=None):
    result = json.loads(dev.execute_json(json.dumps({"schema": 1, "command": name, "args": args or {}})))
    assert result["ok"], result
    return result


def node_state(node):
    return {
        "id": node.node_id,
        "kind": str(node.node_kind),
        "icon": node.icon_path,
        "enabled": node.enabled,
        "visited": node.visited,
        "circle": node.selection_circle_visible,
        "animating": node.selection_circle_animating,
        "frame": node.selection_circle_frame,
        "center": [node.screen_hit_box_center.x, node.screen_hit_box_center.y],
    }


def observe():
    world, controller, subsystem, dev = context()
    route = controller.get_route_map_widget_for_test()
    state = subsystem.get_runtime_state_copy()
    return {
        "ok": True, "map": world.get_name(), "screen": str(state.screen),
        "dev_session": dev.is_session_active(),
        "route_visible": str(route.get_visibility()) if route else None,
        "battle_phase": str(state.card_run.active_battle.phase) if state.card_run.has_active_card_battle else None,
        "living_enemies": [{"id": str(unit.unit_id), "hp": unit.hp} for unit in state.card_run.active_battle.units
                           if "ENEMY" in str(unit.side) and unit.living],
        "nodes": [node_state(node) for node in route.get_route_node_visual_states_for_test()] if route else [],
    }


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "observe"
    world, controller, subsystem, dev = context()
    if mode == "begin":
        assert not dev.is_session_active(), "Another development session is active"
        command(dev, "session.begin")
        unreal._gamexxk_route_art_session = dev
        (OUTPUT / "session-before.json").write_text(json.dumps(command(dev, "snapshot.export"), ensure_ascii=False, indent=2), encoding="utf-8")
        subsystem.cancel_training_challenge_to_workbench()
        assert subsystem.start_training_challenge(unreal.Name("Training.Normal.1-1")), "Cannot start the first unlocked challenge"
        controller.refresh_player_flow_widgets_from_state()
    elif mode in ("select", "watch"):
        assert getattr(unreal, "_gamexxk_route_art_session", None) == dev
        route = controller.get_route_map_widget_for_test()
        nodes = route.get_route_node_visual_states_for_test()
        requested = int(sys.argv[2]) if len(sys.argv) > 2 else next(n.node_id for n in nodes if n.enabled)
        trace = []
        started = time.monotonic()
        if mode == "select":
            assert route.select_route_node_with_feedback(requested)
        trace.append({"time": 0.0, **observe()})
        click_started = None

        def tick(delta):
            nonlocal click_started
            elapsed = time.monotonic() - started
            snapshot = observe()
            if click_started is None and any(node["animating"] for node in snapshot["nodes"]):
                click_started = elapsed
            if mode == "select" or click_started is not None:
                trace.append({"time": elapsed, **snapshot})
            complete = elapsed >= 0.8 if mode == "select" else (
                (click_started is not None and elapsed - click_started >= 1.0) or elapsed >= 30.0)
            if complete:
                unreal.unregister_slate_post_tick_callback(unreal._gamexxk_route_art_trace_handle)
                del unreal._gamexxk_route_art_trace_handle
                (OUTPUT / "selection-timeline.json").write_text(json.dumps(trace, ensure_ascii=False, indent=2), encoding="utf-8")

        unreal._gamexxk_route_art_trace_handle = unreal.register_slate_post_tick_callback(tick)
    elif mode == "outer":
        assert not dev.is_session_active(), "Another development session is active"
        command(dev, "session.begin")
        unreal._gamexxk_route_art_session = dev
        _, _, workbench = base._controller_and_widget()
        assert workbench.open_backpack()
        workbench.handle_desktop_action_for_test(4)
    elif mode == "outer-open-map":
        _, _, workbench = base._controller_and_widget()
        print(json.dumps({"before_nav": str(workbench.get_active_nav_for_test()),
                          "expanded": workbench.is_backpack_expanded_for_test()}))
        if not workbench.is_backpack_expanded_for_test():
            workbench.open_backpack()
        if "TRAINING" not in str(workbench.get_active_nav_for_test()):
            workbench.handle_desktop_action_for_test(4)
        print(json.dumps({"after_nav": str(workbench.get_active_nav_for_test()),
                          "expanded": workbench.is_backpack_expanded_for_test(),
                          "stage_buttons": workbench.get_training_stage_button_count_for_test()}))
    elif mode == "retreat":
        assert getattr(unreal, "_gamexxk_route_art_session", None) == dev
        assert subsystem.retreat_current_battle_to_route(), "Current battle does not allow route retreat"
        controller.refresh_player_flow_widgets_from_state()
    elif mode == "auto":
        assert getattr(unreal, "_gamexxk_route_art_session", None) == dev
        assert subsystem.set_battle_auto_play_enabled(True)
    elif mode == "finish-reward":
        assert getattr(unreal, "_gamexxk_route_art_session", None) == dev
        board = controller.get_battle_board_widget_for_test()
        assert board and board.has_pending_route_reward(), "No completed battle reward is pending"
        assert board.skip_pending_route_reward()
        subsystem.set_battle_auto_play_enabled(False)
        controller.refresh_player_flow_widgets_from_state()
    elif mode == "restore":
        assert getattr(unreal, "_gamexxk_route_art_session", None) == dev
        command(dev, "session.restore")
        del unreal._gamexxk_route_art_session
        restored = command(dev, "snapshot.export")
        (OUTPUT / "session-restored.json").write_text(json.dumps(restored, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(observe(), ensure_ascii=False))


if __name__ == "__main__":
    main()
