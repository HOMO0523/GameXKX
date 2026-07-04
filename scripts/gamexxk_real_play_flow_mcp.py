#!/usr/bin/env python3
"""Drive the real GameXXK MVP entry flow through UE MCP and OS input."""

from __future__ import annotations

import argparse
import base64
import ctypes
import struct
import zlib
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
ACTIVE_WIDGETS_PROBE_SCRIPT = "Content/Python/gamexxk_probe_active_widgets.py"
MAIN_MAP = "/Game/GameXXK/Maps/L_Main"
QINGSHAN_MAP_TOKEN = "L_QingshanInn"
ROUTE_MAP_TOKEN = "L_RouteMap"
BATTLE_MAP_TOKEN = "L_Battle_1Game"
ONEGAME_BATTLE_PC_TOKEN = "BP_PlayerController_C"


def _png_size(data: bytes) -> tuple[int, int]:
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
        return 0, 0
    return struct.unpack(">II", data[16:24])


def _rgba_to_png(width: int, height: int, rgba: bytes) -> bytes:
    def chunk(kind: bytes, payload: bytes) -> bytes:
        return (
            struct.pack(">I", len(payload))
            + kind
            + payload
            + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
        )

    stride = width * 4
    raw = b"".join(b"\x00" + rgba[row * stride : (row + 1) * stride] for row in range(height))
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 6))
        + chunk(b"IEND", b"")
    )


def _load_json_from_probe(result: dict[str, Any]) -> dict[str, Any]:
    stdout = str(result.get("stdout", "")).strip()
    if not stdout:
        return {}
    last_line = stdout.splitlines()[-1]
    return json.loads(last_line)


def _runtime_screen(probe: dict[str, Any]) -> str:
    return str(probe.get("probe", {}).get("runtime_state", {}).get("screen", ""))


def _screen_contains(probe: dict[str, Any], token: str) -> bool:
    normalized_screen = _runtime_screen(probe).replace("_", "").upper()
    normalized_token = token.replace("_", "").upper()
    return normalized_token in normalized_screen


def _runtime_state(probe: dict[str, Any]) -> dict[str, Any]:
    state = probe.get("probe", {}).get("runtime_state", {})
    return state if isinstance(state, dict) else {}


def _save_state(probe: dict[str, Any]) -> dict[str, Any]:
    state = probe.get("probe", {}).get("save_state", {})
    return state if isinstance(state, dict) else {}


def _visible_commands(probe: dict[str, Any]) -> list[dict[str, Any]]:
    commands = probe.get("probe", {}).get("hud", {}).get("visible_commands", [])
    return commands if isinstance(commands, list) else []


def _flow_widgets(probe: dict[str, Any]) -> dict[str, Any]:
    widgets = probe.get("probe", {}).get("player_controller", {}).get("flow_widgets", {})
    return widgets if isinstance(widgets, dict) else {}


def _route_node_visual_states(probe: dict[str, Any]) -> list[dict[str, Any]]:
    route_widget = _flow_widgets(probe).get("route_map", {})
    if not isinstance(route_widget, dict):
        return []
    states = route_widget.get("route_node_visual_states", [])
    return states if isinstance(states, list) else []


def _route_node_visual_state(probe: dict[str, Any], node_id: int) -> dict[str, Any]:
    for state in _route_node_visual_states(probe):
        if int(state.get("node_id", -1)) == int(node_id):
            return state
    return {}


def _widget_visible(probe: dict[str, Any], name: str) -> bool:
    widget = _flow_widgets(probe).get(name, {})
    if not isinstance(widget, dict):
        return False
    visibility = str(widget.get("visibility", "")).upper()
    return bool(widget.get("is_in_viewport")) and ("VISIBLE" in visibility or widget.get("is_town_overlay_visible") or widget.get("is_battle_board_visible"))


def _has_visible_command(probe: dict[str, Any], command_name: str, enabled: bool | None = None) -> bool:
    for command in _visible_commands(probe):
        if str(command.get("command_name", "")) != command_name:
            continue
        if enabled is None:
            return True
        return bool(command.get("b_enabled")) is enabled
    return False


def _map_name(probe: dict[str, Any]) -> str:
    return str(probe.get("probe", {}).get("map_name", ""))


def _pawn_location(probe: dict[str, Any]) -> dict[str, float]:
    location = probe.get("probe", {}).get("pawn", {}).get("location", {})
    return location if isinstance(location, dict) else {}


def _pawn_state(probe: dict[str, Any]) -> dict[str, Any]:
    pawn = probe.get("probe", {}).get("pawn", {})
    return pawn if isinstance(pawn, dict) else {}


