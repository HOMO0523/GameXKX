#!/usr/bin/env python3
"""Drive the real GameXXK MVP entry flow through UE MCP and OS input."""

from __future__ import annotations

import argparse
import base64
import ctypes
import json
import math
import struct
import sys
import time
from pathlib import Path
from typing import Any

from ue_mcp_client import EDITOR_TOOLSET, UnrealMCPClient


PROJECT_ROOT = Path(__file__).resolve().parents[1]
REPORT_DIR = PROJECT_ROOT / "Saved" / "HarnessReports"
SCREENSHOT_DIR = PROJECT_ROOT / "Saved" / "Codex"
SLATE_TOOLSET = "SlateInspectorToolset.SlateInspectorToolset"
SCENE_TOOLSET = "editor_toolset.toolsets.scene.SceneTools"
PROBE_SCRIPT = "Content/Python/gamexxk_probe_real_play_flow.py"
MAIN_MAP = "/Game/GameXXK/Maps/L_Main"
QINGSHAN_MAP_TOKEN = "L_QingshanInn"


def _png_size(data: bytes) -> tuple[int, int]:
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
        return 0, 0
    return struct.unpack(">II", data[16:24])


def _load_json_from_probe(result: dict[str, Any]) -> dict[str, Any]:
    stdout = str(result.get("stdout", "")).strip()
    if not stdout:
        return {}
    last_line = stdout.splitlines()[-1]
    return json.loads(last_line)


def _runtime_screen(probe: dict[str, Any]) -> str:
    return str(probe.get("probe", {}).get("runtime_state", {}).get("screen", ""))


def _map_name(probe: dict[str, Any]) -> str:
    return str(probe.get("probe", {}).get("map_name", ""))


def _pawn_location(probe: dict[str, Any]) -> dict[str, float]:
    location = probe.get("probe", {}).get("pawn", {}).get("location", {})
    return location if isinstance(location, dict) else {}


def _actors(probe: dict[str, Any]) -> list[dict[str, Any]]:
    actors = probe.get("probe", {}).get("actors", [])
    return actors if isinstance(actors, list) else []


def _quest_npc(probe: dict[str, Any]) -> dict[str, Any]:
    for actor in _actors(probe):
        if actor.get("class") == "GameXXKTownNpcActor" and str(actor.get("get_npc_role", "")).upper().endswith("QUEST"):
            return actor
    return {}


def _quest_interacted(probe: dict[str, Any]) -> bool:
    npc = _quest_npc(probe)
    return bool(npc.get("was_last_interaction_successful") and npc.get("is_follower_active"))


def _has_play_session(probe: dict[str, Any]) -> bool:
    data = probe.get("probe", {})
    return bool(
        data.get("has_pie_world")
        and data.get("player_controller", {}).get("class", "").endswith("GameXXKMVPPlayerController")
        and data.get("hud", {}).get("class", "").endswith("GameXXKMVPHUD")
        and data.get("pawn", {}).get("class", "")
    )


def _distance(a: dict[str, float], b: dict[str, float]) -> float:
    return math.sqrt(sum((float(a.get(axis, 0.0)) - float(b.get(axis, 0.0))) ** 2 for axis in ("x", "y", "z")))


