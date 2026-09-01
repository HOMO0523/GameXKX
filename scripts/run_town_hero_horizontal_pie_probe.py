#!/usr/bin/env python3
"""Verify the live town hero locomotion and authored one-shot states through UE MCP."""

from __future__ import annotations

import json
import re
import sys
import time
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_ROOT = PROJECT_ROOT / "scripts"
if str(SCRIPT_ROOT) not in sys.path:
    sys.path.insert(0, str(SCRIPT_ROOT))

from gamexxk_content_assembly_check import parse_stdout_json  # noqa: E402
from gamexxk_real_play_flow_mcp import (  # noqa: E402
    SLATE_TOOLSET,
    _decode_slate_screenshot_png,
    _png_size,
    _slate_preview_window_ref,
)
from ue_mcp_client import UnrealMCPClient  # noqa: E402


TOWN_LOADER = "Content/Python/gamexxk_probe_town_hud_pie.py"
LOCOMOTION_PROBE = "Content/Python/gamexxk_probe_town_hero_locomotion.py"
REPORT_PATH = PROJECT_ROOT / "Saved/HarnessReports/town-hero-horizontal/pie-probe.json"
EXPECTED_IDLE = "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_Idle_Left.FB_Hero_Town_Idle_Left"
EXPECTED_START = "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_WalkStart_Left.FB_Hero_Town_WalkStart_Left"
EXPECTED_LOOP = "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_WalkLoop_Left.FB_Hero_Town_WalkLoop_Left"
EXPECTED_STOP = "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_WalkStop_Left.FB_Hero_Town_WalkStop_Left"
EXPECTED_BREATH = "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_DeepBreath_Left.FB_Hero_Town_DeepBreath_Left"
EXPECTED_BACKPACK = "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_AdjustBackpack_Left.FB_Hero_Town_AdjustBackpack_Left"


def run_probe(client: UnrealMCPClient, *argv: str) -> dict[str, Any]:
    response = client.run_project_python_file(LOCOMOTION_PROBE, list(argv))
    return dict(parse_stdout_json(str(response.get("stdout", ""))))


def capture_live_window(client: UnrealMCPClient, name: str) -> tuple[Path, tuple[int, int]]:
    deadline = time.monotonic() + 15.0
    snapshot = ""
    preview_ref = ""
    while time.monotonic() < deadline:
        snapshot = str(
            client.call_tool(
                "Snapshot",
                {"ref": "", "maxDepth": 3, "bIncludeSourceLocations": False},
                toolset_name=SLATE_TOOLSET,
                timeout=client.timeout,
            )
        )
        preview_ref = _slate_preview_window_ref(snapshot)
        if preview_ref:
            break
        time.sleep(0.10)

    if not preview_ref:
        match = re.search(r'window "GameXXK - Unreal Editor"[^\n]*\[ref=(w\d+)\]', snapshot)
        if not match:
            raise RuntimeError(
                "GameXXK Preview or editor Slate window was not found; "
                f"last snapshot prefix: {snapshot[:500]}"
            )
        preview_ref = match.group(1)

    payload = client.call_tool(
        "Screenshot",
        {"ref": preview_ref},
        toolset_name=SLATE_TOOLSET,
        timeout=client.timeout,
    )
    data = _decode_slate_screenshot_png(payload)
    size = _png_size(data)
    if size[0] <= 0 or size[1] <= 0:
        raise RuntimeError(f"Slate screenshot returned invalid size: {size}")
    path = PROJECT_ROOT / "Saved/Codex" / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return path, size


