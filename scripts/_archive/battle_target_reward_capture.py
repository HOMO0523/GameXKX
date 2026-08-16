"""Capture the two remaining battle states luna needs (PIE) for specs 17/11.

  1. 17 选中目标: click a targeting card in hand -> capture while
     is_card_targeting_active() is true (card raised + target marks + outcome
     preview) -> cancel targeting.
  2. 11 奖励结算: play the real battle to Victory -> send the visible
     ResolveBattleVictory HUD command -> PendingReward.CardIds == 3 ->
     capture the reward overlay on the battle board.

Reuses the live PIE battle if one is present (real HUD entries); otherwise
re-enters through the proven widget chain: PIE -> main menu -> start_game ->
town -> accept_quest -> EnterDungeon -> route nodes 0,1 -> battle board.
"""
from __future__ import annotations

import ctypes
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from test_hp_hud_updates import HpHudTester, HpHudTestError
from gamexxk_real_play_flow_mcp import PreviewWindowController, PROJECT_ROOT
from ue_mcp_client import EDITOR_TOOLSET

TARGET_OUT = PROJECT_ROOT / "Saved" / "Codex" / "battle_target_select_pie.png"
REWARD_OUT = PROJECT_ROOT / "Saved" / "Codex" / "battle_reward_pie.png"
ROUTE_PROBE = "Content/Python/gamexxk_probe_real_play_flow.py"
MAX_TARGET_SCAN_ROUNDS = 6
MAX_GRIND_ROUNDS = 15


def _restore_preview_window(pc: PreviewWindowController) -> dict:
    """Un-minimize the PIE preview window (minimized windows return icon rects
    and throttle PIE ticking, stalling presentations and breaking captures)."""
    win = pc.find_preview_window()
    left, top, right, bottom = [int(v) for v in win["rect"]]
    if right - left < 400 or bottom - top < 300:
        hwnd = ctypes.c_void_p(int(win["hwnd"]))
        pc.user32.ShowWindow(hwnd, 9)  # SW_RESTORE
        time.sleep(0.6)
        win = pc.find_preview_window()
        left, top, right, bottom = [int(v) for v in win["rect"]]
    win["rect"] = [left, top, right, bottom]
    return win


def _capture(pc: PreviewWindowController, out: Path) -> list:
    win = _restore_preview_window(pc)
    left, top, right, bottom = [int(v) for v in win["rect"]]
    if right - left < 400 or bottom - top < 300:
        raise RuntimeError(f"preview window still minimized: rect={win['rect']}")
    data, size = pc.capture_window_png(win)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    return list(size)


def _phase(t: HpHudTester) -> str:
    return str(t._state().get("phase", "")).upper()


def _ensure_battle(t: HpHudTester) -> None:
    """Reuse a live real battle; otherwise restart PIE and walk the widget chain."""
    state = t._state()
    screen = str(state.get("screen", "")).upper()
    if "BATTLE" in screen and state.get("runtime_units"):
        t.event("reuse_live_battle", units=len(state["runtime_units"]))
        return

    if "BATTLE" in screen:
        t.event("stale_battle_detected_restarting_pie")
        t.client.stop_pie()
        if not t.client.wait_for_pie_state(False, timeout=30.0):
            raise HpHudTestError("PIE did not stop for clean battle entry")
    if not str(t._state().get("screen", "")).strip():
        t.client.call_tool(
            "StartPIE",
            {"options": {"bSimulate": False, "playMode": 1, "warmupSeconds": 1.0}},
            toolset_name=EDITOR_TOOLSET,
            timeout=60.0,
        )
        t._wait_for("PIE main menu", lambda: "MAIN_MENU" in str(t._state().get("screen", "")).upper(), timeout=30.0)
    if "MAIN_MENU" in str(t._state().get("screen", "")).upper():
        deadline = time.monotonic() + 30.0
        while time.monotonic() < deadline:
            t._board_action("start_game")
            scr = str(t._state().get("screen", "")).upper()
            if "TOWN" in scr or "DUNGEON_MAP" in scr or "BATTLE" in scr:
                break
            time.sleep(1.0)
        t._wait_for(
            "post-start town",
            lambda: ("TOWN" in str(t._state().get("screen", "")).upper())
            or ("DUNGEON_MAP" in str(t._state().get("screen", "")).upper()),
            timeout=30.0,
        )
    if "TOWN" in str(t._state().get("screen", "")).upper():
        t._board_action("accept_quest")
        t._probe(ROUTE_PROBE, ["--town-command", "EnterDungeon"])
        t._wait_for("route map", lambda: "DUNGEON_MAP" in str(t._state().get("screen", "")).upper(), timeout=30.0)
    t._probe(ROUTE_PROBE, ["--route-node", "0"])
    t._probe(ROUTE_PROBE, ["--route-node", "1"])
    t._wait_for(
        "real battle screen",
        lambda: "BATTLE" in str(t._state().get("screen", "")).upper(),
        timeout=45.0,
    )
    time.sleep(2.0)


