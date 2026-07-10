from __future__ import annotations

import json
import unittest

try:
    from scripts.run_qingshan_town_pcg_vertical_slice import run_vertical_slice
except ModuleNotFoundError:
    run_vertical_slice = None


def _response(payload):
    return {"stdout": json.dumps(payload)}


class FakeClient:
    def __init__(self, setup, finals):
        self.setup = setup
        self.finals = list(finals)
        self.phases = []

    def run_project_python_file(self, relative_path, argv):
        self.phases.append(tuple(argv))
        phase = argv[-1]
        if phase == "setup":
            return _response(self.setup)
        if self.finals:
            return _response(self.finals.pop(0))
        return _response({"success": True, "complete": False, "pending": True})


class FakeClock:
    def __init__(self):
        self.now = 0.0
        self.sleeps = []

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.sleeps.append(seconds)
        self.now += seconds


class VerticalSliceCoordinatorTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(run_vertical_slice, "coordinator module must exist")

    def _run(self, finals, timeout=3.0):
        setup = {
            "success": True,
            "generate": {"scheduled": True, "task_id": 17},
        }
        client = FakeClient(setup, finals)
        clock = FakeClock()
        result = run_vertical_slice(
            client,
            timeout_seconds=timeout,
            poll_interval=0.75,
            monotonic=clock.monotonic,
            sleep=clock.sleep,
        )
        return result, client, clock

    def test_immediate_complete(self):
        result, client, clock = self._run(
            [{"success": True, "complete": True, "pending": False}]
        )
        self.assertTrue(result["success"])
        self.assertEqual(result["terminal_state"], "complete")
        self.assertEqual(result["attempts"], 1)
        self.assertEqual(result["task_id"], 17)
        self.assertEqual(clock.sleeps, [])
        self.assertEqual(client.phases.count(("--phase", "setup")), 1)

    def test_pending_then_complete_never_reschedules_setup(self):
        result, client, clock = self._run(
            [
                {"success": True, "complete": False, "pending": True},
                {"success": True, "complete": True, "pending": False},
            ]
        )
        self.assertTrue(result["success"])
        self.assertEqual(result["attempts"], 2)
        self.assertEqual(clock.sleeps, [0.75])
        self.assertEqual(client.phases, [
            ("--phase", "setup"),
            ("--phase", "finalize"),
            ("--phase", "finalize"),
        ])

    def test_terminal_error_stops_without_retry(self):
        final = {
            "success": False,
            "complete": False,
            "pending": False,
            "error": "generation aborted",
        }
        result, client, clock = self._run([final])
        self.assertFalse(result["success"])
        self.assertEqual(result["terminal_state"], "error")
        self.assertEqual(result["attempts"], 1)
        self.assertEqual(result["error"], "generation aborted")
        self.assertEqual(clock.sleeps, [])

    def test_timeout_is_bounded_and_does_not_reschedule(self):
        result, client, clock = self._run([], timeout=1.5)
        self.assertFalse(result["success"])
        self.assertEqual(result["terminal_state"], "timeout")
        self.assertEqual(result["attempts"], 3)
        self.assertAlmostEqual(result["elapsed_seconds"], 1.5)
        self.assertEqual(client.phases.count(("--phase", "setup")), 1)
        self.assertEqual(client.phases.count(("--phase", "finalize")), 3)


if __name__ == "__main__":
    unittest.main()