def validate(report: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    states = report["states"]
    expected = {
        "idle_before": (EXPECTED_IDLE, False, True, 1),
        "walk_start_right": (EXPECTED_START, True, False, -1),
        "walk_loop_right": (EXPECTED_LOOP, True, True, -1),
		"walk_stop_right": (EXPECTED_STOP, False, False, -1),
        "idle_after": (EXPECTED_IDLE, False, True, -1),
		"deep_breath": (EXPECTED_BREATH, False, False, -1),
		"idle_after_deep_breath": (EXPECTED_IDLE, False, True, -1),
		"adjust_backpack": (EXPECTED_BACKPACK, False, False, -1),
		"idle_after_adjust_backpack": (EXPECTED_IDLE, False, True, -1),
    }
    for name, (path, moving, looping, scale_sign) in expected.items():
        state = states.get(name, {})
        if state.get("ok") is not True:
            errors.append(f"{name}:sample_failed")
            continue
        if state.get("flipbook") != path:
            errors.append(f"{name}:wrong_flipbook:{state.get('flipbook')}")
        if state.get("moving") is not moving:
            errors.append(f"{name}:wrong_moving:{state.get('moving')}")
        if state.get("looping") is not looping:
            errors.append(f"{name}:wrong_looping:{state.get('looping')}")
        scale = state.get("scale") or []
        if not scale or float(scale[0]) * scale_sign <= 0.0:
            errors.append(f"{name}:wrong_horizontal_scale:{scale}")
    if states.get("idle_before", {}).get("pawnClass") != "BP_HeroCharacter_C":
        errors.append(
            f"wrong_playable_pawn:{states.get('idle_before', {}).get('pawnClass')}"
        )
    return errors


def main() -> int:
    client = UnrealMCPClient(timeout=180.0)
    if not client.connect():
        raise RuntimeError(f"cannot connect to UE MCP at {client.endpoint}")
    if client.is_in_pie():
        client.stop_pie()
        if not client.wait_for_pie_state(False, timeout=45.0):
            raise RuntimeError("could not stop existing PIE before town hero probe")
    load_response = client.run_project_python_file(TOWN_LOADER, ["--load"])
    load_result = parse_stdout_json(str(load_response.get("stdout", "")))
    if load_result.get("step") != "loaded":
        raise RuntimeError(f"town map load failed: {load_result}")

    client.start_pie(warmup_seconds=1.5)
    if not client.wait_for_pie_state(True, timeout=60.0):
        raise RuntimeError("PIE did not start for town hero probe")

    states: dict[str, dict[str, Any]] = {}
    screenshots: dict[str, str] = {}
    try:
        states["idle_before"] = run_probe(client)
        idle_path, _ = capture_live_window(client, "town_hero_horizontal_idle.png")
        screenshots["idle_before"] = str(idle_path)

        key_down = run_probe(client, "--key", "D", "down")
        states["walk_start_right"] = dict(key_down.get("sample") or {})
        start_path, _ = capture_live_window(client, "town_hero_horizontal_walk_start.png")
        screenshots["walk_start_right"] = str(start_path)

        time.sleep(1.0)
        states["walk_loop_right"] = run_probe(client)
        loop_path, _ = capture_live_window(client, "town_hero_horizontal_walk_loop.png")
        screenshots["walk_loop_right"] = str(loop_path)
    finally:
        key_up = run_probe(client, "--key", "D", "up")
        states["walk_stop_right"] = dict(key_up.get("sample") or {})

    idle_after = {}
    for _ in range(30):
        time.sleep(0.1)
        idle_after = run_probe(client)
        if idle_after.get("flipbook") == EXPECTED_IDLE:
            break
    states["idle_after"] = idle_after
    idle_after_path, _ = capture_live_window(client, "town_hero_horizontal_idle_after.png")
    screenshots["idle_after"] = str(idle_after_path)

    deep_breath = run_probe(client, "--action", "DEEP_BREATH")
    states["deep_breath"] = dict(deep_breath.get("sample") or {})
    deep_breath_path, _ = capture_live_window(client, "town_hero_horizontal_deep_breath.png")
    screenshots["deep_breath"] = str(deep_breath_path)
    time.sleep(5.3)
    states["idle_after_deep_breath"] = run_probe(client)

    adjust_backpack = run_probe(client, "--action", "ADJUST_BACKPACK")
    states["adjust_backpack"] = dict(adjust_backpack.get("sample") or {})
    adjust_backpack_path, _ = capture_live_window(client, "town_hero_horizontal_adjust_backpack.png")
    screenshots["adjust_backpack"] = str(adjust_backpack_path)
    time.sleep(5.3)
    states["idle_after_adjust_backpack"] = run_probe(client)
    report: dict[str, Any] = {
        "ok": False,
        "map": "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo",
        "states": states,
        "screenshots": screenshots,
        "pieLeftRunningForReview": True,
    }
    report["errors"] = validate(report)
    report["ok"] = not report["errors"]
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
