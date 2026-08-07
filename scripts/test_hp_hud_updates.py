#!/usr/bin/env python3
"""Automated verification: GameXXK battle HP-number HUD stays in sync.

Covers the reported regression "HP text frozen at a stale value after special
damage / healing":

  1. Multi-board instance detection via [Board] alive counter and refresh logs.
  2. Play every playable hand card -> after presentation idle, every unit's
     projected HUD text must equal authoritative runtime HP.
  3. During an active presentation, NON-participant HUDs must equal runtime
     immediately (Fix 1 participant gating); participants may show the
     pre-mutation baseline.
  4. End player phase -> enemy attacks -> after their presentation, party HUDs
     must equal runtime.
  5. Every step also checks HP text consistency on the CURRENT board only.

Requires the editor to be running with the UE MCP server (the script launches
it if needed) and PIE gameplay.  The script drives battle actions through the
board's BlueprintCallable seams, so it never simulates OS input.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

PROJECT_ROOT = Path(__file__).resolve().parents[1]
PROJECT_FILE = PROJECT_ROOT / "GameXXK.uproject"
UE_EDITOR = Path(r"D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe")
MCP_PORT = 18765
MCP_URL = f"http://127.0.0.1:{MCP_PORT}/mcp"

STATE_PROBE = "Content/Python/gamexxk_probe_hp_hud_state.py"
MAIN_PROBE = "Content/Python/gamexxk_probe_real_play_flow.py"
SLATE_TOOLSET = "SlateInspectorToolset.SlateInspectorToolset"

SLATE_WINDOW_PATTERN = re.compile(r'window "([^"]*GameXXK Preview[^"]*)"[^\n]*\[ref=([^\]]+)\]')
SLATE_BUTTON_PATTERN = re.compile(
    r'button(?: "(?P<label>[^"]*)")?(?P<disabled> \[disabled\])? \[pos=(?P<x>-?\d+),(?P<y>-?\d+) size=(?P<w>\d+),(?P<h>\d+)\] \[ref=(?P<ref>[^\]]+)\]'
)

sys.path.insert(0, str(PROJECT_ROOT / "scripts"))
from ue_mcp_client import UnrealMCPClient  # noqa: E402


def _slate_preview_window_ref(snapshot: str) -> str:
    preview_ref = ""
    for match in SLATE_WINDOW_PATTERN.finditer(str(snapshot or "")):
        preview_ref = match.group(2)
    return preview_ref


class HpHudTestError(RuntimeError):
    pass


# Probe that executes ONE battle-board action through BlueprintCallable seams.
BATTLE_ACTION_PROBE = "Content/Python/_probe_battle_action.py"


class HpHudTester:
    def __init__(self, timeout: float = 30.0, keep_pie: bool = False) -> None:
        self.timeout = timeout
        self.keep_pie = keep_pie
        self.client: UnrealMCPClient | None = None
        self.events: list[dict[str, Any]] = []
        self.failures: list[str] = []
        self.board_alive: int | None = None

    # ---------- infrastructure ----------

    def event(self, name: str, **payload: Any) -> None:
        self.events.append({"event": name, **payload})

    def _ensure_editor(self) -> None:
        """Connect to the UE MCP server; launch the editor if needed."""
        client = UnrealMCPClient(host="127.0.0.1", port=MCP_PORT, timeout=30.0)
        deadline = time.monotonic() + 120.0
        while time.monotonic() < deadline:
            if client.connect():
                self.client = client
                self.event("editor_connected", endpoint=client.endpoint)
                return
            if not UE_EDITOR.exists():
                raise HpHudTestError(f"UE editor binary missing: {UE_EDITOR}")
            if time.monotonic() < deadline - 110.0:
                subprocess.Popen(
                    [str(UE_EDITOR), str(PROJECT_FILE),
                     "-ModelContextProtocolStartServer", f"-ModelContextProtocolPort={MCP_PORT}"],
                    cwd=str(PROJECT_ROOT),
                )
                self.event("editor_launched", editor=str(UE_EDITOR))
            time.sleep(5.0)
        raise HpHudTestError("UE MCP server did not become reachable within 120s")

    def _probe(self, script: str, argv: list[str] | None = None) -> dict[str, Any]:
        result = self.client.run_project_python_file(script, argv or [])
        stdout = str(result.get("stdout", "")).strip()
        try:
            return json.loads(stdout)
        except Exception as exc:
            raise HpHudTestError(f"probe {script} returned non-JSON: {stdout[:200]} ({exc})")

    def _board_action(self, action: str, arg: str = "") -> dict[str, Any]:
        result = self._probe(BATTLE_ACTION_PROBE, [action, arg])
        return result

    def _state(self) -> dict[str, Any]:
        return self._probe(STATE_PROBE, [])

    def _wait_for(self, label: str, predicate, timeout: float | None = None, interval: float = 0.4) -> dict[str, Any]:
        deadline = time.monotonic() + (timeout or self.timeout)
        last = None
        while time.monotonic() < deadline:
            last = predicate()
            if last:
                return last
            time.sleep(interval)
        raise HpHudTestError(f"timed out waiting for {label}: {last}")

    def _retry_select_battle(self, attempts: int = 10) -> dict[str, Any]:
        last = {"ok": False}
        for _ in range(attempts):
            last = self._board_action("select_battle")
            if last.get("ok"):
                return last
            time.sleep(1.0)
        raise HpHudTestError(f"select_battle kept failing: {last}")

    # ---------- Slate helpers ----------

    def _slate_snapshot(self) -> str:
        root = str(self.client.call_tool(
            "Snapshot", {"ref": "", "maxDepth": 3, "bIncludeSourceLocations": False},
            toolset_name=SLATE_TOOLSET, timeout=self.client.timeout))
        preview_ref = _slate_preview_window_ref(root)
        if not preview_ref:
            # Embedded (non-floating) PIE has no separate "Preview" window; the
            # game viewport lives inside the main editor window.
            matches = list(SLATE_WINDOW_PATTERN.finditer(root))
            fallback = re.search(r'window "[^"]*"[^\n]*\[ref=(\w+)\]', root)
            preview_ref = fallback.group(1) if fallback else ""
            self.event("slate_window_fallback", used=preview_ref)
        if not preview_ref:
            raise HpHudTestError(f"No Slate window found in snapshot: {root[:400]}")
        self.client.call_tool(
            "Observe", {"ref": preview_ref, "maxDepth": 80},
            toolset_name=SLATE_TOOLSET, timeout=self.client.timeout)
        time.sleep(0.15)
        return str(self.client.call_tool(
            "Snapshot", {"ref": preview_ref, "maxDepth": 80, "bIncludeSourceLocations": False},
            toolset_name=SLATE_TOOLSET, timeout=self.client.timeout))

    def _slate_button(self, label: str) -> dict[str, Any]:
        snapshot = self._slate_snapshot()
        named = [
            {"ref": m.group("ref"), "disabled": bool(m.group("disabled"))}
            for m in SLATE_BUTTON_PATTERN.finditer(snapshot)
            if m.group("label") == label and not bool(m.group("disabled"))
        ]
        if named:
            return named[-1]
        label_index = snapshot.rfind(f'"{label}"')
        if label_index < 0:
            raise HpHudTestError(f"Slate text '{label}' missing in preview snapshot")
        candidates = [
            {"ref": m.group("ref"), "disabled": bool(m.group("disabled"))}
            for m in SLATE_BUTTON_PATTERN.finditer(snapshot[:label_index])
        ]
        candidates = [c for c in candidates if not c["disabled"]]
        if not candidates:
            raise HpHudTestError(f"No enabled Slate button precedes '{label}'")
        return candidates[-1]

    def _slate_click(self, label: str) -> None:
        deadline = time.monotonic() + 20.0
        button = None
        while time.monotonic() < deadline:
            try:
                button = self._slate_button(label)
                break
            except HpHudTestError:
                time.sleep(0.5)
        if button is None:
            raise HpHudTestError(f"Slate button '{label}' never became available")
        ok = bool(self.client.call_tool(
            "Click", {"ref": button["ref"], "button": "left", "doubleClick": False},
            toolset_name=SLATE_TOOLSET, timeout=self.client.timeout))
        self.event("slate_click", label=label, ok=ok)
        if not ok:
            raise HpHudTestError(f"Slate click failed for '{label}'")
        time.sleep(0.8)

    # ---------- battle entry ----------

    def _enter_battle_from_main_menu(self) -> None:
        # Prefer the BlueprintCallable command route over Slate clicks: the
        # start button's Slate label round-trips with encoding corruption.
        # Retry until the command actually flips the screen (PIE may not be
        # fully settled immediately after start).
        deadline = time.monotonic() + 30.0
        started = False
        while time.monotonic() < deadline:
            start = self._board_action("start_game")
            screen_now = str(self._state().get("screen", "")).upper()
            self.event("start_game_attempt", ok=bool(start.get("ok")),
                       reason=start.get("reason", ""), screen=screen_now)
            if start.get("ok") and "WORLD_MAP" in screen_now:
                started = True
                break
            time.sleep(1.0)
        if not started:
            self._slate_click("开始游戏")
        self._wait_for("world map after Start", lambda: "WORLD_MAP" in str(self._state().get("screen", "")).upper())
        selected = self._board_action("select_qingshan")
        if not selected.get("ok"):
            self._slate_click("青山镇")
        self._wait_for("town after Qingshan", lambda: "TOWN" in str(self._state().get("screen", "")).upper())
        self.event("reached_town")

    def _ensure_battle(self) -> dict[str, Any]:
        state = self._state()
        screen = str(state.get("screen", ""))
        if "BATTLE" in screen.upper() and state.get("ok"):
            self.event("already_in_battle", screen=screen)
            return state

        if "MAIN_MENU" in screen.upper():
            self._enter_battle_from_main_menu()
            screen = str(self._state().get("screen", "")).upper()

        if "TOWN" in screen.upper():
            # SelectDungeonNode(Battle) opens a battle directly without a quest.
            battle_sel = self._board_action("select_battle")
            if battle_sel.get("ok"):
                self.event("select_battle_ok")
                state = self._wait_for(
                    "battle screen", lambda: "BATTLE" in str(self._state().get("screen", "")).upper())
                return state
            self.event("select_battle_failed", detail=battle_sel)
            # Fallback: accept quest -> EnterDungeon (generates the route map)
            # -> SelectDungeonNode(Battle), which now has reachable battle nodes.
            accepted = self._board_action("accept_quest")
            self.event("accept_quest", ok=bool(accepted.get("ok")), detail=accepted)
            self._probe(MAIN_PROBE, ["--town-command", "EnterDungeon"])
            self._wait_for("route map after EnterDungeon",
                           lambda: "DUNGEON_MAP" in str(self._state().get("screen", "")).upper())
            self.event("entered_route_map")
            # Visiting the Start node unlocks the first reachable battle layer.
            self._board_action("select_route", "0")
            battle_sel2 = self._retry_select_battle(10)
            self.event("select_battle_ok_after_dungeon")
            state = self._wait_for(
                "battle screen", lambda: "BATTLE" in str(self._state().get("screen", "")).upper())
            return state

        if "DUNGEON_MAP" in str(self._state().get("screen", "")).upper():
            battle_sel = self._board_action("select_battle")
            if battle_sel.get("ok"):
                self.event("select_battle_ok_from_route_map")
                state = self._wait_for(
                    "battle screen", lambda: "BATTLE" in str(self._state().get("screen", "")).upper())
                return state
            raise HpHudTestError(f"select_battle from route map failed: {battle_sel}")

        raise HpHudTestError(
            f"cannot auto-enter battle from screen {screen}; start PIE from the main menu/route map")

    # ---------- assertions ----------

    def _assert_hud_sync(self, label: str, allow_during_presentation: bool = False,
                         participants: list[str] | None = None) -> dict[str, Any]:
        """Assert every HUD text equals runtime HP.

        During an active presentation, participant units may show the baseline
        (pre-mutation) value; everyone else must already be live.
        """
        state = self._state()
        problems = self._hud_problems(state, allow_during_presentation, participants)
        if problems:
            self.failures.append(f"{label}: {', '.join(problems)}")
            self.event("hud_mismatch", label=label, problems=problems, state=state)
        else:
            self.event("hud_sync_ok", label=label, allow_during_presentation=allow_during_presentation)
        return state

    # ---------- battle actions ----------

    def _hand_ids(self) -> list[str]:
        return [c["instance_id"] for c in self._hand_ids_full()]

    def _hand_ids_full(self) -> list[dict[str, str]]:
        result = self._board_action("hand_ids")
        if not result.get("ok"):
            return []
        cards = []
        for entry in result.get("hand_ids", []):
            if isinstance(entry, dict):
                cards.append(
                    {
                        "instance_id": str(entry.get("instance_id", "")),
                        "card_id": str(entry.get("card_id", "")),
                    }
                )
        return cards

    def _legal_target_candidates(self) -> list[str]:
        state = self._state()
        units = state.get("runtime_units", [])
        enemies = [u["unit_id"] for u in units if "ENEMY" in str(u.get("side", "")).upper()]
        party = [u["unit_id"] for u in units if "PARTY" in str(u.get("side", "")).upper()]
        return enemies + party

    def _play_card(self, instance_id: str) -> dict[str, Any]:
        clicked = self._board_action("click_card", instance_id)
        if not clicked.get("ok"):
            return clicked
        targeting = self._board_action("targeting_active")
        if targeting.get("ok"):
            # Manual target card: try enemies first, then party members; the
            # board rejects illegal targets so the first accepted one wins.
            for candidate in self._legal_target_candidates():
                confirmed = self._board_action("confirm", candidate)
                if confirmed.get("ok"):
                    self.event("card_confirmed", instance_id=instance_id, target=candidate)
                    return {"ok": True, "auto": False, "target": candidate}
            self._board_action("cancel_targeting")
            return {"ok": False, "reason": "no_legal_target_found"}
        self.event("card_auto_played", instance_id=instance_id)
        return {"ok": True, "auto": True}

    def _hp_signature(self, state: dict[str, Any]) -> list[tuple[str, int]]:
        signature = []
        for unit in state.get("runtime_units", []):
            signature.append((str(unit.get("unit_id", "")), int(unit.get("hp", -1))))
        return sorted(signature)

    def _hud_problems(self, state: dict[str, Any], allow_during_presentation: bool = False,
                      participants: list[str] | None = None) -> list[str]:
        units = {u["unit_id"]: u for u in state.get("runtime_units", [])}
        huds = state.get("hud_entries", {})
        problems = []
        for unit_id, hud in huds.items():
            text = hud.get("hud_text") or ""
            runtime = units.get(unit_id, {}).get("hp")
            if runtime is None or " / " not in text:
                continue
            try:
                shown = int(text.split(" / ")[0].split(" ")[-1])
            except Exception:
                problems.append(f"{unit_id}: unparsable text {text!r}")
                continue
            if shown == runtime:
                continue
            if allow_during_presentation and unit_id in (participants or []):
                continue
            problems.append(f"{unit_id}: HUD={shown} runtime={runtime}")
        return problems

    def _wait_presentation_idle(self, label: str) -> None:
        """Wait until every HUD equals runtime and the state stops changing."""
        deadline = time.monotonic() + max(self.timeout, 20.0)
        last_signature = None
        while time.monotonic() < deadline:
            state = self._state()
            problems = self._hud_problems(state)
            if not problems:
                signature = self._hp_signature(state)
                if last_signature is not None and last_signature == signature:
                    return
                last_signature = signature
            else:
                last_signature = None
            time.sleep(0.5)
        raise HpHudTestError(f"timed out waiting for HUD/runtime convergence ({label}): {problems or 'unstable'}")

    # ---------- main run ----------

    def run(self) -> dict[str, Any]:
        self._ensure_editor()
        # Restart PIE into a clean state: a lingering is_in_pie flag after a prior
        # stop leaves no PIE Slate window to drive.
        if self.client.is_in_pie():
            self.event("stopping_stale_pie")
            self.client.stop_pie()
            self.client.wait_for_pie_state(False, timeout=30.0)
        self.event("starting_pie")
        self.client.start_pie(warmup_seconds=2.0)
        self._wait_for("PIE running", lambda: self.client.is_in_pie(), timeout=60.0)
        # Wait for the PIE world to expose a runtime screen before driving it.
        self._wait_for("PIE world ready", lambda: str(self._state().get("screen", "")).strip() != "", timeout=60.0)
        self._ensure_battle()
        state = self._state()
        self.event("battle_ready", screen=state.get("screen"), phase=state.get("phase"))

        # Ensure we are in the player phase before driving card plays.
        phase = str(state.get("phase", "")).upper()
        if "ENEMY" in phase:
            self._wait_presentation_idle("enemy presentation idle")
            self._wait_for("player phase", lambda: "PLAYER" in str(self._state().get("phase", "")).upper(),
                           timeout=60.0)

        self._assert_hud_sync("initial")

        rounds_played = 0
        max_rounds = 4
        while rounds_played < max_rounds:
            phase = str(self._state().get("phase", "")).upper()
            if "ENEMY" in phase:
                self._wait_presentation_idle("enemy phase idle")
                self._wait_for("player phase again", lambda: "PLAYER" in str(self._state().get("phase", "")).upper(),
                               timeout=60.0)
                continue

            # 0) GuiYuanShu (pure Heal, no damage packet) special case: the HUD
            #    must reflect the healed value IMMEDIATELY and stay live after
            #    the enemy phase that follows.
            for hand_card in self._hand_ids_full():
                if "GuiYuanShu" in hand_card.get("card_id", ""):
                    clicked = self._board_action("click_card", hand_card["instance_id"])
                    if clicked.get("ok"):
                        confirmed = self._board_action("confirm", "Player")
                        self.event("guiyuan_played", ok=bool(confirmed.get("ok")),
                                   card_id=hand_card.get("card_id"), detail=confirmed)
                        time.sleep(0.3)
                        self._assert_hud_sync("after guiyuan heal (immediate)")
                        self._wait_presentation_idle("guiyuan settle")
                        self._assert_hud_sync("after guiyuan settle")
                    break

            # 1) Play every card currently in hand.
            played_any = False
            hand_ids = self._hand_ids()
            for instance_id in hand_ids:
                result = self._play_card(instance_id)
                if not result.get("ok"):
                    continue  # unplayable or no legal target this moment
                played_any = True
                # Sample DURING the presentation: non-participants must be live.
                participants = []
                if not result.get("auto"):
                    participants.append(result.get("target", ""))
                time.sleep(0.15)
                self._assert_hud_sync(
                    f"during-presentation card {instance_id}",
                    allow_during_presentation=True,
                    participants=participants,
                )
                self._wait_presentation_idle(f"card {instance_id} presentation idle")
                self._assert_hud_sync(f"after card {instance_id}")

            # 2) End the player phase so enemies attack (heal/defend may already
            #    have happened above; enemy damage is the required live path).
            if played_any:
                ended = self._board_action("end_phase")
                self.event("end_phase", ok=bool(ended.get("ok")), detail=ended)
                self._wait_presentation_idle("enemy attacks idle")
                self._assert_hud_sync(f"after enemy phase round{rounds_played}")
            rounds_played += 1

        # Multi-board detection from logs.
        self._board_log_summary()
        return {
            "ok": not self.failures,
            "failures": self.failures,
            "events": self.events,
            "board_alive": self.board_alive,
        }

    def _board_log_summary(self) -> None:
        try:
            lines = self.client.get_recent_log_lines(num_lines=5000, pattern="[Board]")
        except Exception:
            return
        constructed = [l for l in lines if "constructed" in l]
        destructed = [l for l in lines if "destructed" in l]
        refresh = [l for l in lines if "refresh" in l]
        alive = None
        if constructed:
            match = __import__("re").search(r"alive=(\d+)", constructed[-1])
            if match:
                alive = int(match.group(1))
        self.board_alive = alive
        refreshing_names = set()
        for line in refresh:
            match = __import__("re").search(r"refresh name=(\S+)", line)
            if match:
                refreshing_names.add(match.group(1))
        self.event(
            "board_log_summary",
            constructed=len(constructed),
            destructed=len(destructed),
            refresh=len(refresh),
            alive=alive,
            refreshing_names=sorted(refreshing_names),
        )
        if alive is not None and alive > 1:
            self.failures.append(f"multiple battle board instances alive: {alive}")
        if len(refreshing_names) > 1:
            self.failures.append(f"multiple boards refreshing concurrently: {sorted(refreshing_names)}")

    def close(self) -> None:
        if self.client and not self.keep_pie and self.client.is_in_pie():
            try:
                self.client.stop_pie()
            except Exception:
                pass


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="GameXXK battle HP HUD sync test")
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--keep-pie", action="store_true", help="leave PIE running on exit")
    args = parser.parse_args(argv)

    tester = HpHudTester(timeout=args.timeout, keep_pie=args.keep_pie)
    try:
        result = tester.run()
    except HpHudTestError as exc:
        result = {"ok": False, "error": str(exc), "events": tester.events, "failures": tester.failures}
    finally:
        tester.close()
    print(json.dumps(result, ensure_ascii=False, indent=2), flush=True)
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
