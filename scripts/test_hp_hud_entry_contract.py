#!/usr/bin/env python3
"""Headless contract tests for the HP HUD live runner's entry flow."""

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path
from unittest import mock


PROJECT_ROOT = Path(__file__).resolve().parents[1]
RUNNER_PATH = PROJECT_ROOT / "scripts" / "test_hp_hud_updates.py"


def load_runner():
    spec = importlib.util.spec_from_file_location("gamexxk_hp_hud_updates", RUNNER_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load HP HUD runner: {RUNNER_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class HpHudEntryContractTest(unittest.TestCase):
    def test_editor_boot_launches_at_most_one_process_while_mcp_becomes_ready(self) -> None:
        runner = load_runner()
        tester = runner.HpHudTester(timeout=0.1)

        class FakeClient:
            endpoint = "http://127.0.0.1:18765/mcp"

            def __init__(self) -> None:
                self.connect_results = iter((False, False, True))

            def connect(self) -> bool:
                return next(self.connect_results)

        client = FakeClient()
        clock = iter((0.0, 0.0, 0.0, 5.0, 5.0, 10.0))
        with (
            mock.patch.object(runner, "UnrealMCPClient", return_value=client),
            mock.patch.object(runner.time, "monotonic", side_effect=lambda: next(clock)),
            mock.patch.object(runner.time, "sleep", return_value=None),
            mock.patch.object(runner.subprocess, "Popen") as popen,
        ):
            tester._ensure_editor()

        self.assertIs(tester.client, client)
        self.assertEqual(popen.call_count, 1)
        command = popen.call_args.args[0]
        self.assertIn("-Unattended", command)
        self.assertIn("-UnattendedInput", command)
        self.assertIn("-NoZenAutoLaunch", command)
        self.assertIn("-DDC-ForceMemoryCache", command)
        self.assertEqual(
            popen.call_args.kwargs.get("env", {}).get("UE_SKIP_UBT_SDK_SETUP"),
            "1",
        )
        self.assertEqual(
            [event["event"] for event in tester.events],
            ["editor_launched", "editor_connected"],
        )

    def test_cleanup_does_not_mask_the_result_when_mcp_disconnects(self) -> None:
        runner = load_runner()
        tester = runner.HpHudTester(timeout=0.1)
        client = mock.Mock()
        client.is_in_pie.side_effect = ConnectionRefusedError("editor exited")
        tester.client = client

        tester.close()

        client.stop_pie.assert_not_called()
        self.assertEqual(tester.events[-1]["event"], "cleanup_mcp_unavailable")

    def test_main_menu_start_accepts_the_current_direct_town_destination(self) -> None:
        runner = load_runner()
        tester = runner.HpHudTester(timeout=0.1)
        actions: list[str] = []
        waits: list[str] = []

        tester._board_action = lambda action, arg="": actions.append(action) or {"ok": True}
        tester._state = lambda: {"ok": True, "screen": "EGameXXKScreen::Town"}
        tester._slate_click = lambda label: self.fail(f"direct Town start must not click Slate button {label!r}")

        def wait_for(label, predicate, timeout=None, interval=0.4):
            waits.append(label)
            self.assertTrue(predicate(), f"predicate for {label!r} must accept Town")
            return tester._state()

        tester._wait_for = wait_for
        clock = iter((0.0, 0.0, 31.0))
        with (
            mock.patch.object(runner.time, "monotonic", side_effect=lambda: next(clock)),
            mock.patch.object(runner.time, "sleep", return_value=None),
        ):
            tester._enter_battle_from_main_menu()

        self.assertEqual(actions, ["start_game"])
        self.assertEqual(waits, ["town after Start"])
        self.assertTrue(any(event.get("event") == "reached_town" for event in tester.events))


if __name__ == "__main__":
    unittest.main()