def _wait_player_phase(t: HpHudTester) -> None:
    if "ENEMY" in _phase(t):
        t._wait_presentation_idle("enemy presentation idle")
    t._wait_for(
        "player phase",
        lambda: "PLAYER" in _phase(t),
        timeout=60.0,
    )


def _resolve_pending_choice(t: HpHudTester) -> None:
    """Drain Deck.PendingChoice (forced discard / insight / task search) — it
    blocks ClickCardInHand and EndCardPlayerPhase until confirmed or cancelled."""
    for _ in range(4):
        check = t._board_action("pending_choice")
        if not check.get("ok"):
            return
        kind = str(check.get("kind", ""))
        candidates = check.get("candidate_ids", []) or []
        if not candidates:
            raise HpHudTestError(f"pending choice {kind} has no candidates: {check}")
        kind_upper = kind.upper()
        if "DISCARD" in kind_upper:
            action = "submit_discard"
        elif "TASK_SEARCH" in kind_upper:
            action = "submit_task_search"
        elif "INSIGHT" in kind_upper:
            action = "submit_insight"
        else:
            raise HpHudTestError(f"unknown pending choice kind: {kind}")
        submitted = t._board_action(action, candidates[0])
        t.event("pending_choice_submitted", kind=kind, candidate=candidates[0],
                ok=bool(submitted.get("ok")))
        if not submitted.get("ok"):
            if "INSIGHT" in kind_upper:  # insight is cancellable, forced discard is not
                t._board_action("cancel_insight")
                t.event("pending_choice_cancelled", kind=kind)
            else:
                raise HpHudTestError(f"pending choice submission failed: {submitted}")
        t._wait_presentation_idle(f"pending choice {kind} settle")
    raise HpHudTestError("pending choice did not drain")


def _capture_target_selection(t: HpHudTester, pc: PreviewWindowController) -> bool:
    """Scan the hand for a targeting card; capture the pending-target state."""
    for round_index in range(MAX_TARGET_SCAN_ROUNDS):
        phase = _phase(t)
        if "VICTORY" in phase:
            t.event("victory_before_target_capture")
            return False
        if "DEFEAT" in phase:
            raise HpHudTestError("battle lost before target-selection capture")
        _wait_player_phase(t)
        for card in t._hand_ids_full():
            clicked = t._board_action("click_card", card["instance_id"])
            if not clicked.get("ok"):
                continue
            targeting = t._board_action("targeting_active")
            if targeting.get("ok"):
                highlights = t._board_action("target_highlights")
                highlighted = highlights.get("highlighted_unit_ids", []) or []
                enemies = [
                    u["unit_id"]
                    for u in t._state().get("runtime_units", [])
                    if "ENEMY" in str(u.get("side", "")).upper()
                ]
                if not any(uid in enemies for uid in highlighted):
                    # Party-targeting card (defend/heal): not the canonical
                    # target-selection state luna needs. Skip, keep scanning.
                    t._board_action("cancel_targeting")
                    t.event("target_scan_skipped_party_target",
                            card_id=card.get("card_id"), highlighted=highlighted)
                    continue
                time.sleep(0.9)  # let raised card / target marks / outcome preview render
                size = _capture(pc, TARGET_OUT)
                t.event("target_selection_captured", card_id=card.get("card_id"),
                        size=size, highlighted=highlighted)
                t._board_action("cancel_targeting")
                return True
            # Auto-played (no targeting): settle its presentation, keep scanning.
            t.event("card_auto_played_scan", card_id=card.get("card_id"))
            t._wait_presentation_idle(f"scan auto card {card.get('card_id')} idle")
            _resolve_pending_choice(t)
        _resolve_pending_choice(t)
        t._board_action("end_phase")
        t.event("target_scan_round", round=round_index + 1)
        t._wait_presentation_idle("scan enemy phase idle")
    return False


