#!/usr/bin/env python3
"""Reuse the dedicated battle-entry flow, then capture terrain + Slate screenshot."""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_hp_hud_updates import HpHudTester

TERRAIN_PROBE = "Content/Python/_probe_terrain_backdrop_pie.py"


def main() -> int:
    tester = HpHudTester(timeout=30.0, keep_pie=True)
    tester._ensure_editor()
    state = tester._ensure_battle()
    print(json.dumps({"step": "in_battle", "screen": state.get("screen")}, ensure_ascii=False), flush=True)

    terrain = tester._probe(TERRAIN_PROBE, ["terrain"])
    print(json.dumps({"step": "terrain", **terrain}, ensure_ascii=False), flush=True)

    # Slate window capture of the battle screen (UMG included).
    try:
        from gamexxk_real_play_flow_mcp import RealFlowHarness
        harness = RealFlowHarness(timeout=30.0, keep_pie=True)
        harness.client = tester.client
        harness.preview_window = harness.input.find_preview_window()
        path, size = harness.screenshot("battle_terrain_check_slate.png")
        print(json.dumps({"step": "screenshot", "path": path, "size": size}, ensure_ascii=False), flush=True)
    except Exception as exc:
        print(json.dumps({"step": "screenshot", "error": str(exc)[:300]}, ensure_ascii=False), flush=True)

    print(json.dumps({"step": "done", "ok": True}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
