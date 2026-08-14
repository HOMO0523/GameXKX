#!/usr/bin/env python3
"""Walk generated route layers until an Elite node is reachable, win it, and
leave the tiered reward pending for screenshot capture."""

from __future__ import annotations

import json
import subprocess
import sys
import time

from gamexxk_content_assembly_check import parse_stdout_json
from ue_mcp_client import UnrealMCPClient

PROBE = "Content/Python/_probe_battle_action.py"


def main() -> int:
    client = UnrealMCPClient(timeout=180.0)
    if not client.connect():
        print(json.dumps({"ok": False, "error": "mcp_connect_failed"}, ensure_ascii=False))
        return 1

    def act(*argv):
        r = client.run_project_python_file(PROBE, argv=list(argv))
        return parse_stdout_json(str(r.get("stdout", "")))

    def grind() -> bool:
        result = subprocess.run([sys.executable, "grind_to_reward.py"], capture_output=False)
        return result.returncode == 0

    def settle_reward() -> bool:
        # Accept the first option; the settlement advances the route node.
        act("refresh_board")
        ok = act("choose_reward_option", "0").get("ok") is True
        time.sleep(0.5)
        act("refresh_route")
        time.sleep(0.3)
        return ok

    # Reuse the current dungeon state if present; otherwise enter fresh.
    state = act("battle_state")
    if state.get("b_has_active_card_battle"):
        # Mid-battle (e.g. a pending reward from a previous run): settle it first.
        settle_reward()
        state = act("battle_state")
    if not state.get("b_has_active_card_battle"):
        if str(state.get("screen", "")).upper().endswith("DUNGEON_MAP"):
            pass
        else:
            act("start_game")
            act("accept_quest")
            act("enter_route")
        act("select_route", "0")

    for layer in range(5):
        nodes = act("route_nodes").get("nodes", [])
        elite = next(
            (n for n in nodes if n.get("enabled") and "ELITE" in str(n.get("node_kind", "")).upper()),
            None,
        )
        if elite:
            act("select_route", str(elite["node_id"]))
            st = act("battle_state")
            if st.get("b_has_active_card_battle"):
                print(json.dumps({"layer": layer, "elite_entered": elite}, ensure_ascii=False), flush=True)
                if grind():
                    print(json.dumps({"elite_victory": True}, ensure_ascii=False), flush=True)
                    return 0
                print(json.dumps({"elite_grind_failed": True}, ensure_ascii=False), flush=True)
                return 3
        battle = next(
            (n for n in nodes if n.get("enabled") and "BATTLE" in str(n.get("node_kind", "")).upper()),
            None,
        )
        if not battle:
            print(json.dumps({"layer": layer, "no_battle_or_elite": True}, ensure_ascii=False), flush=True)
            return 2
        act("select_route", str(battle["node_id"]))
        print(json.dumps({"layer": layer, "battle": battle.get("node_id")}, ensure_ascii=False), flush=True)
        if not grind():
            print(json.dumps({"battle_grind_failed": True}, ensure_ascii=False), flush=True)
            return 4
        if not settle_reward():
            print(json.dumps({"settle_failed": True}, ensure_ascii=False), flush=True)
            return 5
        time.sleep(1.0)

    print(json.dumps({"ok": False, "error": "elite_not_reached"}, ensure_ascii=False))
    return 6


if __name__ == "__main__":
    sys.exit(main())
