from __future__ import annotations

import contextlib
import io
import json
import math
import subprocess
import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

from scripts import run_qingshan_whitebox_b0r as coordinator
from scripts.run_qingshan_town_pcg_vertical_slice import run_vertical_slice


ROOT = Path(__file__).resolve().parents[1]
ASSEMBLY_SCRIPT = "Content/Python/gamexxk_assemble_qingshan_whitebox_b0r.py"


def _response(payload):
    return {"stdout": json.dumps(payload)}


class FakeClient:
    def __init__(self, setup=None, finals=()):
        self.setup = setup if setup is not None else {"success": True}
        self.finals = list(finals)
        self.calls = []

    @staticmethod
    def _resolve(value):
        if isinstance(value, BaseException):
            raise value
        if isinstance(value, dict) and set(value) == {"stdout"}:
            return value
        return _response(value)

    def run_project_python_file(self, relative_path, argv):
        self.calls.append((relative_path, list(argv)))
        if argv == ["--phase", "setup"]:
            return self._resolve(self.setup)
        if argv == ["--phase", "finalize"]:
            value = self.finals.pop(0) if self.finals else {
                "success": True,
                "complete": False,
                "pending": True,
            }
            return self._resolve(value)
        raise AssertionError(f"unexpected argv: {argv!r}")


class FakeClock:
    def __init__(self, now=0.0):
        self.now = float(now)
        self.sleeps = []

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        if seconds <= 0:
            raise AssertionError(f"nonpositive sleep: {seconds!r}")
        self.sleeps.append(seconds)
        self.now += seconds


