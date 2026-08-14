#!/usr/bin/env python3
"""Grind the current PIE battle to victory via BlueprintCallable probes, then
apply ResolveBattleVictory and refresh the board so the reward row is visible."""

from __future__ import annotations

import json
import sys
import time

from gamexxk_content_assembly_check import parse_stdout_json
from ue_mcp_client import UnrealMCPClient

PROBE = "Content/Python/_probe_battle_action.py"
MAX_ROUNDS = 60


def main() -> int:
    client = UnrealMCPClient(timeout=180.0)
    if not client.connect():
        print(json.dumps({"ok": False, "error": "mcp_connect_failed"}, ensure_ascii=False))
        return 1

    def act(*argv):
        r = client.run_project_python_file(PROBE, argv=list(argv))
        return parse_stdout_json(str(r.get("stdout", "")))

    for round_index in range(MAX_ROUNDS):
        state = act("battle_state")
        phase = str(state.get("phase", ""))
        if "VICTORY" in phase.upper() or state.get("pending_reward_card_ids"):
            break
        if not state.get("b_has_active_card_battle"):
            print(json.dumps({"error": "no_active_battle"}, ensure_ascii=False))
            return 2

        # Play every playable hand card once per round, preferring enemy targets.
        enemy_ids = {
            u["unit_id"] for u in act("battle_state").get("units", [])
            if "ENEMY" in str(u.get("side", "")).upper() and u.get("b_living")
        }
        hand = act("hand_ids").get("hand_ids", [])
        for card in hand:
            click = act("click_card", card["instance_id"])
            if not click.get("ok"):
                continue
            if act("targeting_active").get("ok"):
                highlights = act("target_highlights").get("highlighted_unit_ids", [])
                enemy_targets = [h for h in highlights if h in enemy_ids]
                if enemy_targets:
                    act("confirm", enemy_targets[0])
                elif highlights:
                    act("confirm", highlights[0])
                else:
                    act("cancel_targeting")
            time.sleep(0.25)
        # Drain any blocking choice (forced discard / insight / task search).
        choice = act("pending_choice")
        if choice.get("ok"):
            kind = str(choice.get("kind", "")).upper()
            candidates = choice.get("candidate_ids", [])
            if candidates:
                if "DISCARD" in kind:
                    act("submit_discard", candidates[0])
                elif "INSIGHT" in kind:
                    act("submit_insight", candidates[0])
                elif "TASK" in kind or "SEARCH" in kind:
                    act("submit_task_search", candidates[0])
                time.sleep(0.25)
        end = act("end_phase")
        # The enemy phase resolves one intent presentation per board tick.  Give the
        # board wall-clock time and poll until the phase leaves ENEMY.
        for _ in range(40):
            state = act("battle_state")
            if "ENEMY" not in str(state.get("phase", "")).upper():
                break
            time.sleep(0.5)
        if end.get("error_text"):
            print(json.dumps({"round": round_index + 1, "end_phase": end}, ensure_ascii=False), flush=True)
        state = act("battle_state")
        print(json.dumps({"round": round_index + 1, "phase": state.get("phase"),
                          "hand": state.get("hand_count")}, ensure_ascii=False), flush=True)

    out = {}
    out["resolve_victory"] = act("resolve_victory")
    time.sleep(0.5)
    out["refresh_board"] = act("refresh_board")
    time.sleep(0.5)
    out["battle_state"] = act("battle_state")
    print(json.dumps(out, ensure_ascii=False))
    return 0 if out.get("resolve_victory", {}).get("ok") else 3


if __name__ == "__main__":
    sys.exit(main())
