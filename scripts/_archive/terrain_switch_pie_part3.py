#!/usr/bin/env python3
"""Part 3: play 平野观势 (Plain switch) against a living enemy unit."""

from __future__ import annotations

import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_hp_hud_updates import HpHudTester
from gamexxk_real_play_flow_mcp import RealFlowHarness

BATTLE_PROBE = "Content/Python/_probe_battle_action.py"
TERRAIN_PROBE = "Content/Python/_probe_terrain_backdrop_pie.py"
DECK_PROBE = "Content/Python/_probe_battle_deck_dump.py"
BOARD_REFRESH = "Content/Python/_probe_board_refresh.py"

FORMATION_TERRAIN_CARDS = {
    "GuanShi": "Plain",
    "DingZhen": "Village",
    "YinShuiHuiYuan": "WaterShore",
    "KunZhen": "Cave",
    "LinYingMiZong": "Forest",
    "JieShanWeiZhang": "Cliff",
}


def main() -> int:
    tester = HpHudTester(timeout=30.0, keep_pie=True)
    tester._ensure_editor()

    deck = tester._probe(DECK_PROBE, [])
    enemies = [
        u.get("unit_id")
        for u in deck.get("units", [])
        if "ENEMY" in str(u.get("side", "")).upper() and u.get("living")
    ]
    hand = deck.get("deck", {}).get("hand", [])
    formation_hand = [c for c in hand if "FormationMaster" in c.get("card_id", "")]
    print(json.dumps({"step": "context", "enemies": enemies, "formation_hand": formation_hand}, ensure_ascii=False), flush=True)

    played = None
    for card in formation_hand:
        short = card["card_id"].rsplit(".", 1)[-1]
        if short in FORMATION_TERRAIN_CARDS:
            played = dict(card)
            played["destination_terrain"] = FORMATION_TERRAIN_CARDS[short]
            break
    if not played or not enemies:
        print(json.dumps({"step": "done", "ok": False, "reason": "missing card or enemy"}), flush=True)
        return 1

    clicked = tester._board_action("click_card", played["instance_id"])
    time.sleep(0.6)
    targeting = tester._board_action("targeting_active")
    if targeting.get("ok") and targeting.get("targeting_active"):
        confirmed = tester._board_action("confirm", enemies[0])
        print(json.dumps({"step": "confirm", **confirmed}, ensure_ascii=False), flush=True)
    for _ in range(12):
        time.sleep(0.5)
        idle = tester._board_action("presentation_idle")
        if idle.get("ok") and idle.get("idle"):
            break
    refreshed = tester._probe(BOARD_REFRESH, [])
    time.sleep(1.0)

    after_terrain = tester._probe(TERRAIN_PROBE, ["terrain"])
    print(json.dumps({"step": "after_terrain", **after_terrain}, ensure_ascii=False), flush=True)

    harness = RealFlowHarness(timeout=30.0, keep_pie=True)
    harness.client = tester.client
    harness.preview_window = harness.input.find_preview_window()
    path, size = harness.screenshot("battle_terrain_after_switch_slate.png")
    print(json.dumps({"step": "after_shot", "path": str(path), "size": list(size)}, ensure_ascii=False), flush=True)

    ok = after_terrain.get("terrain_name", "").upper() == played["destination_terrain"].upper()
    print(json.dumps({"step": "done", "ok": ok, "expected_texture": f"T_BattleArena_{played['destination_terrain']}_GeneratedV2"}, ensure_ascii=False))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