class WhiteboxB0RCoordinatorTests(unittest.TestCase):
    def _run(self, setup=None, finals=(), timeout=3.0, poll=0.75):
        if setup is None:
            setup = {
                "success": True,
                "generate": {"scheduled": True, "task_id": 41},
            }
        client = FakeClient(setup, finals)
        clock = FakeClock()
        result = coordinator.run_whitebox_b0r(
            client,
            timeout_seconds=timeout,
            poll_interval=poll,
            monotonic=clock.monotonic,
            sleep=clock.sleep,
        )
        return result, client, clock

    def test_import_does_not_load_ue_client_or_unreal(self):
        code = (
            "import sys; import scripts.run_qingshan_whitebox_b0r; "
            "assert 'scripts.ue_mcp_client' not in sys.modules; "
            "assert 'ue_mcp_client' not in sys.modules; "
            "assert 'unreal' not in sys.modules"
        )
        completed = subprocess.run(
            [sys.executable, "-c", code],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_setup_failure_is_terminal_and_preserves_task_id(self):
        setup = {
            "success": False,
            "generate": {"task_id": 12},
            "error": "setup refused",
        }
        result, client, clock = self._run(setup=setup)
        self.assertEqual(result, {
            "success": False,
            "terminal_state": "setup_error",
            "attempts": 0,
            "elapsed_seconds": 0.0,
            "task_id": 12,
            "setup": setup,
            "final": None,
            "error": "setup refused",
        })
        self.assertEqual(client.calls, [(ASSEMBLY_SCRIPT, ["--phase", "setup"])])
        self.assertEqual(clock.sleeps, [])

    def test_setup_exception_is_structured(self):
        result, _, _ = self._run(setup=RuntimeError("MCP unavailable"))
        self.assertFalse(result["success"])
        self.assertEqual(result["terminal_state"], "setup_error")
        self.assertEqual(result["attempts"], 0)
        self.assertIsNone(result["setup"])
        self.assertIn("MCP unavailable", result["error"])

    def test_pending_then_complete_polls_finalize_only(self):
        pending = {"success": True, "complete": False, "pending": True}
        complete = {"success": True, "complete": True, "pending": False}
        result, client, clock = self._run(finals=[pending, complete])
        self.assertTrue(result["success"])
        self.assertEqual(result["terminal_state"], "complete")
        self.assertEqual(result["attempts"], 2)
        self.assertEqual(result["elapsed_seconds"], 0.75)
        self.assertEqual(result["task_id"], 41)
        self.assertEqual(result["final"], complete)
        self.assertEqual(clock.sleeps, [0.75])
        self.assertEqual(client.calls, [
            (ASSEMBLY_SCRIPT, ["--phase", "setup"]),
            (ASSEMBLY_SCRIPT, ["--phase", "finalize"]),
            (ASSEMBLY_SCRIPT, ["--phase", "finalize"]),
        ])

    def test_finalize_failure_is_terminal_without_sleep(self):
        final = {
            "success": False,
            "complete": False,
            "pending": False,
            "error": "generation aborted",
        }
        result, _, clock = self._run(finals=[final])
        self.assertEqual(result["terminal_state"], "error")
        self.assertEqual(result["attempts"], 1)
        self.assertEqual(result["error"], "generation aborted")
        self.assertEqual(result["final"], final)
        self.assertEqual(clock.sleeps, [])

    def test_success_without_pending_or_complete_is_terminal_error(self):
        result, _, clock = self._run(finals=[{"success": True}])
        self.assertFalse(result["success"])
        self.assertEqual(result["terminal_state"], "error")
        self.assertEqual(result["error"], "finalize failed")
        self.assertEqual(clock.sleeps, [])

    def test_finalize_exception_is_structured_with_attempt_count(self):
        result, _, _ = self._run(finals=[OSError("socket closed")])
        self.assertFalse(result["success"])
        self.assertEqual(result["terminal_state"], "error")
        self.assertEqual(result["attempts"], 1)
        self.assertIn("socket closed", result["error"])

    def test_timeout_at_deadline_clips_sleep_and_never_sleeps_nonpositive(self):
        result, client, clock = self._run(timeout=1.0, poll=0.75)
        self.assertFalse(result["success"])
        self.assertEqual(result["terminal_state"], "timeout")
        self.assertEqual(result["attempts"], 3)
        self.assertEqual(result["elapsed_seconds"], 1.0)
        self.assertEqual(clock.sleeps, [0.75, 0.25])
        self.assertEqual(
            sum(argv == ["--phase", "finalize"] for _, argv in client.calls),
            3,
        )
        self.assertIn("1.000s deadline", result["error"])

    def test_invalid_empty_and_non_object_json_are_structured(self):
        invalid_stdout = ("", "not-json", "[]")
        for stdout in invalid_stdout:
            with self.subTest(stdout=stdout):
                result, _, _ = self._run(setup={"stdout": stdout})
                self.assertEqual(result["terminal_state"], "setup_error")
                self.assertEqual(result["attempts"], 0)
                self.assertTrue(result["error"])
        for stdout in invalid_stdout:
            with self.subTest(final_stdout=stdout):
                result, _, _ = self._run(finals=[{"stdout": stdout}])
                self.assertEqual(result["terminal_state"], "error")
                self.assertEqual(result["attempts"], 1)
                self.assertTrue(result["error"])

    def test_decoder_uses_last_stdout_line(self):
        payload = {"success": True, "task": "B0R"}
        decoded = coordinator._decode_phase_response({
            "stdout": "editor diagnostic\n" + json.dumps(payload),
        })
        self.assertEqual(decoded, payload)

    def test_nonpositive_and_nonfinite_timeout_or_poll_are_rejected(self):
        invalid = (0.0, -1.0, math.inf, -math.inf, math.nan)
        for timeout in invalid:
            with self.subTest(timeout=timeout):
                with self.assertRaises(ValueError):
                    coordinator.run_whitebox_b0r(FakeClient(), timeout_seconds=timeout)
        for poll in invalid:
            with self.subTest(poll=poll):
                with self.assertRaises(ValueError):
                    coordinator.run_whitebox_b0r(FakeClient(), poll_interval=poll)

    def test_matches_existing_coordinator_for_successful_state_machine(self):
        setup = {"success": True, "generate": {"task_id": 88}}
        finals = [
            {"success": True, "complete": False, "pending": True},
            {"success": True, "complete": True, "pending": False},
        ]
        b0r_client, old_client = FakeClient(setup, finals), FakeClient(setup, finals)
        b0r_clock, old_clock = FakeClock(), FakeClock()
        b0r = coordinator.run_whitebox_b0r(
            b0r_client, monotonic=b0r_clock.monotonic, sleep=b0r_clock.sleep
        )
        old = run_vertical_slice(
            old_client, monotonic=old_clock.monotonic, sleep=old_clock.sleep
        )
        self.assertEqual(b0r, old)
        self.assertEqual([argv for _, argv in b0r_client.calls], [argv for _, argv in old_client.calls])

    def test_cli_parses_bounded_timeout_and_poll_interval(self):
        args = coordinator._parse_args([
            "--timeout-seconds", "300", "--poll-interval", "0.01"
        ])
        self.assertEqual(args.timeout_seconds, 300.0)
        self.assertEqual(args.poll_interval, 0.01)
        for argv in (
            ["--timeout-seconds", "0.999"],
            ["--timeout-seconds", "301"],
            ["--timeout-seconds", "nan"],
            ["--poll-interval", "0"],
            ["--poll-interval", "61"],
            ["--poll-interval", "inf"],
        ):
            with self.subTest(argv=argv), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    coordinator._parse_args(argv)

    def test_main_uses_adequate_client_timeout_and_prints_one_json_result(self):
        instances = []

        class DisconnectedClient:
            endpoint = "http://fake/mcp"

            def __init__(self, timeout):
                self.timeout = timeout
                instances.append(self)

            def connect(self):
                return False

        fake_module = SimpleNamespace(UnrealMCPClient=DisconnectedClient)
        output = io.StringIO()
        with mock.patch.dict(sys.modules, {"scripts.ue_mcp_client": fake_module}):
            with contextlib.redirect_stdout(output):
                exit_code = coordinator.main([
                    "--timeout-seconds", "60", "--poll-interval", "0.5"
                ])
        lines = output.getvalue().splitlines()
        self.assertEqual(exit_code, 1)
        self.assertEqual(len(lines), 1)
        self.assertFalse(json.loads(lines[0])["success"])
        self.assertGreaterEqual(instances[0].timeout, 75.0)


if __name__ == "__main__":
    unittest.main()
