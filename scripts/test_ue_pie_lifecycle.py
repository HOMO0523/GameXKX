"""Focused lifecycle contracts for sequential PIE automation."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path
from unittest.mock import patch


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import gamexxk_real_play_flow_mcp as flow
import ue_mcp_client as mcp
import ue_tdd_pipeline as pipeline


class PIESettleWaitTests(unittest.TestCase):
    def test_wait_for_pie_state_observes_true_to_false_transition(self) -> None:
        client = mcp.UnrealMCPClient()
        with patch.object(client, "is_in_pie", side_effect=[True, False]) as is_in_pie:
            with patch.object(mcp.time, "sleep") as sleep:
                self.assertTrue(client.wait_for_pie_state(False, timeout=1.0, interval=0.125))

        self.assertEqual(2, is_in_pie.call_count)
        sleep.assert_called_once_with(0.125)


class _PipelineLifecycleClient:
    def __init__(self, settle_results: list[bool]) -> None:
        self.events: list[tuple[object, ...]] = []
        self._pie_running = True
        self._settle_results = list(settle_results)

    def clear_log_buffer(self) -> None:
        self.events.append(("clear_log_buffer",))

    def write_log(self, message: str, severity: str) -> None:
        self.events.append(("write_log", message, severity))

    def is_in_pie(self) -> bool:
        self.events.append(("is_in_pie",))
        return self._pie_running

    def stop_pie(self) -> bool:
        self.events.append(("stop_pie",))
        self._pie_running = False
        return True

    def wait_for_pie_state(self, expected: bool, timeout: float = 30.0, interval: float = 0.25) -> bool:
        self.events.append(("wait_for_pie_state", expected))
        return self._settle_results.pop(0)

    def start_pie(self, warmup_seconds: float) -> bool:
        self.events.append(("start_pie", warmup_seconds))
        self._pie_running = True
        return True

    def get_pie_world_time(self) -> float:
        self.events.append(("get_pie_world_time",))
        return 2.0

    def filter_tdd_lines(self, num_lines: int, pattern: str = "") -> list[str]:
        self.events.append(("filter_tdd_lines", num_lines, pattern))
        return []

    def get_recent_log_lines(self, num_lines: int) -> list[str]:
        self.events.append(("get_recent_log_lines", num_lines))
        return []


class PipelinePIELifecycleTests(unittest.TestCase):
    def test_pipeline_waits_after_each_stop_before_starting_or_returning(self) -> None:
        client = _PipelineLifecycleClient([True, True])
        with patch.object(pipeline, "wait_for_mcp", return_value=client):
            with patch.object(pipeline.time, "sleep"):
                result = pipeline.run_tdd_cycle(build=False, launch=False, pie_duration=0.0)

        self.assertTrue(result["success"])
        stops = [index for index, event in enumerate(client.events) if event[0] == "stop_pie"]
        settles = [index for index, event in enumerate(client.events) if event[0] == "wait_for_pie_state"]
        start = next(index for index, event in enumerate(client.events) if event[0] == "start_pie")
        self.assertEqual(2, len(stops))
        self.assertEqual(2, len(settles))
        self.assertLess(stops[0], settles[0])
        self.assertLess(settles[0], start)
        self.assertLess(start, stops[1])
        self.assertLess(stops[1], settles[1])

    def test_pipeline_does_not_start_pie_when_existing_pie_never_stops(self) -> None:
        client = _PipelineLifecycleClient([False])
        with patch.object(pipeline, "wait_for_mcp", return_value=client):
            with patch.object(pipeline.time, "sleep"):
                result = pipeline.run_tdd_cycle(build=False, launch=False, pie_duration=0.0)

        self.assertFalse(result["success"])
        self.assertIn("PIE did not stop", result["error"])
        self.assertNotIn("start_pie", [event[0] for event in client.events])


class _FlowStopTimeoutClient:
    endpoint = "http://fake-mcp:18765/mcp"

    def __init__(self) -> None:
        self.events: list[tuple[object, ...]] = []

    def connect(self) -> bool:
        self.events.append(("connect",))
        return True

    def is_in_pie(self) -> bool:
        self.events.append(("is_in_pie",))
        return True

    def stop_pie(self) -> bool:
        self.events.append(("stop_pie",))
        return True

    def wait_for_pie_state(self, expected: bool, timeout: float = 30.0, interval: float = 0.25) -> bool:
        self.events.append(("wait_for_pie_state", expected))
        return False

    def call_tool(self, *args, **kwargs):
        self.events.append(("call_tool", args, kwargs))
        raise AssertionError("A flow must not load a level or start PIE before teardown settles")


class RealFlowPIELifecycleTests(unittest.TestCase):
    def test_real_flow_does_not_load_or_start_when_existing_pie_stop_times_out(self) -> None:
        client = _FlowStopTimeoutClient()
        harness = object.__new__(flow.RealFlowHarness)
        harness.client = client
        harness.events = []
        harness.keep_pie = False
        harness.input = None
        harness.battle_hud_fixture_may_be_applied = False
        harness._screenshot_contexts = {}
        harness._default_save_backup_active = False

        with self.assertRaisesRegex(RuntimeError, "PIE did not stop"):
            harness.run()

        self.assertEqual(
            [
                ("connect",),
                ("is_in_pie",),
                ("stop_pie",),
                ("wait_for_pie_state", False),
            ],
            client.events,
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