def _current_flipbook(probe: dict[str, Any]) -> str:
    return str(_pawn_state(probe).get("current_flipbook", ""))


def _is_town_moving(probe: dict[str, Any]) -> bool:
    return bool(_pawn_state(probe).get("is_town_moving"))


def _actors(probe: dict[str, Any]) -> list[dict[str, Any]]:
    actors = probe.get("probe", {}).get("actors", [])
    return actors if isinstance(actors, list) else []


def _quest_npc(probe: dict[str, Any]) -> dict[str, Any]:
    for actor in _actors(probe):
        if str(actor.get("get_npc_role", "")).upper().endswith("QUEST"):
            return actor
    return {}


def _town_exit(probe: dict[str, Any]) -> dict[str, Any]:
    for actor in _actors(probe):
        label = str(actor.get("label", ""))
        class_name = str(actor.get("class", ""))
        if label == "QingshanInn_TownExit" or "TownExit" in class_name:
            return actor
    return {}


def _npc_by_role(probe: dict[str, Any], role: str) -> dict[str, Any]:
    role_token = role.upper()
    for actor in _actors(probe):
        if str(actor.get("get_npc_role", "")).upper().endswith(role_token):
            return actor
    return {}


def _expect_npc_visual(actor: dict[str, Any], class_token: str, flipbook_token: str) -> dict[str, Any]:
    body = actor.get("body_character", {})
    if not isinstance(body, dict):
        body = {}
    visual = body or actor.get("visual_character", {})
    if not isinstance(visual, dict):
        visual = {}
    visual_component = visual.get("visual", {})
    if not isinstance(visual_component, dict):
        visual_component = {}
    class_text = " ".join(
        str(value)
        for value in (
            actor.get("class", ""),
            actor.get("name", ""),
            body.get("class", ""),
            body.get("path", ""),
            actor.get("visual_character_class", ""),
            visual.get("class", ""),
            visual.get("path", ""),
        )
    )
    flipbook_text = " ".join(
        str(value)
        for value in (
            visual.get("current_flipbook", ""),
            visual.get("get_default_town_flipbook_path_string", ""),
            visual_component.get("flipbook", ""),
        )
    )
    collision_text = str(visual.get("collision_enabled", ""))
    body_location = visual.get("location", {})
    if not isinstance(body_location, dict):
        body_location = {}
    grounded_root_z = float(body_location.get("z", 0.0) or 0.0)
    ok = bool(
        class_token in class_text
        and flipbook_token in flipbook_text
        and bool(visual.get("has_assigned_town_flipbook")) is True
        and bool(visual.get("is_town_moving")) is False
        and grounded_root_z >= 60.0
    )
    return {
        "ok": ok,
        "actor": actor.get("name", ""),
        "role": actor.get("get_npc_role", ""),
        "expected_class_token": class_token,
        "expected_flipbook_token": flipbook_token,
        "class_text": class_text,
        "flipbook_text": flipbook_text,
        "collision_enabled": collision_text,
        "actor_tick_enabled": visual.get("actor_tick_enabled"),
        "body_class": body.get("class", ""),
        "grounded_root_z": grounded_root_z,
    }


def _npc_visual_state(probe: dict[str, Any]) -> dict[str, Any]:
    checks = [
        _expect_npc_visual(_npc_by_role(probe, "QUEST"), "BP_NpcCharacter_C", "FB_Npc_Idle_South"),
        _expect_npc_visual(_npc_by_role(probe, "MERCHANT"), "BP_MerchantCharacter_C", "FB_Merchant_Idle_South"),
        _expect_npc_visual(_npc_by_role(probe, "FOLLOWER"), "BP_NpcCharacter_C", "FB_Npc_Idle_South"),
    ]
    return {
        "ok": all(bool(check.get("ok")) for check in checks),
        "checks": checks,
    }


def _quest_interacted(probe: dict[str, Any]) -> bool:
    npc = _quest_npc(probe)
    return bool(npc.get("was_last_interaction_successful") and npc.get("is_follower_active"))


def _expect_visual_state(probe: dict[str, Any], expected_state: str, expected_direction: str) -> dict[str, Any]:
    flipbook = _current_flipbook(probe)
    expected_token = f"/FB_Hero_{expected_state}_{expected_direction}."
    moving = _is_town_moving(probe)
    visual_rotation = ((_pawn_state(probe).get("visual") or {}).get("relative_rotation") or {})
    ok = expected_token in flipbook
    if expected_state == "Walk":
        ok = ok and moving
    if expected_state == "Idle":
        ok = ok and not moving
    try:
        ok = ok and abs(float(visual_rotation.get("yaw", 0.0)) - 90.0) <= 0.1
    except (TypeError, ValueError):
        ok = False
    return {
        "ok": bool(ok),
        "expected_state": expected_state,
        "expected_direction": expected_direction,
        "current_flipbook": flipbook,
        "is_town_moving": moving,
        "visual_yaw": visual_rotation.get("yaw"),
    }