class PreviewInput:
    def __init__(self) -> None:
        self.user32 = ctypes.windll.user32
        try:
            self.user32.SetProcessDPIAware()
        except Exception:
            pass

    def find_preview_window(self) -> dict[str, Any]:
        enum_proc_type = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

        class Rect(ctypes.Structure):
            _fields_ = [
                ("left", ctypes.c_long),
                ("top", ctypes.c_long),
                ("right", ctypes.c_long),
                ("bottom", ctypes.c_long),
            ]

        matches: list[dict[str, Any]] = []

        def enum_proc(hwnd, _lparam):
            if not self.user32.IsWindowVisible(hwnd):
                return True
            length = self.user32.GetWindowTextLengthW(hwnd)
            if length <= 0:
                return True
            buffer = ctypes.create_unicode_buffer(length + 1)
            self.user32.GetWindowTextW(hwnd, buffer, length + 1)
            title = buffer.value
            if "GameXXK Preview" in title:
                rect = Rect()
                self.user32.GetWindowRect(hwnd, ctypes.byref(rect))
                matches.append({
                    "hwnd": int(hwnd),
                    "title": title,
                    "rect": [rect.left, rect.top, rect.right, rect.bottom],
                })
            return True

        self.user32.EnumWindows(enum_proc_type(enum_proc), 0)
        if not matches:
            raise RuntimeError("GameXXK Preview window was not found")
        return matches[0]

    def focus(self, window: dict[str, Any]) -> None:
        hwnd = ctypes.c_void_p(int(window["hwnd"]))
        self.user32.ShowWindow(hwnd, 5)
        self.user32.SetForegroundWindow(hwnd)
        time.sleep(0.2)

    def click_image_point(self, window: dict[str, Any], image_size: tuple[int, int], image_x: int, image_y: int) -> dict[str, int]:
        left, top, right, bottom = [int(v) for v in window["rect"]]
        width = max(1, right - left)
        height = max(1, bottom - top)
        image_width, image_height = image_size
        x = left + int(image_x * width / max(1, image_width))
        y = top + int(image_y * height / max(1, image_height))
        self.focus(window)
        self.user32.SetCursorPos(x, y)
        time.sleep(0.1)
        self.user32.mouse_event(0x0002, 0, 0, 0, 0)
        time.sleep(0.05)
        self.user32.mouse_event(0x0004, 0, 0, 0, 0)
        return {"x": x, "y": y}

    def press_key(self, window: dict[str, Any], virtual_key: int, hold_seconds: float = 0.0) -> None:
        self.focus(window)
        self.user32.keybd_event(virtual_key, 0, 0, 0)
        time.sleep(max(0.0, hold_seconds))
        self.user32.keybd_event(virtual_key, 0, 0x0002, 0)


