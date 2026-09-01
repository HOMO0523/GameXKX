"""One-shot -game capability probe for the Task10B runtime Python surface."""

from __future__ import annotations

import json
import re
from pathlib import Path

import unreal


COMMAND_LINE = str(unreal.SystemLibrary.get_command_line())


def _argument(name: str, default: str = "") -> str:
    match = re.search(rf"-{re.escape(name)}=(?:\"([^\"]*)\"|(\S+))", COMMAND_LINE)
    return (match.group(1) or match.group(2)) if match else default


report_name = _argument(
    "GameXXKTask10BReport", "task10b-game-runtime-python-probe.json"
)
if not re.fullmatch(r"[A-Za-z0-9_.-]+\.json", report_name):
    report_name = "task10b-game-runtime-python-probe.json"
REPORT = Path(unreal.Paths.project_saved_dir()) / "HarnessReports" / report_name
ACTION_TOKENS = [
    token.strip()
    for token in _argument("GameXXKTask10BActions").split(",")
    if token.strip()
]
STATE = {
    "ticks": 0,
    "handle": None,
    "controller": None,
    "actionIndex": 0,
    "waitTicks": 0,
    "observations": [],
}


def _finish(payload: dict) -> None:
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    handle = STATE.get("handle")
    if handle is not None:
        unreal.unregister_slate_post_tick_callback(handle)
        STATE["handle"] = None


def _tick(_delta_seconds: float) -> None:
    STATE["ticks"] += 1
    controller = STATE.get("controller")
    if controller is None:
        controllers = []
        try:
            for obj in unreal.ObjectIterator():
                if "GameXXKMVPPlayerController" not in type(obj).__name__:
                    continue
                path = str(obj.get_path_name())
                if path.startswith("/Script/") or "Default__" in path:
                    continue
                controllers.append(obj)
        except Exception as exc:
            _finish({"ok": False, "reason": f"object_iterator_failed:{exc}"})
            return
        if not controllers and STATE["ticks"] < 300:
            return
        if not controllers:
            _finish({"ok": False, "reason": "runtime_controller_unavailable"})
            return
        controller = controllers[0]
        STATE["controller"] = controller
    world = controller.get_world()
    can_probe = hasattr(controller, "get_desktop_story_probe_state")
    can_act = hasattr(controller, "execute_desktop_story_probe_action")
    snapshot = lambda: {
        str(key): str(value)
        for key, value in dict(controller.get_desktop_story_probe_state()).items()
    }
    if not ACTION_TOKENS:
        _finish(
            {
                "ok": bool(world and can_probe and can_act),
                "ticks": STATE["ticks"],
                "controller": str(controller.get_path_name()),
                "world": str(world.get_path_name()) if world else "",
                "canProbe": can_probe,
                "canAct": can_act,
                "state": snapshot() if can_probe else {},
            }
        )
        return
    if not world or not can_probe or not can_act:
        _finish({"ok": False, "reason": "runtime_probe_api_unavailable"})
        return
    if STATE["waitTicks"] > 0:
        STATE["waitTicks"] -= 1
        return
    if STATE["actionIndex"] >= len(ACTION_TOKENS):
        final = snapshot()
        _finish(
            {
                "ok": all(item.get("ok") is True for item in STATE["observations"]),
                "ticks": STATE["ticks"],
                "controller": str(controller.get_path_name()),
                "world": str(world.get_path_name()),
                "observations": STATE["observations"],
                "state": final,
            }
        )
        return
    token = ACTION_TOKENS[STATE["actionIndex"]]
    STATE["actionIndex"] += 1
    action, separator, argument = token.partition("|")
    if action == "wait":
        try:
            STATE["waitTicks"] = max(0, min(600, int(argument)))
        except ValueError:
            _finish({"ok": False, "reason": "invalid_wait", "token": token})
        return
    before = snapshot()
    accepted = bool(
        controller.execute_desktop_story_probe_action(
            unreal.Name(action), unreal.Name(argument if separator else "")
        )
    )
    after = snapshot()
    STATE["observations"].append(
        {
            "action": action,
            "argument": argument if separator else "",
            "ok": accepted,
            "before": before,
            "after": after,
        }
    )
    if not accepted:
        _finish(
            {
                "ok": False,
                "reason": "action_rejected",
                "failedAction": action,
                "failedArgument": argument if separator else "",
                "ticks": STATE["ticks"],
                "controller": str(controller.get_path_name()),
                "world": str(world.get_path_name()),
                "observations": STATE["observations"],
                "state": after,
            }
        )
        return
    STATE["waitTicks"] = 8


STATE["handle"] = unreal.register_slate_post_tick_callback(_tick)