def _has_play_session(probe: dict[str, Any]) -> bool:
    data = probe.get("probe", {})
    return bool(
        data.get("has_pie_world")
        and data.get("player_controller", {}).get("class", "").endswith("GameXXKMVPPlayerController")
        and data.get("hud", {}).get("class", "").endswith("GameXXKMVPHUD")
        and data.get("pawn", {}).get("class", "")
    )


def _topdown_camera_state(probe: dict[str, Any]) -> dict[str, Any]:
    pawn = probe.get("probe", {}).get("pawn", {})
    if not isinstance(pawn, dict):
        return {"ok": False, "reason": "pawn_missing"}
    camera = pawn.get("camera", {})
    spring_arm = pawn.get("spring_arm", {})
    if not isinstance(camera, dict) or not isinstance(spring_arm, dict):
        return {"ok": False, "reason": "camera_or_spring_arm_missing"}

    projection = str(camera.get("projection_mode", "")).upper()
    target_arm_length = float(spring_arm.get("target_arm_length") or 0.0)
    pitch = float((spring_arm.get("relative_rotation") or {}).get("pitch") or 0.0)
    camera_name = str(camera.get("name", ""))
    boom_name = str(spring_arm.get("name", ""))
    ok = bool(
        camera_name == "TopDownCamera"
        and boom_name == "CameraBoom"
        and "PERSPECTIVE" in projection
        and 760.0 <= target_arm_length <= 840.0
        and -65.0 <= pitch <= -55.0
        and bool(spring_arm.get("absolute_rotation")) is True
        and bool(spring_arm.get("do_collision_test")) is False
    )
    return {
        "ok": ok,
        "camera_name": camera_name,
        "boom_name": boom_name,
        "projection": projection,
        "target_arm_length": target_arm_length,
        "spring_arm_pitch": pitch,
        "absolute_rotation": bool(spring_arm.get("absolute_rotation")),
        "do_collision_test": bool(spring_arm.get("do_collision_test")),
    }


def _distance(a: dict[str, float], b: dict[str, float]) -> float:
    return math.sqrt(sum((float(a.get(axis, 0.0)) - float(b.get(axis, 0.0))) ** 2 for axis in ("x", "y", "z")))


