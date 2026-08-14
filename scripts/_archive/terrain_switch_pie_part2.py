#!/usr/bin/env python3
"""Part 2: play the Formation Master terrain-switch card in the open battle."""

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
    hand = deck.get("deck", {}).get("hand", [])
    formation_hand = [c for c in hand if "FormationMaster" in c.get("card_id", "")]
    print(json.dumps({"step": "formation_hand", "cards": formation_hand}, ensure_ascii=False), flush=True)

    played = None
    for card in formation_hand:
        short = card["card_id"].rsplit(".", 1)[-1]
        if short in FORMATION_TERRAIN_CARDS:
            played = dict(card)
            played["destination_terrain"] = FORMATION_TERRAIN_CARDS[short]
            break
    if not played:
        print(json.dumps({"step": "done", "ok": False, "reason": "no_terrain_switch_card_in_hand"}), flush=True)
        return 1

    clicked = tester._board_action("click_card", played["instance_id"])
    print(json.dumps({"step": "click_switch_card", "card": played, **clicked}, ensure_ascii=False), flush=True)
    time.sleep(0.6)
    targeting = tester._board_action("targeting_active")
    if targeting.get("ok") and targeting.get("targeting_active"):
        tester._board_action("confirm", "Player")
    for _ in range(12):
        time.sleep(0.5)
        idle = tester._board_action("presentation_idle")
        if idle.get("ok") and idle.get("idle"):
            break
    refreshed = tester._probe(BOARD_REFRESH, [])
    print(json.dumps({"step": "board_refresh_after", **refreshed}, ensure_ascii=False), flush=True)
    time.sleep(1.0)

    after_terrain = tester._probe(TERRAIN_PROBE, ["terrain"])
    print(json.dumps({"step": "after_terrain", **after_terrain}, ensure_ascii=False), flush=True)

    harness = RealFlowHarness(timeout=30.0, keep_pie=True)
    harness.client = tester.client
    harness.preview_window = harness.input.find_preview_window()
    path, size = harness.screenshot("battle_terrain_after_switch_slate.png")
    print(json.dumps({"step": "after_shot", "path": str(path), "size": list(size)}, ensure_ascii=False), flush=True)

    expected = f"T_BattleArena_{played['destination_terrain']}_GeneratedV2"
    ok = after_terrain.get("terrain_name", "").upper() == played["destination_terrain"].upper()
    print(json.dumps({"step": "done", "ok": ok, "expected_texture": expected}, ensure_ascii=False))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
