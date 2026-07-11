from __future__ import annotations

import ast
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
TASK4_SCRIPT = ROOT / "Content/Python/gamexxk_assemble_qingshan_whitebox_b0r.py"
ASSEMBLY_SCRIPT = "Content/Python/gamexxk_assemble_qingshan_whitebox_b0r.py"
KINDS = ("buildings", "foliage", "mountains")
EXPECTED_COUNTS = {"buildings": 16, "foliage": 100, "mountains": 24}


def _task4_constants():
    tree = ast.parse(TASK4_SCRIPT.read_text(encoding="utf-8"))
    wanted = {"PHASE_SETUP", "PHASE_FINALIZE", "EXPECTED_COUNTS"}
    return {
        node.targets[0].id: ast.literal_eval(node.value)
        for node in tree.body
        if isinstance(node, ast.Assign)
        and len(node.targets) == 1
        and isinstance(node.targets[0], ast.Name)
        and node.targets[0].id in wanted
    }


def _setup_payload(task_ids=None):
    task_ids = task_ids or {kind: index + 101 for index, kind in enumerate(KINDS)}
    return {
        "success": True,
        "complete": False,
        "pending": True,
        "phase": "setup",
        "generation": {
            kind: {
                "success": True,
                "scheduled": True,
                "generating": True,
                "generated": False,
                "task_id": task_ids[kind],
            }
            for kind in KINDS
        },
    }


def _pending_payload():
    return {
        "success": True,
        "complete": False,
        "pending": True,
        "phase": "finalize",
        "statuses": {
            kind: {"success": True, "generating": True, "generated": False}
            for kind in KINDS
        },
    }


def _complete_payload():
    return {
        "success": True,
        "complete": True,
        "pending": False,
        "phase": "finalize",
        "statuses": {
            kind: {"success": True, "generating": False, "generated": True}
            for kind in KINDS
        },
        "actual_counts": dict(EXPECTED_COUNTS),
    }


def _failure_payload(phase="finalize", error="generation aborted"):
    payload = _pending_payload()
    payload.update(success=False, complete=False, pending=False, phase=phase, error=error)
    payload["statuses"]["buildings"].update(generating=False, generated=False)
    return payload


class RawStdout:
    def __init__(self, stdout):
        self.stdout = stdout


class Timed:
    def __init__(self, payload, seconds):
        self.payload = payload
        self.seconds = seconds


class FakeClient:
    def __init__(self, clock, setup=None, finals=()):
        self.clock = clock
        self.setup = setup if setup is not None else _setup_payload()
        self.finals = list(finals)
        self.calls = []

    def run_project_python_file(self, *args, **kwargs):
        raise AssertionError("coordinator must not use the client's hidden 180s helper")

    def call_tool(self, tool_name, arguments, toolset_name=None, timeout=None):
        argv = json.loads(arguments["argv_json"])
        self.calls.append({
            "tool_name": tool_name,
            "arguments": dict(arguments),
            "toolset_name": toolset_name,
            "timeout": timeout,
            "argv": argv,
        })
        if argv == ["--phase", "setup"]:
            value = self.setup
        elif argv == ["--phase", "finalize"]:
            value = self.finals.pop(0) if self.finals else _pending_payload()
        else:
            raise AssertionError(f"unexpected argv: {argv!r}")
        if isinstance(value, Timed):
            self.clock.advance(value.seconds)
            value = value.payload
        if isinstance(value, BaseException):
            raise value
        if isinstance(value, RawStdout):
            response = {"stdout": value.stdout}
        else:
            response = {
                "stdout": json.dumps(value, separators=(",", ":"), allow_nan=True)
            }
        # This matches UnrealMCPClient.call_tool(): the Python toolset's string
        # returnValue is returned as a string for the caller to decode.
        return json.dumps(response, separators=(",", ":"), allow_nan=True)


class FakeClock:
    def __init__(self, now=0.0):
        self.now = float(now)
        self.sleeps = []
        self.monotonic_error = None
        self.sleep_error = None

    def monotonic(self):
        if self.monotonic_error is not None:
            raise self.monotonic_error
        return self.now

    def sleep(self, seconds):
        if self.sleep_error is not None:
            raise self.sleep_error
        if seconds <= 0:
            raise AssertionError(f"nonpositive sleep: {seconds!r}")
        self.sleeps.append(seconds)
        self.advance(seconds)

    def advance(self, seconds):
        self.now += seconds


class ScriptedClock(FakeClock):
    def __init__(self, values):
        super().__init__()
        self.values = list(values)

    def monotonic(self):
        value = self.values.pop(0)
        if isinstance(value, BaseException):
            raise value
        return value