class PreviewInput:
    def __init__(self) -> None:
        self.user32 = ctypes.windll.user32
        self.gdi32 = ctypes.windll.gdi32
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

    def capture_window_png(self, window: dict[str, Any]) -> tuple[bytes, tuple[int, int]]:
        class BitmapInfoHeader(ctypes.Structure):
            _fields_ = [
                ("biSize", ctypes.c_uint32),
                ("biWidth", ctypes.c_int32),
                ("biHeight", ctypes.c_int32),
                ("biPlanes", ctypes.c_uint16),
                ("biBitCount", ctypes.c_uint16),
                ("biCompression", ctypes.c_uint32),
                ("biSizeImage", ctypes.c_uint32),
                ("biXPelsPerMeter", ctypes.c_int32),
                ("biYPelsPerMeter", ctypes.c_int32),
                ("biClrUsed", ctypes.c_uint32),
                ("biClrImportant", ctypes.c_uint32),
            ]

        class BitmapInfo(ctypes.Structure):
            _fields_ = [("bmiHeader", BitmapInfoHeader), ("bmiColors", ctypes.c_uint32 * 3)]

        left, top, right, bottom = [int(v) for v in window["rect"]]
        width = max(0, right - left)
        height = max(0, bottom - top)
        if width <= 0 or height <= 0:
            raise RuntimeError(f"Cannot capture GameXXK Preview: invalid rect={window['rect']}")

        hwnd = ctypes.c_void_p(int(window["hwnd"]))
        self.focus(window)
        source_dc = self.user32.GetWindowDC(hwnd)
        if not source_dc:
            raise RuntimeError("GetWindowDC failed for GameXXK Preview")
        memory_dc = self.gdi32.CreateCompatibleDC(source_dc)
        bitmap = self.gdi32.CreateCompatibleBitmap(source_dc, width, height)
        previous = self.gdi32.SelectObject(memory_dc, bitmap)
        try:
            try:
                self.user32.PrintWindow.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint]
                self.user32.PrintWindow.restype = ctypes.c_bool
                ok = bool(self.user32.PrintWindow(hwnd, memory_dc, 0x00000002))
            except Exception:
                ok = False
            if not ok:
                ok = bool(self.gdi32.BitBlt(memory_dc, 0, 0, width, height, source_dc, 0, 0, 0x00CC0020))
            if not ok:
                raise RuntimeError("Window capture failed for GameXXK Preview")
            info = BitmapInfo()
            info.bmiHeader.biSize = ctypes.sizeof(BitmapInfoHeader)
            info.bmiHeader.biWidth = width
            info.bmiHeader.biHeight = -height
            info.bmiHeader.biPlanes = 1
            info.bmiHeader.biBitCount = 32
            info.bmiHeader.biCompression = 0
            buffer = (ctypes.c_ubyte * (width * height * 4))()
            lines = self.gdi32.GetDIBits(source_dc, bitmap, 0, height, ctypes.byref(buffer), ctypes.byref(info), 0)
            if lines != height:
                raise RuntimeError(f"GetDIBits captured {lines}/{height} lines")
            bgra = bytes(buffer)
            rgba = bytearray(len(bgra))
            rgba[0::4] = bgra[2::4]
            rgba[1::4] = bgra[1::4]
            rgba[2::4] = bgra[0::4]
            rgba[3::4] = b"\xff" * (width * height)
            return _rgba_to_png(width, height, bytes(rgba)), (width, height)
        finally:
            if previous:
                self.gdi32.SelectObject(memory_dc, previous)
            if bitmap:
                self.gdi32.DeleteObject(bitmap)
            if memory_dc:
                self.gdi32.DeleteDC(memory_dc)
            self.user32.ReleaseDC(hwnd, source_dc)

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

    def click_screen_point(self, window: dict[str, Any], screen_x: int, screen_y: int) -> dict[str, int]:
        self.focus(window)
        self.user32.SetCursorPos(int(screen_x), int(screen_y))
        time.sleep(0.1)
        self.user32.mouse_event(0x0002, 0, 0, 0, 0)
        time.sleep(0.05)
        self.user32.mouse_event(0x0004, 0, 0, 0, 0)
        return {"x": int(screen_x), "y": int(screen_y)}

    def press_key(self, window: dict[str, Any], virtual_key: int, hold_seconds: float = 0.0) -> None:
        self.focus(window)
        self.user32.keybd_event(virtual_key, 0, 0, 0)
        time.sleep(max(0.0, hold_seconds))
        self.user32.keybd_event(virtual_key, 0, 0x0002, 0)

    def key_down(self, window: dict[str, Any], virtual_key: int) -> None:
        self.focus(window)
        self.user32.keybd_event(virtual_key, 0, 0, 0)

    def key_up(self, virtual_key: int) -> None:
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

    def hud_command(self, command_name: str) -> dict[str, Any]:
        result = self.client.run_project_python_file(PROBE_SCRIPT, ["--hud-command", command_name])
        parsed = _load_json_from_probe(result)
        command_result = parsed.get("hud_command", {})
        if not isinstance(command_result, dict):
            command_result = {}
        self.event("hud_command", command=command_name, ok=bool(command_result.get("ok")), detail=command_result)
        if not command_result.get("ok"):
            raise RuntimeError(f"HUD command {command_name} failed: {command_result}")
        return parsed

    def main_menu_start(self) -> dict[str, Any]:
        result = self.client.run_project_python_file(PROBE_SCRIPT, ["--main-menu-start"])
        parsed = _load_json_from_probe(result)
        command_result = parsed.get("main_menu_start", {})
        if not isinstance(command_result, dict):
            command_result = {}
        self.event("main_menu_start", ok=bool(command_result.get("ok")), detail=command_result)
        if not command_result.get("ok"):
            raise RuntimeError(f"Main menu StartGame failed: {command_result}")
        return parsed

    def town_command(self, command_name: str) -> dict[str, Any]:
        result = self.client.run_project_python_file(PROBE_SCRIPT, ["--town-command", command_name])
        parsed = _load_json_from_probe(result)
        command_result = parsed.get("town_command", {})
        if not isinstance(command_result, dict):
            command_result = {}
        self.event("town_command", command=command_name, ok=bool(command_result.get("ok")), detail=command_result)
        if not command_result.get("ok"):
            raise RuntimeError(f"Town widget command {command_name} failed: {command_result}")
        return parsed

    def route_node(self, node_index: int) -> dict[str, Any]:
        result = self.client.run_project_python_file(PROBE_SCRIPT, ["--route-node", str(node_index)])
        parsed = _load_json_from_probe(result)
        command_result = parsed.get("route_node", {})
        if not isinstance(command_result, dict):
            command_result = {}
        self.event("route_node", node_index=node_index, ok=bool(command_result.get("ok")), detail=command_result)
        if not command_result.get("ok"):
            raise RuntimeError(f"Route widget node {node_index} failed: {command_result}")
        return parsed

    def click_route_node(self, probe: dict[str, Any], node_id: int) -> dict[str, Any]:
        node_state = _route_node_visual_state(probe, node_id)
        if not node_state:
            raise RuntimeError(f"Route node {node_id} visual state was not found: {_route_node_visual_states(probe)}")
        if not bool(node_state.get("b_enabled")):
            raise RuntimeError(f"Route node {node_id} is not enabled for screen click: {node_state}")
        center = node_state.get("viewport_hit_box_center", {})
        if not isinstance(center, dict) or "x" not in center or "y" not in center:
            raise RuntimeError(f"Route node {node_id} missing viewport hit center: {node_state}")
        window = self.input.find_preview_window()
        click = self.input.click_screen_point(window, int(center["x"]), int(center["y"]))
        self.event("route_node_screen_click", node_id=node_id, click=click, node_state=node_state)
        time.sleep(0.45)
        return self.probe()

    def screenshot(self, name: str) -> tuple[Path, tuple[int, int]]:
        window = self.input.find_preview_window()
        data, size = self.input.capture_window_png(window)
        SCREENSHOT_DIR.mkdir(parents=True, exist_ok=True)
        path = SCREENSHOT_DIR / name
        path.write_bytes(data)
        if size[0] <= 0 or size[1] <= 0:
            raise RuntimeError(f"Captured invalid screenshot size for {name}: {size}")
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

        before_start_probe = self.probe()
        main_menu_probe = {
            "ok": _widget_visible(before_start_probe, "main_menu"),
            "widgets": _flow_widgets(before_start_probe),
        }
        self.event("main_menu_widget_probe", **main_menu_probe)
        if not main_menu_probe["ok"]:
            raise RuntimeError(f"L_Main PIE did not show the player main menu: {main_menu_probe}")

        before_start_path, _ = self.screenshot("real_flow_before_start.png")
        self.main_menu_start()
        after_qingshan = self.wait_for(
            "StartGame opens Qingshan town map",
            lambda probe: QINGSHAN_MAP_TOKEN in _map_name(probe) and _screen_contains(probe, "Town"),
            timeout=10.0,
            interval=0.5,
        )
        if bool(_save_state(after_qingshan).get("exists")):
            raise RuntimeError(f"Start click should not create a save before manual Save; probe={json.dumps(after_qingshan, ensure_ascii=False)}")
        town_widget_probe = {
            "ok": _widget_visible(after_qingshan, "town_overlay") and not _widget_visible(after_qingshan, "main_menu"),
            "widgets": _flow_widgets(after_qingshan),
        }
        self.event("town_widget_probe", **town_widget_probe)
        if not town_widget_probe["ok"]:
            raise RuntimeError(f"PlayerController town UI was not visible after Start OpenLevel: {town_widget_probe}")
        camera_state = _topdown_camera_state(after_qingshan)
        self.event("topdown_camera_probe", **camera_state)
        if not camera_state.get("ok"):
            raise RuntimeError(f"BP_HeroCharacter top-down camera is not active/configured: {camera_state}")
        initial_idle_state = _expect_visual_state(after_qingshan, "Idle", "South")
        self.event("initial_idle_probe", **initial_idle_state)
        if not initial_idle_state.get("ok"):
            raise RuntimeError(f"BP_HeroCharacter did not enter Idle_South after town load: {initial_idle_state}")
        npc_visual_state = _npc_visual_state(after_qingshan)
        self.event("npc_visual_probe", **npc_visual_state)
        if not npc_visual_state.get("ok"):
            raise RuntimeError(f"Town NPC visual blueprints are not attached or idle: {npc_visual_state}")
        after_qingshan_path, _ = self.screenshot("real_flow_after_qingshan.png")

        before_move = _pawn_location(after_qingshan)
        window = self.input.find_preview_window()
        self.input.key_down(window, ord("D"))
        time.sleep(0.25)
        while_d_down = self.probe()
        walk_state = _expect_visual_state(while_d_down, "Walk", "East")
        self.event("walk_state_probe", **walk_state)
        if not walk_state.get("ok"):
            self.input.key_up(ord("D"))
            raise RuntimeError(f"D key did not switch hero to Walk_East while held: {walk_state}")
        time.sleep(0.5)
        self.input.key_up(ord("D"))
        time.sleep(0.25)
        after_move = self.probe()
        released_idle_state = _expect_visual_state(after_move, "Idle", "East")
        self.event("released_idle_probe", **released_idle_state)
        if not released_idle_state.get("ok"):
            raise RuntimeError(f"D key release did not switch hero back to Idle_East: {released_idle_state}")
        window = self.input.find_preview_window()
        self.input.key_down(window, ord("W"))
        self.input.key_down(window, ord("D"))
        time.sleep(0.25)
        while_diagonal_down = self.probe()
        diagonal_walk_state = _expect_visual_state(while_diagonal_down, "Walk", "NorthEast")
        self.event("diagonal_walk_state_probe", **diagonal_walk_state)
        if not diagonal_walk_state.get("ok"):
            self.input.key_up(ord("W"))
            self.input.key_up(ord("D"))
            raise RuntimeError(f"W+D did not switch hero to Walk_NorthEast while held: {diagonal_walk_state}")
        time.sleep(0.25)
        self.input.key_up(ord("W"))
        self.input.key_up(ord("D"))
        time.sleep(0.25)
        after_move = self.probe()
        diagonal_released_idle_state = _expect_visual_state(after_move, "Idle", "NorthEast")
        self.event("diagonal_released_idle_probe", **diagonal_released_idle_state)
        if not diagonal_released_idle_state.get("ok"):
            raise RuntimeError(f"W+D release did not switch hero back to Idle_NorthEast: {diagonal_released_idle_state}")
        window = self.input.find_preview_window()
        self.input.key_down(window, ord("W"))
        self.input.key_down(window, ord("D"))
        time.sleep(0.25)
        delayed_diagonal_down = self.probe()
        delayed_diagonal_walk_state = _expect_visual_state(delayed_diagonal_down, "Walk", "NorthEast")
        self.event("delayed_diagonal_walk_state_probe", **delayed_diagonal_walk_state)
        if not delayed_diagonal_walk_state.get("ok"):
            self.input.key_up(ord("W"))
            self.input.key_up(ord("D"))
            raise RuntimeError(f"W+D did not switch hero to Walk_NorthEast before delayed release: {delayed_diagonal_walk_state}")
        self.input.key_up(ord("W"))
        time.sleep(0.08)
        self.input.key_up(ord("D"))
        time.sleep(0.25)
        after_move = self.probe()
        delayed_diagonal_released_idle_state = _expect_visual_state(after_move, "Idle", "East")
        self.event("delayed_diagonal_released_idle_probe", **delayed_diagonal_released_idle_state)
        if not delayed_diagonal_released_idle_state.get("ok"):
            raise RuntimeError(f"W then delayed D release should switch hero back to Idle_East: {delayed_diagonal_released_idle_state}")
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
        after_interact: dict[str, Any] = {}
        interact_probe: dict[str, Any] = near_quest
        for attempt in range(3):
            self.input.press_key(window, ord("F"), hold_seconds=0.08)
            self.event("pressed_interact", attempt=attempt + 1)
            deadline = time.monotonic() + 2.0
            while time.monotonic() < deadline:
                interact_probe = self.probe()
                if _quest_interacted(interact_probe):
                    after_interact = interact_probe
                    self.event("wait_ok", label="F accepts quest through real interaction overlap", attempt=attempt + 1)
                    break
                time.sleep(0.35)
            if after_interact:
                break
        if not after_interact:
            raise RuntimeError(f"Timed out waiting for F accepts quest through real interaction overlap; last probe={json.dumps(interact_probe, ensure_ascii=False)}")
        quest_after_interact = _quest_npc(after_interact)
        quest_location_after_interact = quest_after_interact.get("location") if isinstance(quest_after_interact.get("location"), dict) else {}
        if not quest_location_after_interact:
            raise RuntimeError("Quest NPC location missing after accepting quest")
        window = self.input.find_preview_window()
        self.input.key_down(window, ord("D"))
        try:
            time.sleep(0.75)
            after_follower_input_move = self.probe()
        finally:
            self.input.key_up(ord("D"))
        time.sleep(0.15)
        quest_after_follower_input_move = _quest_npc(after_follower_input_move)
        quest_location_after_follower_input_move = (
            quest_after_follower_input_move.get("location")
            if isinstance(quest_after_follower_input_move.get("location"), dict)
            else {}
        )
        quest_body_after_follower_input_move = quest_after_follower_input_move.get("body_character", {})
        if not isinstance(quest_body_after_follower_input_move, dict):
            quest_body_after_follower_input_move = {}
        quest_body_flipbook = str(quest_body_after_follower_input_move.get("current_flipbook", ""))
        quest_body_moving = bool(quest_body_after_follower_input_move.get("is_town_moving"))
        save_after_follower_input_move = _save_state(after_follower_input_move)
        quest_follower_input_distance = _distance(quest_location_after_interact, quest_location_after_follower_input_move)
        quest_follower_input_probe = {
            "ok": (
                quest_follower_input_distance >= 10.0
                and quest_body_moving
                and "/FB_Npc_Walk_East." in quest_body_flipbook
                and not bool(save_after_follower_input_move.get("exists"))
            ),
            "distance": quest_follower_input_distance,
            "before": quest_location_after_interact,
            "after": quest_location_after_follower_input_move,
            "body_is_town_moving": quest_body_moving,
            "body_current_flipbook": quest_body_flipbook,
            "save_exists_before_manual_save": save_after_follower_input_move.get("exists"),
        }
        self.event("quest_follower_range_chase_probe", **quest_follower_input_probe)
        if not quest_follower_input_probe["ok"]:
            raise RuntimeError(f"Quest follower did not chase with walk animation or saved before manual save: {quest_follower_input_probe}")

        after_manual_save = self.town_command("SaveSlot1")
        quest_after_manual_save = _quest_npc(after_manual_save)
        quest_location_after_manual_save = (
            quest_after_manual_save.get("location")
            if isinstance(quest_after_manual_save.get("location"), dict)
            else {}
        )
        save_after_manual = _save_state(after_manual_save)
        manual_saved_location = save_after_manual.get("quest_npc_location", {})
        if not isinstance(manual_saved_location, dict):
            manual_saved_location = {}
        manual_saved_distance = _distance(manual_saved_location, quest_location_after_manual_save)
        player_location_after_manual_save = _pawn_location(after_manual_save)
        manual_saved_player_location = save_after_manual.get("player_location", {})
        if not isinstance(manual_saved_player_location, dict):
            manual_saved_player_location = {}
        manual_saved_player_distance = _distance(manual_saved_player_location, player_location_after_manual_save)
        manual_save_probe = {
            "ok": (
                bool(save_after_manual.get("exists"))
                and bool(save_after_manual.get("b_has_player_location"))
                and bool(save_after_manual.get("b_follower_joined"))
                and bool(save_after_manual.get("b_has_quest_npc_location"))
                and manual_saved_player_distance <= 5.0
                and manual_saved_distance <= 5.0
            ),
            "saved_has_player_location": save_after_manual.get("b_has_player_location"),
            "saved_player_location": manual_saved_player_location,
            "player_location": player_location_after_manual_save,
            "saved_player_location_distance": manual_saved_player_distance,
            "saved_follower_joined": save_after_manual.get("b_follower_joined"),
            "saved_has_quest_npc_location": save_after_manual.get("b_has_quest_npc_location"),
            "saved_quest_npc_location": manual_saved_location,
            "quest_npc_location": quest_location_after_manual_save,
            "saved_quest_npc_location_distance": manual_saved_distance,
        }
        self.event("manual_save_probe", **manual_save_probe)
        if not manual_save_probe["ok"]:
            raise RuntimeError(f"Manual SaveGame did not persist player, follower state, and moved NPC location: {manual_save_probe}")

        town_exit = _town_exit(after_manual_save)
        town_exit_location = town_exit.get("location") if isinstance(town_exit.get("location"), dict) else {}
        if not town_exit_location:
            raise RuntimeError(f"Town route entrance QingshanInn_TownExit was not present in the real town actors: {_actors(after_manual_save)}")
        town_exit_approach_location = dict(town_exit_location)
        town_exit_approach_location["y"] = float(town_exit_approach_location.get("y", 0.0) or 0.0) - 320.0
        near_town_exit = self.walk_to_world_location(window, after_manual_save, town_exit_approach_location)
        self.event(
            "town_exit_walk_probe",
            town_exit=town_exit,
            approach_location=town_exit_approach_location,
            distance_to_approach=_distance(_pawn_location(near_town_exit), town_exit_approach_location),
        )

        after_route_map: dict[str, Any] = {}
        town_exit_interact_probe: dict[str, Any] = near_town_exit
        for attempt in range(4):
            self.input.press_key(window, ord("F"), hold_seconds=0.08)
            self.event("pressed_town_exit_interact", attempt=attempt + 1)
            time.sleep(0.35)
            town_exit_interact_probe = self.probe()
            if _screen_contains(town_exit_interact_probe, "DungeonMap") and ROUTE_MAP_TOKEN in _map_name(town_exit_interact_probe):
                after_route_map = town_exit_interact_probe
                self.event("wait_ok", label="F enters route map through QingshanInn_TownExit", attempt=attempt + 1)
                break
        if not after_route_map:
            raise RuntimeError(f"Timed out waiting for F to enter the Slay-the-Spire route map through QingshanInn_TownExit; last probe={json.dumps(town_exit_interact_probe, ensure_ascii=False)}")
        route_runtime = _runtime_state(after_route_map)
        route_node_states = _route_node_visual_states(after_route_map)
        route_start_state = _route_node_visual_state(after_route_map, 0)
        route_map_probe = {
            "ok": (
                _screen_contains(after_route_map, "DungeonMap")
                and ROUTE_MAP_TOKEN in _map_name(after_route_map)
                and bool(route_runtime.get("b_dungeon_active"))
                and _widget_visible(after_route_map, "route_map")
                and bool(route_node_states)
                and bool(route_start_state.get("b_enabled"))
                and isinstance(route_start_state.get("viewport_hit_box_center"), dict)
            ),
            "map": _map_name(after_route_map),
            "screen": route_runtime.get("screen"),
            "b_dungeon_active": route_runtime.get("b_dungeon_active"),
            "dungeon_node_index": route_runtime.get("dungeon_node_index"),
            "visible_commands": _visible_commands(after_route_map),
            "widgets": _flow_widgets(after_route_map),
            "route_node_visual_states": route_node_states,
            "town_exit": town_exit,
            "town_exit_approach_location": town_exit_approach_location,
            "town_exit_approach_distance": _distance(_pawn_location(near_town_exit), town_exit_approach_location),
        }
        self.event("route_map_probe", **route_map_probe)
        if not route_map_probe["ok"]:
            raise RuntimeError(f"Town exit F interaction did not open the Slay-the-Spire route map screen: {route_map_probe}")
        after_route_map_path, _ = self.screenshot("real_flow_after_route_map.png")

        after_start_node = self.click_route_node(after_route_map, 0)
        start_node_runtime = _runtime_state(after_start_node)
        start_node_probe = {
            "ok": _screen_contains(after_start_node, "DungeonMap") and int(start_node_runtime.get("dungeon_node_index") or 0) >= 1,
            "screen": start_node_runtime.get("screen"),
            "dungeon_node_index": start_node_runtime.get("dungeon_node_index"),
        }
        self.event("route_start_node_probe", **start_node_probe)
        if not start_node_probe["ok"]:
            raise RuntimeError(f"Route start node did not advance route map: {start_node_probe}")

        battle_click_probe = self.click_route_node(after_start_node, 1)
        after_battle: dict[str, Any] = battle_click_probe
        for attempt in range(8):
            if BATTLE_MAP_TOKEN in _map_name(after_battle):
                self.event("wait_ok", label="Battle route node opens original 1Game island", attempt=attempt + 1)
                break
            time.sleep(0.35)
            after_battle = self.probe()
        battle_runtime = _runtime_state(after_battle)
        active_widgets_probe = _load_json_from_probe(self.client.run_project_python_file(ACTIVE_WIDGETS_PROBE_SCRIPT))
        active_player_controller = active_widgets_probe.get("player_controller", {})
        if not isinstance(active_player_controller, dict):
            active_player_controller = {}
        battle_probe = {
            "ok": (
                BATTLE_MAP_TOKEN in _map_name(after_battle)
                and ONEGAME_BATTLE_PC_TOKEN in str(active_player_controller.get("class_name", ""))
            ),
            "map": _map_name(after_battle),
            "screen": battle_runtime.get("screen"),
            "widgets": _flow_widgets(after_battle),
            "active_player_controller": active_player_controller,
            "onegame_route_widgets": active_widgets_probe.get("onegame_route_widgets", []),
            "click_probe_map": _map_name(battle_click_probe),
        }
        self.event("battle_1game_island_probe", **battle_probe)
        if not battle_probe["ok"]:
            raise RuntimeError(f"Route battle node did not open the original 1Game island: {battle_probe}")
        after_battle_path, _ = self.screenshot("real_flow_after_battle.png")

        result = {
            "ok": True,
            "screenshots": {
                "before_start": str(before_start_path),
                "after_qingshan": str(after_qingshan_path),
                "after_route_map": str(after_route_map_path),
                "after_battle": str(after_battle_path),
            },
            "topdown_camera": camera_state,
            "npc_visuals": npc_visual_state,
            "movement_distance_cm": movement_distance,
            "quest_distance_cm": _distance(_pawn_location(near_quest), quest_location),
            "quest_follower_input_distance_cm": quest_follower_input_distance,
            "manual_save": manual_save_probe,
            "route_map": route_map_probe,
            "battle": battle_probe,
            "final_probe": after_battle,
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
        try:
            if self.client.session_id:
                cleanup_result = self.client.run_project_python_file(PROBE_SCRIPT, ["--delete-default-save"])
                cleanup_payload = _load_json_from_probe(cleanup_result)
                self.event("deleted_default_save_after_real_flow", result=cleanup_payload.get("delete_default_save"))
        except Exception as exc:
            self.event("delete_default_save_after_real_flow_failed", error=str(exc))


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
