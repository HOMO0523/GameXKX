"""Transient driver: capture the editor main window (embedded PIE viewport) via win32."""

import ctypes
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from gamexxk_real_play_flow_mcp import PreviewWindowController  # noqa: E402
from ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, UnrealMCPClient  # noqa: E402

PORT = 12345


def _run_python(client, relative_path, argv=None):
    return client.run_project_python_file(relative_path, argv or [], True)


def find_window_by_title(title_token: str) -> dict:
    u32 = ctypes.windll.user32
    proc = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

    class Rect(ctypes.Structure):
        _fields_ = [
            ("left", ctypes.c_long),
            ("top", ctypes.c_long),
            ("right", ctypes.c_long),
            ("bottom", ctypes.c_long),
        ]

    matches = []

    def cb(hwnd, _lparam):
        if not u32.IsWindowVisible(hwnd):
            return True
        length = u32.GetWindowTextLengthW(hwnd)
        if length <= 0:
            return True
        buffer = ctypes.create_unicode_buffer(length + 1)
        u32.GetWindowTextW(hwnd, buffer, length + 1)
        if title_token in buffer.value:
            rect = Rect()
            u32.GetWindowRect(hwnd, ctypes.byref(rect))
            matches.append({
                "hwnd": int(hwnd),
                "title": buffer.value,
                "rect": [rect.left, rect.top, rect.right, rect.bottom],
            })
        return True

    u32.EnumWindows(proc(cb), 0)
    if not matches:
        raise RuntimeError(f"window with title token {title_token!r} not found")
    return matches[0]


def main() -> int:
    client = UnrealMCPClient(host=DEFAULT_HOST, port=PORT, path=DEFAULT_PATH, timeout=300.0)
    deadline = time.time() + 240
    while time.time() < deadline:
        if client.connect():
            break
        time.sleep(5)
    if not client.connect():
        print(json.dumps({"ok": False, "error": "mcp_timeout"}))
        return 1

    out = {}
    client.stop_pie()
    time.sleep(3.0)
    out["pie_start"] = client.start_pie(warmup_seconds=2.0)
    time.sleep(6.0)
    _run_python(client, "Content/Python/gamexxk_pilot_2k_session.py", ["real_entry"])
    time.sleep(3.0)
    out["fixture"] = (_run_python(client, "Content/Python/gamexxk_pilot_2k_session.py", ["fixture"]) or {}).get("stdout", "")[:400]
    _run_python(client, "Content/Python/gamexxk_pilot_2k_session.py", ["idle_shot"])  # refresh board + settle
    time.sleep(4.0)

    window = find_window_by_title("GameXXK - Unreal Editor")
    out["window"] = {"title": window.get("title"), "rect": window.get("rect")}
    controller = PreviewWindowController()
    data, size = controller.capture_window_png(window)
    target = ROOT / "Saved" / "Codex" / "pilot_compare_editor_window.png"
    target.write_bytes(data)
    out["shot"] = str(target)
    out["size"] = list(size)
    out["bytes"] = len(data)
    client.stop_pie()
    print(json.dumps(out, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
