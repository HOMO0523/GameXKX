#!/usr/bin/env python3
"""Reproduce the battle reward screen in PIE via BlueprintCallable probes (no OS input)."""

from __future__ import annotations

import json
import sys
import time

from gamexxk_content_assembly_check import parse_stdout_json
from ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, DEFAULT_PORT, UnrealMCPClient

PROBE = "Content/Python/_probe_battle_action.py"


def main() -> int:
    client = UnrealMCPClient(timeout=180.0)
    if not client.connect():
        print(json.dumps({"ok": False, "error": "mcp_connect_failed"}, ensure_ascii=False))
        return 1

    def act(*argv):
        r = client.run_project_python_file(PROBE, argv=list(argv))
        return parse_stdout_json(str(r.get("stdout", "")))

    if not client.is_in_pie():
        client.start_pie(warmup_seconds=2.0)
        for _ in range(60):
            if client.is_in_pie():
                break
            time.sleep(0.5)
        time.sleep(2.0)

    out: dict = {}
    steps = [
        ("start_game", ("start_game",)),
        ("accept_quest", ("accept_quest",)),
        ("enter_route", ("enter_route",)),
        ("select_route0", ("select_route", "0")),
        ("select_battle", ("select_battle",)),
        ("battle_state", ("battle_state",)),
    ]
    for name, argv in steps:
        out[name] = act(*argv)
        print(json.dumps({name: out[name]}, ensure_ascii=False), flush=True)

    # Fallback: pick an enabled battle node from the route map and enter it.
    battle_state = out.get("battle_state", {})
    if not battle_state.get("b_has_active_card_battle"):
        out["route_nodes"] = act("route_nodes")
        print(json.dumps({"route_nodes": out["route_nodes"]}, ensure_ascii=False), flush=True)
        nodes = out["route_nodes"].get("nodes", [])
        battle_node = next(
            (n for n in nodes if n.get("enabled") and n.get("node_kind") == "GameXXKNodeKind::BATTLE"),
            None,
        )
        if battle_node:
            out["select_route_battle"] = act("select_route", str(battle_node["node_id"]))
            print(json.dumps({"select_route_battle": out["select_route_battle"]}, ensure_ascii=False), flush=True)
            out["select_battle_retry"] = act("select_battle")
            print(json.dumps({"select_battle_retry": out["select_battle_retry"]}, ensure_ascii=False), flush=True)
            out["battle_state"] = act("battle_state")
            print(json.dumps({"battle_state": out["battle_state"]}, ensure_ascii=False), flush=True)

    out["resolve_victory"] = act("resolve_victory")
    print(json.dumps({"resolve_victory": out["resolve_victory"]}, ensure_ascii=False), flush=True)
    time.sleep(0.5)
    out["refresh_board"] = act("refresh_board")
    print(json.dumps({"refresh_board": out["refresh_board"]}, ensure_ascii=False), flush=True)
    time.sleep(0.5)
    out["battle_state2"] = act("battle_state")
    print(json.dumps({"battle_state2": out["battle_state2"]}, ensure_ascii=False), flush=True)

    print(json.dumps({"summary": out}, ensure_ascii=False))
    ok = out.get("battle_state2", {}).get("pending_reward_card_ids")
    return 0 if ok else 2


if __name__ == "__main__":
    sys.exit(main())