class RealFlowHarness:
    def __init__(self, timeout: float, keep_pie: bool) -> None:
        self.client = UnrealMCPClient(timeout=timeout)
        self.input = PreviewInput()
        self.keep_pie = keep_pie
        self.events: list[dict[str, Any]] = []

    def event(self, name: str, **payload: Any) -> None:
        item = {"name": name, **payload}
        self.events.append(item)
        print(json.dumps(item, ensure_ascii=False), flush=True)

    def connect(self) -> None:
        if not self.client.connect():
            raise RuntimeError(f"Cannot connect to UE MCP at {self.client.endpoint}")
        self.event("mcp_connected", endpoint=self.client.endpoint)

    def probe(self, *args: str) -> dict[str, Any]:
        result = self.client.run_project_python_file(PROBE_SCRIPT, list(args))
        parsed = _load_json_from_probe(result)
        self.event("probe", screen=_runtime_screen(parsed), map=_map_name(parsed))
        return parsed

    def screenshot(self, name: str) -> tuple[Path, tuple[int, int]]:
        image = self.client.call_tool("Screenshot", {"ref": ""}, toolset_name=SLATE_TOOLSET, timeout=30.0)
        data = base64.b64decode(image.get("data", ""))
        SCREENSHOT_DIR.mkdir(parents=True, exist_ok=True)
        path = SCREENSHOT_DIR / name
        path.write_bytes(data)
        size = _png_size(data)
        self.event("screenshot", path=str(path), size=list(size))
        return path, size

    def wait_for(self, label: str, predicate, timeout: float = 10.0, interval: float = 0.5) -> dict[str, Any]:
        deadline = time.monotonic() + timeout
        last_probe: dict[str, Any] = {}
        while time.monotonic() < deadline:
            last_probe = self.probe()
            if predicate(last_probe):
                self.event("wait_ok", label=label)
                return last_probe
            time.sleep(interval)
        raise RuntimeError(f"Timed out waiting for {label}; last probe={json.dumps(last_probe, ensure_ascii=False)}")

    def walk_to_world_location(self, window: dict[str, Any], start_probe: dict[str, Any], target: dict[str, float]) -> dict[str, Any]:
        speed = 260.0
        current = _pawn_location(start_probe)
        if not current:
            raise RuntimeError("Cannot walk: pawn location missing")

        def press_for_axis(delta: float, positive_key: int, negative_key: int) -> None:
            if abs(delta) < 24.0:
                return
            duration = min(8.0, max(0.05, abs(delta) / speed))
            self.input.press_key(window, positive_key if delta > 0.0 else negative_key, hold_seconds=duration)
            time.sleep(0.15)

        press_for_axis(float(target.get("x", 0.0)) - float(current.get("x", 0.0)), ord("W"), ord("S"))
        mid_probe = self.probe()
        mid = _pawn_location(mid_probe)
        press_for_axis(float(target.get("y", 0.0)) - float(mid.get("y", 0.0)), ord("D"), ord("A"))
        after_probe = self.probe()

        for _ in range(2):
            current = _pawn_location(after_probe)
            if _distance(current, target) <= 96.0:
                break
            press_for_axis(float(target.get("x", 0.0)) - float(current.get("x", 0.0)), ord("W"), ord("S"))
            after_probe = self.probe()
            current = _pawn_location(after_probe)
            press_for_axis(float(target.get("y", 0.0)) - float(current.get("y", 0.0)), ord("D"), ord("A"))
            after_probe = self.probe()

        self.event("walk_probe", distance_to_target=_distance(_pawn_location(after_probe), target))
        return after_probe

    def run(self) -> dict[str, Any]:
        self.connect()
        if self.client.is_in_pie():
            self.client.stop_pie()
            self.event("stopped_existing_pie")
        self.client.call_tool("load_level", {"level_path": MAIN_MAP}, toolset_name=SCENE_TOOLSET, timeout=60.0)
        self.event("loaded_main_map", map=MAIN_MAP)
        self.probe("--delete-default-save")

        self.client.call_tool(
            "StartPIE",
            {"options": {"bSimulate": False, "playMode": "PlayMode_InEditorFloating", "warmupSeconds": 1.0}},
            toolset_name=EDITOR_TOOLSET,
            timeout=60.0,
        )
        self.event("started_pie")
        self.wait_for("PIE play session", _has_play_session)

        before_start_path, image_size = self.screenshot("real_flow_before_start.png")
        window = self.input.find_preview_window()
        start_click = self.input.click_image_point(window, image_size, 208, 160)
        self.event("clicked_start", **start_click)
        time.sleep(1.0)
        after_start_path, image_size = self.screenshot("real_flow_after_start.png")

        window = self.input.find_preview_window()
        qingshan_click = self.input.click_image_point(window, image_size, 208, 160)
        self.event("clicked_qingshan", **qingshan_click)
        after_qingshan = self.wait_for(
            "Qingshan click enters playable town map",
            lambda probe: QINGSHAN_MAP_TOKEN in _map_name(probe),
            timeout=12.0,
        )
        after_qingshan_path, _ = self.screenshot("real_flow_after_qingshan.png")

        before_move = _pawn_location(after_qingshan)
        window = self.input.find_preview_window()
        self.input.press_key(window, ord("D"), hold_seconds=0.75)
        time.sleep(0.5)
        after_move = self.probe()
        movement_distance = _distance(before_move, _pawn_location(after_move))
        self.event("movement_probe", distance=movement_distance)
        if movement_distance < 10.0:
            raise RuntimeError(f"D key did not move the hero enough; distance={movement_distance:.2f} cm")

        quest_npc = _quest_npc(after_move)
        quest_location = quest_npc.get("location") if isinstance(quest_npc.get("location"), dict) else {}
        if not quest_location:
            raise RuntimeError("Quest NPC was not found in the playable town map")
        window = self.input.find_preview_window()
        near_quest = self.walk_to_world_location(window, after_move, quest_location)
        self.input.press_key(window, ord("F"), hold_seconds=0.05)
        after_interact = self.wait_for("F accepts quest through real interaction overlap", _quest_interacted, timeout=5.0, interval=0.5)

        result = {
            "ok": True,
            "screenshots": {
                "before_start": str(before_start_path),
                "after_start": str(after_start_path),
                "after_qingshan": str(after_qingshan_path),
            },
            "movement_distance_cm": movement_distance,
            "quest_distance_cm": _distance(_pawn_location(near_quest), quest_location),
            "final_probe": after_interact,
            "events": self.events,
        }
        return result

    def close(self) -> None:
        if self.keep_pie:
            return
        try:
            if self.client.session_id and self.client.is_in_pie():
                self.client.stop_pie()
                self.event("stopped_pie")
        except Exception as exc:
            self.event("stop_pie_failed", error=str(exc))


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--keep-pie", action="store_true")
    parser.add_argument("--report", type=Path, default=None)
    args = parser.parse_args(argv)

    harness = RealFlowHarness(timeout=args.timeout, keep_pie=args.keep_pie)
    try:
        result = harness.run()
    except Exception as exc:
        result = {"ok": False, "error": str(exc), "events": harness.events}
        print(json.dumps(result, ensure_ascii=False, indent=2), flush=True)
        return_code = 1
    else:
        print(json.dumps(result, ensure_ascii=False, indent=2), flush=True)
        return_code = 0
    finally:
        harness.close()

    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    report_path = args.report or REPORT_DIR / f"gamexxk-real-play-flow-{time.strftime('%Y%m%d-%H%M%S')}.json"
    report_path.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"report": str(report_path), "ok": bool(result.get("ok"))}, ensure_ascii=False), flush=True)
    return return_code


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
