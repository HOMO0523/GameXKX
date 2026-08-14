#!/usr/bin/env python3
"""Enter an Elite battle in a fresh PIE and leave it active for the grind driver."""

from __future__ import annotations

import json
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

    if client.is_in_pie():
        client.stop_pie()
        time.sleep(3.0)
    client.start_pie(warmup_seconds=2.0)
    for _ in range(60):
        if client.is_in_pie():
            break
        time.sleep(0.5)
    time.sleep(2.0)

    for attempt in range(25):
        act("start_game")
        act("accept_quest")
        act("enter_route")
        act("select_route", "0")
        nodes = act("route_nodes").get("nodes", [])
        elite = next(
            (n for n in nodes if n.get("enabled") and "ELITE" in str(n.get("node_kind", "")).upper()),
            None,
        )
        if not elite:
            print(json.dumps({"attempt": attempt + 1, "elite": False}, ensure_ascii=False), flush=True)
            continue
        print(json.dumps({"attempt": attempt + 1, "elite_node": elite}, ensure_ascii=False), flush=True)
        act("select_route", str(elite["node_id"]))
        state = act("battle_state")
        if state.get("b_has_active_card_battle"):
            print(json.dumps({"ok": True, "battle": state}, ensure_ascii=False))
            return 0
    print(json.dumps({"ok": False, "error": "no_elite_node_found"}, ensure_ascii=False))
    return 2


if __name__ == "__main__":
    sys.exit(main())