def _capture_reward_settlement(t: HpHudTester, pc: PreviewWindowController) -> bool:
    """Grind the real battle to Victory, resolve it, capture the reward overlay."""
    for round_index in range(MAX_GRIND_ROUNDS):
        phase = _phase(t)
        if "VICTORY" in phase:
            break
        if "DEFEAT" in phase:
            raise HpHudTestError("battle lost before reward capture")
        _wait_player_phase(t)
        for card in t._hand_ids_full():
            result = t._play_card(card["instance_id"])
            if result.get("ok"):
                time.sleep(0.15)
                t._wait_presentation_idle(f"card {card.get('card_id')} idle")
            _resolve_pending_choice(t)
        _resolve_pending_choice(t)
        # Always pass the turn (an empty hand must not deadlock the round).
        ended = t._board_action("end_phase")
        t.event("end_phase", ok=bool(ended.get("ok")), round=round_index + 1)
        t._wait_presentation_idle(f"round {round_index + 1} enemy phase idle")
    else:
        raise HpHudTestError(f"battle did not reach Victory within {MAX_GRIND_ROUNDS} rounds")

    t.event("victory_reached")
    # Player normally clicks a victory button; the visible-command router drives
    # ResolveBattleVictory -> PendingReward.CardIds (3) -> reward overlay on the
    # same battle screen (shell test asserts Screen==Battle && CardIds==3).
    resolved = t._probe(ROUTE_PROBE, ["--hud-command", "ResolveBattleVictory"])
    hud_result = resolved.get("hud_command", {})
    t.event("resolve_victory_command", **{k: hud_result.get(k) for k in ("ok", "reason", "command") if k in hud_result})
    if not hud_result.get("ok"):
        raise HpHudTestError(f"ResolveBattleVictory failed: {hud_result}")
    time.sleep(1.5)
    check = t._board_action("pending_reward")
    if not check.get("ok") or not check.get("card_ids"):
        raise HpHudTestError(f"pending reward missing after victory: {check}")
    t.event("pending_reward_ready", card_ids=check.get("card_ids"))
    time.sleep(0.6)
    size = _capture(pc, REWARD_OUT)
    t.event("reward_captured", size=size, card_ids=check.get("card_ids"))
    return True


def main() -> int:
    t = HpHudTester(timeout=45.0, keep_pie=True)
    try:
        t._ensure_editor()
        _ensure_battle(t)
        pc = PreviewWindowController()
        t.event("battle_ready", screen=str(t._state().get("screen", "")), phase=_phase(t))
        _restore_preview_window(pc)  # keep PIE ticking at full rate for presentations
        t._wait_presentation_idle("battle start settle")
        _resolve_pending_choice(t)  # e.g. a GuanXi discard left pending mid-battle
        target_ok = _capture_target_selection(t, pc)
        reward_ok = _capture_reward_settlement(t, pc)
        print(json.dumps(
            {
                "ok": True,
                "target_captured": target_ok,
                "target_path": str(TARGET_OUT) if target_ok else "",
                "reward_captured": reward_ok,
                "reward_path": str(REWARD_OUT) if reward_ok else "",
                "events": t.events,
            },
            ensure_ascii=False,
            indent=2,
        ))
        return 0 if target_ok and reward_ok else 2
    except HpHudTestError as exc:
        print(json.dumps({"ok": False, "error": str(exc), "events": t.events}, ensure_ascii=False, indent=2))
        return 1


if __name__ == "__main__":
    sys.exit(main())
