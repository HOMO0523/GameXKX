"""Capture the live in-game battle board (PIE) for visual review of battle pages.

Reuses HpHudTester's proven main-menu -> town -> battle flow, then captures the
PIE preview window via Win32 (no Slate dependency, always includes UMG).
"""
from __future__ import annotations

import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_hp_hud_updates import HpHudTester
from gamexxk_real_play_flow_mcp import PreviewWindowController, PROJECT_ROOT
from ue_mcp_client import EDITOR_TOOLSET


def main() -> int:
    t = HpHudTester(timeout=30.0, keep_pie=True)
    t._ensure_editor()
    client = t.client

    # Start PIE if not already in a play session (main menu / town / battle screen).
    state = t._state()
    screen = str(state.get("screen", "")).upper()
    if not screen:
        client.call_tool(
            "StartPIE",
            {"options": {"bSimulate": False, "playMode": 1, "warmupSeconds": 1.0}},
            toolset_name=EDITOR_TOOLSET,
            timeout=60.0,
        )
        t._wait_for("PIE main menu", lambda: "MAIN_MENU" in str(t._state().get("screen", "")).upper(), timeout=30.0)
    if "MAIN_MENU" in str(t._state().get("screen", "")).upper():
        # Current version: start_game drops straight into the town (not WORLD_MAP).
        deadline = time.monotonic() + 30.0
        while time.monotonic() < deadline:
            t._board_action("start_game")
            scr = str(t._state().get("screen", "")).upper()
            if "TOWN" in scr or "DUNGEON_MAP" in scr or "BATTLE" in scr:
                break
            time.sleep(1.0)
        t._wait_for("post-start town", lambda: ("TOWN" in str(t._state().get("screen", "")).upper())
                    or ("DUNGEON_MAP" in str(t._state().get("screen", "")).upper()), timeout=30.0)

    # Direct subsystem mutation never refreshes the widget tree (no broadcast,
    # PC does not poll). The proven path is the widget-level chain via
    # gamexxk_probe_real_play_flow.py --route-node N (ExecuteRouteNodeById ->
    # CommandRouter -> NotifyPlayerFlowStateChanged -> EnterBattleOverlay).
    # A stale BATTLE screen with a collapsed board (left by direct mutation)
    # blocks route-node (requires Screen==DungeonMap), so restart PIE clean.
    ROUTE_PROBE = "Content/Python/gamexxk_probe_real_play_flow.py"
    if "BATTLE" in str(t._state().get("screen", "")).upper():
        t.event("stale_battle_detected_restarting_pie")
        client.stop_pie()
        if not client.wait_for_pie_state(False, timeout=30.0):
            raise RuntimeError("PIE did not stop for clean battle capture")
        client.call_tool(
            "StartPIE",
            {"options": {"bSimulate": False, "playMode": 1, "warmupSeconds": 1.0}},
            toolset_name=EDITOR_TOOLSET,
            timeout=60.0,
        )
        t._wait_for("fresh main menu", lambda: "MAIN_MENU" in str(t._state().get("screen", "")).upper(), timeout=30.0)
        deadline = time.monotonic() + 30.0
        while time.monotonic() < deadline:
            t._board_action("start_game")
            scr = str(t._state().get("screen", "")).upper()
            if "TOWN" in scr:
                break
            time.sleep(1.0)
    if "TOWN" in str(t._state().get("screen", "")).upper():
        t._board_action("accept_quest")
        t._probe(ROUTE_PROBE, ["--town-command", "EnterDungeon"])
        t._wait_for("route map", lambda: "DUNGEON_MAP" in str(t._state().get("screen", "")).upper(), timeout=30.0)
    n0 = t._probe(ROUTE_PROBE, ["--route-node", "0"])
    t.event("route_node_0", **{k: n0.get(k) for k in ("ok", "reason", "node_index") if k in n0})
    n1 = t._probe(ROUTE_PROBE, ["--route-node", "1"])
    t.event("route_node_1", **{k: n1.get(k) for k in ("ok", "reason", "node_index") if k in n1})
    # Hard gate: the capture must be the real battle board, not the route map.
    t._wait_for(
        "real battle screen",
        lambda: "BATTLE" in str(t._state().get("screen", "")).upper(),
        timeout=45.0,
    )
    battle_state = t._state()
    t.event("battle_reached", screen=str(battle_state.get("screen", "")))
    time.sleep(2.0)  # let the board settle

    # Capture the PIE preview window (Win32) — includes the UMG battle HUD.
    pc = PreviewWindowController()
    win = pc.find_preview_window()
    data, size = pc.capture_window_png(win)
    out = PROJECT_ROOT / "Saved" / "Codex" / "battle_open_pie.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    print(json.dumps({"ok": True, "path": str(out), "size": list(size)}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
