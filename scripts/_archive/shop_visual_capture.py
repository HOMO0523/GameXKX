"""PIE flow for shop visual review: main menu -> town -> open meta shop -> UMG screenshot.

Stops after the capture and keeps PIE open so the user can look at the live screen.
Prints the screenshot path as JSON on the last line.
"""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from gamexxk_real_play_flow_mcp import (
    RealFlowHarness,
    SCENE_TOOLSET,
    PROJECT_ROOT,
    _has_play_session,
    _is_qingshan_town,
    _load_json_from_probe,
    _widget_visible,
)
from ue_mcp_client import EDITOR_TOOLSET, UnrealMCPClient


def main() -> int:
    harness = RealFlowHarness(timeout=30.0, keep_pie=True)
    harness.preserve_default_save()
    harness.connect()
    client: UnrealMCPClient = harness.client

    if client.is_in_pie():
        client.stop_pie()
        if not client.wait_for_pie_state(False):
            raise RuntimeError("PIE did not stop before the shop capture flow")

    client.call_tool(
        "load_level",
        {"level_path": "/Game/GameXXK/Maps/L_Main"},
        toolset_name=SCENE_TOOLSET,
        timeout=60.0,
    )
    harness.probe("--delete-default-save")

    client.call_tool(
        "StartPIE",
        {"options": {"bSimulate": False, "playMode": 1, "warmupSeconds": 1.0}},
        toolset_name=EDITOR_TOOLSET,
        timeout=60.0,
    )
    harness.wait_for("PIE play session", _has_play_session)
    harness.preview_window = harness.input.find_preview_window()

    harness.wait_for("main menu visible", lambda p: _widget_visible(p, "main_menu"), timeout=15.0, interval=0.5)

    # Match the PSD page export aspect (1920x1080) before capturing
    client.run_project_python_file("Content/Python/gamexxk_probe_town_hud_pie.py", ["--setres"])
    time.sleep(1.5)

    harness.click_main_menu_start()
    harness.wait_for("StartGame opens the Qingshan town map", _is_qingshan_town, timeout=10.0, interval=0.5)
    time.sleep(1.0)

    shop_result = _load_json_from_probe(client.run_project_python_file("Content/Python/gamexxk_probe_open_meta_shop.py", []))
    if not shop_result.get("ok"):
        raise RuntimeError(f"Meta shop did not open: {shop_result}")
    harness.event("meta_shop_open_probe", **shop_result)
    time.sleep(1.0)

    path, size = harness.screenshot("shop_open_pie.png")

    # The Slate window capture is letterboxed/clipped (window is not 16:9), which
    # misleads visual review. Use the engine's HighResShot instead: a clean
    # 1920x1080 render with the UMG overlay baked in.
    shot_result = _load_json_from_probe(
        client.run_project_python_file("Content/Python/gamexxk_probe_town_hud_pie.py", ["--capture"])
    )
    if not isinstance(shot_result, dict) or not shot_result.get("captured"):
        raise RuntimeError(f"HighResShot failed: {shot_result}")
    time.sleep(2.0)
    shots_dir = PROJECT_ROOT / "Saved" / "Screenshots" / "WindowsEditor"
    candidates = sorted(shots_dir.glob("HighresScreenshot*.png"), key=lambda p: p.stat().st_mtime, reverse=True)
    if not candidates:
        raise RuntimeError("HighResShot produced no screenshot file")
    final = path.parent / "shop_open_pie_hires.png"
    candidates[0].replace(final)

    print(json.dumps({"ok": True, "screenshot": str(final), "raw": str(path), "size": list(size), "shop": shop_result}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