class WhiteboxB0RCoordinatorTests(unittest.TestCase):
    def _run(self, setup=None, finals=(), timeout=3.0, poll=0.75, clock=None):
        clock = clock or FakeClock()
        client = FakeClient(clock, setup, finals)
        result = coordinator.run_whitebox_b0r(
            client,
            timeout_seconds=timeout,
            poll_interval=poll,
            monotonic=clock.monotonic,
            sleep=clock.sleep,
        )
        return result, client, clock

    def test_fixtures_match_task4_phase_and_count_constants(self):
        self.assertEqual(_task4_constants(), {
            "PHASE_SETUP": "setup",
            "PHASE_FINALIZE": "finalize",
            "EXPECTED_COUNTS": EXPECTED_COUNTS,
        })

    def test_import_does_not_load_ue_client_or_unreal(self):
        code = (
            "import sys; import scripts.run_qingshan_whitebox_b0r; "
            "assert 'scripts.ue_mcp_client' not in sys.modules; "
            "assert 'ue_mcp_client' not in sys.modules; "
            "assert 'unreal' not in sys.modules"
        )
        completed = subprocess.run(
            [sys.executable, "-c", code], cwd=ROOT,
            capture_output=True, text=True, check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_pending_then_complete_uses_exact_script_phases_and_task_ids(self):
        task_ids = {"buildings": 7, "foliage": 8, "mountains": 9}
        result, client, clock = self._run(
            setup=_setup_payload(task_ids), finals=[_pending_payload(), _complete_payload()]
        )
        self.assertTrue(result["success"])
        self.assertEqual(result["terminal_state"], "complete")
        self.assertEqual(result["attempts"], 2)
        self.assertEqual(result["elapsed_seconds"], 0.75)
        self.assertEqual(result["task_ids"], task_ids)
        self.assertEqual(clock.sleeps, [0.75])
        self.assertEqual([call["argv"] for call in client.calls], [
            ["--phase", "setup"],
            ["--phase", "finalize"],
            ["--phase", "finalize"],
        ])
        for call in client.calls:
            self.assertEqual(call["tool_name"], "run_project_python_file")
            self.assertEqual(call["arguments"]["relative_path"], ASSEMBLY_SCRIPT)
            self.assertTrue(call["arguments"]["run_as_main"])

    def test_calls_public_tool_with_remaining_deadline_timeout(self):
        clock = FakeClock()
        result, client, _ = self._run(
            setup=Timed(_setup_payload(), 1.0), finals=[Timed(_complete_payload(), 0.5)],
            timeout=5.0, clock=clock,
        )
        self.assertTrue(result["success"])
        self.assertEqual([call["timeout"] for call in client.calls], [5.0, 4.0])

    def test_setup_failure_and_exception_are_structured(self):
        failed = _setup_payload()
        failed.update(success=False, pending=False, error="setup refused")
        for setup, expected in ((failed, "setup refused"), (RuntimeError("MCP unavailable"), "MCP unavailable")):
            with self.subTest(setup=setup):
                result, client, clock = self._run(setup=setup)
                self.assertFalse(result["success"])
                self.assertEqual(result["terminal_state"], "setup_error")
                self.assertEqual(result["attempts"], 0)
                self.assertEqual(result["task_ids"], {})
                self.assertIn(expected, result["error"])
                self.assertEqual(len(client.calls), 1)
                self.assertEqual(clock.sleeps, [])

    def test_finalize_terminal_error_and_exception_do_not_retry(self):
        for final, expected in (
            (_failure_payload(error="generation aborted"), "generation aborted"),
            (OSError("socket closed"), "socket closed"),
        ):
            with self.subTest(final=final):
                result, client, clock = self._run(finals=[final])
                self.assertEqual(result["terminal_state"], "error")
                self.assertEqual(result["attempts"], 1)
                self.assertIn(expected, result["error"])
                self.assertEqual(len(client.calls), 2)
                self.assertEqual(clock.sleeps, [])

    def test_timeout_clips_sleep_and_never_calls_at_deadline(self):
        result, client, clock = self._run(timeout=1.0, poll=0.75)
        self.assertEqual(result["terminal_state"], "timeout")
        self.assertEqual(result["attempts"], 2)
        self.assertEqual(result["elapsed_seconds"], 1.0)
        self.assertEqual(clock.sleeps, [0.75, 0.25])
        self.assertEqual(len(client.calls), 3)

    def test_delayed_setup_at_deadline_cannot_be_accepted_or_finalize(self):
        result, client, _ = self._run(
            setup=Timed(_setup_payload(), 1.0), timeout=1.0
        )
        self.assertFalse(result["success"])
        self.assertEqual(result["terminal_state"], "timeout")
        self.assertEqual(result["attempts"], 0)
        self.assertEqual(len(client.calls), 1)

    def test_delayed_finalize_at_deadline_cannot_report_success(self):
        result, client, _ = self._run(
            finals=[Timed(_complete_payload(), 1.0)], timeout=1.0
        )
        self.assertFalse(result["success"])
        self.assertEqual(result["terminal_state"], "timeout")
        self.assertEqual(result["attempts"], 1)
        self.assertEqual(len(client.calls), 2)

    def test_deadline_reached_before_setup_makes_no_client_call(self):
        clock = ScriptedClock([0.0, 1.0])
        client = FakeClient(clock)
        result = coordinator.run_whitebox_b0r(
            client, timeout_seconds=1.0,
            monotonic=clock.monotonic, sleep=clock.sleep,
        )
        self.assertEqual(result["terminal_state"], "timeout")
        self.assertEqual(client.calls, [])

    def test_invalid_empty_ambiguous_diagnostic_and_nonfinite_stdout_are_rejected(self):
        valid = json.dumps(_setup_payload(), separators=(",", ":"))
        cases = (
            "", "not-json", "[]", "NaN", "Infinity", "-Infinity",
            "diagnostic\n" + valid,
            valid + "\ndiagnostic",
            valid + "\n" + valid,
        )
        for stdout in cases:
            with self.subTest(stdout=stdout):
                result, _, _ = self._run(setup=RawStdout(stdout))
                self.assertEqual(result["terminal_state"], "setup_error")
                self.assertTrue(result["error"])

    def test_wrong_phase_missing_generation_and_bad_task_ids_are_rejected(self):
        wrong_phase = _setup_payload()
        wrong_phase["phase"] = "finalize"
        missing_generation = _setup_payload()
        del missing_generation["generation"]
        missing_kind = _setup_payload()
        del missing_kind["generation"]["foliage"]
        bad_id = _setup_payload()
        bad_id["generation"]["mountains"]["task_id"] = float("nan")
        for payload in (wrong_phase, missing_generation, missing_kind, bad_id):
            with self.subTest(payload=payload):
                result, _, _ = self._run(setup=payload)
                self.assertEqual(result["terminal_state"], "setup_error")

    def test_finalize_wrong_phase_missing_statuses_or_counts_are_rejected(self):
        wrong_phase = _complete_payload()
        wrong_phase["phase"] = "setup"
        missing_statuses = _complete_payload()
        del missing_statuses["statuses"]
        missing_kind = _complete_payload()
        del missing_kind["statuses"]["foliage"]
        missing_counts = _complete_payload()
        del missing_counts["actual_counts"]
        wrong_counts = _complete_payload()
        wrong_counts["actual_counts"]["mountains"] = 23
        bad_status = _complete_payload()
        bad_status["statuses"]["buildings"]["generated"] = False
        for payload in (
            wrong_phase, missing_statuses, missing_kind, missing_counts, wrong_counts, bad_status
        ):
            with self.subTest(payload=payload):
                result, _, _ = self._run(finals=[payload])
                self.assertEqual(result["terminal_state"], "error")

    def test_nonpositive_and_nonfinite_timeout_or_poll_are_rejected(self):
        invalid = (0.0, -1.0, math.inf, -math.inf, math.nan)
        for timeout in invalid:
            with self.subTest(timeout=timeout), self.assertRaises(ValueError):
                coordinator.run_whitebox_b0r(FakeClient(FakeClock()), timeout_seconds=timeout)
        for poll in invalid:
            with self.subTest(poll=poll), self.assertRaises(ValueError):
                coordinator.run_whitebox_b0r(FakeClient(FakeClock()), poll_interval=poll)

    def test_clock_failures_and_backward_time_are_structured(self):
        clocks = (
            ScriptedClock([RuntimeError("clock broke")]),
            ScriptedClock([0.0, float("nan")]),
            ScriptedClock([1.0, 0.5]),
        )
        for clock in clocks:
            with self.subTest(clock=clock):
                client = FakeClient(clock)
                result = coordinator.run_whitebox_b0r(
                    client, monotonic=clock.monotonic, sleep=clock.sleep
                )
                self.assertFalse(result["success"])
                self.assertEqual(result["terminal_state"], "clock_error")
                self.assertEqual(client.calls, [])

    def test_sleep_failure_is_structured(self):
        clock = FakeClock()
        clock.sleep_error = RuntimeError("sleep broke")
        result, _, _ = self._run(clock=clock)
        self.assertEqual(result["terminal_state"], "clock_error")
        self.assertIn("sleep broke", result["error"])

    def test_matches_existing_coordinator_attempts_for_equivalent_success(self):
        old_setup = {"success": True, "generate": {"task_id": 88}}

        class OldClient:
            def __init__(self):
                self.finals = [
                    {"success": True, "complete": False, "pending": True},
                    {"success": True, "complete": True, "pending": False},
                ]

            def run_project_python_file(self, relative_path, argv):
                payload = old_setup if argv[-1] == "setup" else self.finals.pop(0)
                return {"stdout": json.dumps(payload)}

        b0r, _, b0r_clock = self._run(finals=[_pending_payload(), _complete_payload()])
        old_clock = FakeClock()
        old = run_vertical_slice(
            OldClient(), monotonic=old_clock.monotonic, sleep=old_clock.sleep
        )
        self.assertEqual(b0r["terminal_state"], old["terminal_state"])
        self.assertEqual(b0r["attempts"], old["attempts"])
        self.assertEqual(b0r_clock.sleeps, old_clock.sleeps)

    def test_cli_parses_bounded_timeout_and_poll_interval(self):
        args = coordinator._parse_args([
            "--timeout-seconds", "300", "--poll-interval", "0.01"
        ])
        self.assertEqual((args.timeout_seconds, args.poll_interval), (300.0, 0.01))
        for argv in (
            ["--timeout-seconds", "0.999"], ["--timeout-seconds", "301"],
            ["--timeout-seconds", "nan"], ["--poll-interval", "0"],
            ["--poll-interval", "61"], ["--poll-interval", "inf"],
        ):
            with self.subTest(argv=argv), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as raised:
                    coordinator._parse_args(argv)
                self.assertEqual(raised.exception.code, 2)


class MainEnvelopeTests(unittest.TestCase):
    def _main(self, client_class):
        output = io.StringIO()
        fake_module = SimpleNamespace(UnrealMCPClient=client_class)
        with mock.patch.dict(sys.modules, {"scripts.ue_mcp_client": fake_module}):
            with contextlib.redirect_stdout(output):
                code = coordinator.main(["--timeout-seconds", "2"])
        return code, output.getvalue().splitlines()

    def test_client_import_and_connect_exceptions_print_one_json_error(self):
        class ConnectErrorClient:
            def __init__(self, timeout):
                self.endpoint = "fake"

            def connect(self):
                raise RuntimeError("connect exploded")

        for side_effect in (ImportError("import exploded"), None):
            with self.subTest(side_effect=side_effect):
                output = io.StringIO()
                context = (
                    mock.patch.object(coordinator, "_load_client_class", side_effect=side_effect)
                    if side_effect else mock.patch.object(
                        coordinator, "_load_client_class", return_value=ConnectErrorClient
                    )
                )
                with context, contextlib.redirect_stdout(output):
                    code = coordinator.main([])
                lines = output.getvalue().splitlines()
                self.assertEqual(code, 1)
                self.assertEqual(len(lines), 1)
                self.assertEqual(json.loads(lines[0])["terminal_state"], "connection_error")

    def test_coordinator_exception_is_wrapped(self):
        class ConnectedClient:
            endpoint = "fake"

            def __init__(self, timeout):
                pass

            def connect(self):
                return True

        with mock.patch.object(
            coordinator, "run_whitebox_b0r", side_effect=RuntimeError("run exploded")
        ):
            code, lines = self._main(ConnectedClient)
        self.assertEqual(code, 1)
        self.assertEqual(len(lines), 1)
        self.assertIn("run exploded", json.loads(lines[0])["error"])

    def test_serialization_failure_uses_literal_fallback_json(self):
        result = {"success": False, "bad": object()}
        with mock.patch.object(coordinator.json, "dumps", side_effect=TypeError("bad")):
            serialized = coordinator._serialize_result(result)
        self.assertEqual(
            json.loads(serialized),
            {"success": False, "terminal_state": "serialization_error", "error": "result serialization failed"},
        )

    def test_main_serialization_failure_cannot_exit_success(self):
        class ConnectedClient:
            endpoint = "fake"

            def __init__(self, timeout):
                pass

            def connect(self):
                return True

        impossible_result = {"success": True, "bad": object()}
        with mock.patch.object(
            coordinator, "run_whitebox_b0r", return_value=impossible_result
        ):
            code, lines = self._main(ConnectedClient)
        self.assertEqual(code, 1)
        self.assertEqual(json.loads(lines[0])["terminal_state"], "serialization_error")


if __name__ == "__main__":
    unittest.main()
