"""Re-capture the reward settlement screen cleanly (PIE).

The previous reward screenshot was captured mid-transition (reward overlay not
settled). This run captures two states:
  1. Victory settled (battle finished screen, nothing clicked) ->
     Saved/Codex/battle_victory_pie.png
  2. ResolveBattleVictory -> wait for presentation idle + extra settle ->
     pending_reward verified, NO reward selected -> battle_reward_pie.png
"""
from __future__ import annotations

import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_hp_hud_updates import HpHudTester, HpHudTestError
from gamexxk_real_play_flow_mcp import PreviewWindowController, PROJECT_ROOT
from battle_target_reward_capture import (
    _restore_preview_window,
    _ensure_battle,
    _wait_player_phase,
    _resolve_pending_choice,
    _capture,
)

VICTORY_OUT = PROJECT_ROOT / "Saved" / "Codex" / "battle_victory_pie.png"
REWARD_OUT = PROJECT_ROOT / "Saved" / "Codex" / "battle_reward_pie.png"
ROUTE_PROBE = "Content/Python/gamexxk_probe_real_play_flow.py"
MAX_GRIND_ROUNDS = 15


def _phase(t: HpHudTester) -> str:
    return str(t._state().get("phase", "")).upper()


def _wait_player_phase_terminal(t: HpHudTester) -> str:
    """Wait for the player phase, but return immediately if the battle reaches
    a terminal phase (VICTORY can arrive during the enemy phase via damage-
    over-time effects)."""
    if "ENEMY" in _phase(t):
        t._wait_presentation_idle("enemy presentation idle")
    deadline = time.monotonic() + 90.0
    while time.monotonic() < deadline:
        phase = _phase(t)
        if "PLAYER" in phase:
            return phase
        if "VICTORY" in phase or "DEFEAT" in phase:
            return phase
        time.sleep(2.0)
    raise HpHudTestError("timed out waiting for player phase")


def _grind_to_victory(t: HpHudTester) -> None:
    for round_index in range(MAX_GRIND_ROUNDS):
        phase = _phase(t)
        if "VICTORY" in phase:
            return
        if "DEFEAT" in phase:
            raise HpHudTestError("battle lost before reward capture")
        phase = _wait_player_phase_terminal(t)
        if "VICTORY" in phase:
            return
        if "DEFEAT" in phase:
            raise HpHudTestError("battle lost before reward capture")
        for card in t._hand_ids_full():
            result = t._play_card(card["instance_id"])
            if result.get("ok"):
                time.sleep(0.15)
                t._wait_presentation_idle(f"card {card.get('card_id')} idle")
            _resolve_pending_choice(t)
        _resolve_pending_choice(t)
        ended = t._board_action("end_phase")
        t.event("end_phase", ok=bool(ended.get("ok")), round=round_index + 1)
        t._wait_presentation_idle(f"round {round_index + 1} enemy phase idle")
    raise HpHudTestError(f"battle did not reach Victory within {MAX_GRIND_ROUNDS} rounds")


def main() -> int:
    t = HpHudTester(timeout=45.0, keep_pie=True)
    try:
        t._ensure_editor()
        _ensure_battle(t)
        pc = PreviewWindowController()
        _restore_preview_window(pc)
        t._wait_presentation_idle("battle start settle")
        _resolve_pending_choice(t)
        t.event("battle_ready", screen=str(t._state().get("screen", "")), phase=_phase(t))

        _grind_to_victory(t)
        t.event("victory_reached")
        # 1. battle-finished screen: let the victory presentation fully settle.
        t._wait_presentation_idle("victory settle")
        time.sleep(2.0)
        victory_size = _capture(pc, VICTORY_OUT)
        t.event("victory_screen_captured", size=victory_size)

        # 2. reward overlay: resolve, then wait for presentation idle + settle.
        resolved = t._probe(ROUTE_PROBE, ["--hud-command", "ResolveBattleVictory"])
        hud_result = resolved.get("hud_command", {})
        t.event("resolve_victory_command",
                **{k: hud_result.get(k) for k in ("ok", "reason", "command") if k in hud_result})
        if not hud_result.get("ok"):
            raise HpHudTestError(f"ResolveBattleVictory failed: {hud_result}")
        t._wait_presentation_idle("reward overlay settle")
        time.sleep(3.0)
        check = t._board_action("pending_reward")
        if not check.get("ok") or not check.get("card_ids"):
            raise HpHudTestError(f"pending reward missing after victory: {check}")
        t.event("pending_reward_ready", card_ids=check.get("card_ids"))
        # No reward is selected: capture the resting default state.
        reward_size = _capture(pc, REWARD_OUT)
        t.event("reward_captured", size=reward_size, card_ids=check.get("card_ids"))
        print(json.dumps(
            {
                "ok": True,
                "victory_path": str(VICTORY_OUT),
                "victory_size": victory_size,
                "reward_path": str(REWARD_OUT),
                "reward_size": reward_size,
                "card_ids": check.get("card_ids"),
                "events": t.events,
            },
            ensure_ascii=False,
            indent=2,
        ))
        return 0
    except HpHudTestError as exc:
        print(json.dumps({"ok": False, "error": str(exc), "events": t.events}, ensure_ascii=False, indent=2))
        return 1


if __name__ == "__main__":
    sys.exit(main())
