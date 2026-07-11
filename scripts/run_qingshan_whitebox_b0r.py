"""Bounded host coordinator for Qingshan B0R whitebox setup/finalize phases."""

from __future__ import annotations

import argparse
import json
import math
import time
from typing import Any, Callable


ASSEMBLY_SCRIPT = "Content/Python/gamexxk_assemble_qingshan_whitebox_b0r.py"
MCP_TOOLSET = "gamexxk_mcp_tdd_toolset.GameXXKTDDToolset"
KINDS = ("buildings", "foliage", "mountains")
EXPECTED_COUNTS = {"buildings": 16, "foliage": 100, "mountains": 24}
DEFAULT_TIMEOUT_SECONDS = 60.0
DEFAULT_POLL_INTERVAL = 0.75
MIN_TIMEOUT_SECONDS = 1.0
MAX_TIMEOUT_SECONDS = 300.0
MIN_POLL_INTERVAL = 0.01
MAX_POLL_INTERVAL = 60.0
MIN_CALL_TIMEOUT_SECONDS = 0.001
SERIALIZATION_FALLBACK = (
    '{"error":"result serialization failed","success":false,'
    '"terminal_state":"serialization_error"}'
)


class _ClockError(RuntimeError):
    pass


class _ClockTracker:
    def __init__(self, monotonic: Callable[[], float]):
        self._monotonic = monotonic
        self.started: float | None = None
        self.last: float | None = None

    def read(self) -> float:
        try:
            value = float(self._monotonic())
        except Exception as error:
            raise _ClockError(f"monotonic clock failed: {error}") from error
        if not math.isfinite(value):
            raise _ClockError(f"monotonic clock returned nonfinite value: {value!r}")
        if self.last is not None and value < self.last:
            raise _ClockError(
                f"monotonic clock moved backward from {self.last!r} to {value!r}"
            )
        if self.started is None:
            self.started = value
        self.last = value
        return value

    def elapsed(self) -> float:
        if self.started is None or self.last is None:
            return 0.0
        return self.last - self.started


def _reject_json_constant(value: str):
    raise ValueError(f"nonfinite JSON constant is forbidden: {value}")


def _decode_phase_response(response: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(response, dict):
        raise RuntimeError(f"assembly phase response must be an object: {response!r}")
    stdout = response.get("stdout")
    if not isinstance(stdout, str) or not stdout.strip():
        raise RuntimeError(f"assembly phase returned no JSON stdout: {response}")
    lines = [line.strip() for line in stdout.splitlines() if line.strip()]
    if len(lines) != 1:
        raise RuntimeError(
            f"assembly phase must return exactly one nonblank JSON line, got {len(lines)}"
        )
    try:
        payload = json.loads(lines[0], parse_constant=_reject_json_constant)
    except (json.JSONDecodeError, ValueError) as error:
        raise RuntimeError(
            f"assembly phase returned invalid JSON stdout: {lines[0]!r}"
        ) from error
    if not isinstance(payload, dict):
        raise RuntimeError(f"assembly phase JSON must be an object: {payload!r}")
    return payload


def _require_bool(payload: dict[str, Any], field: str, context: str) -> bool:
    value = payload.get(field)
    if not isinstance(value, bool):
        raise RuntimeError(f"{context} field {field!r} must be boolean")
    return value


def _require_exact_kind_mapping(payload: Any, field: str) -> dict[str, Any]:
    if not isinstance(payload, dict):
        raise RuntimeError(f"{field} must be an object")
    if set(payload) != set(KINDS):
        raise RuntimeError(f"{field} must contain exactly {KINDS!r}")
    return payload


def _validate_setup_payload(payload: dict[str, Any]) -> dict[str, int]:
    if payload.get("phase") != "setup":
        raise RuntimeError("setup response phase must be 'setup'")
    success = _require_bool(payload, "success", "setup response")
    if not success:
        return {}
    generation = _require_exact_kind_mapping(payload.get("generation"), "generation")
    task_ids: dict[str, int] = {}
    for kind in KINDS:
        entry = generation[kind]
        if not isinstance(entry, dict):
            raise RuntimeError(f"generation.{kind} must be an object")
        if entry.get("success") is not True:
            raise RuntimeError(f"generation.{kind}.success must be true")
        task_id = entry.get("task_id")
        if (
            isinstance(task_id, bool)
            or not isinstance(task_id, (int, float))
            or not math.isfinite(float(task_id))
            or float(task_id) < 0.0
            or not float(task_id).is_integer()
        ):
            raise RuntimeError(f"generation.{kind}.task_id must be a finite nonnegative integer")
        task_ids[kind] = int(task_id)
    return task_ids


def _validate_finalize_payload(payload: dict[str, Any]) -> None:
    if payload.get("phase") != "finalize":
        raise RuntimeError("finalize response phase must be 'finalize'")
    success = _require_bool(payload, "success", "finalize response")
    complete = _require_bool(payload, "complete", "finalize response")
    pending = _require_bool(payload, "pending", "finalize response")
    statuses = _require_exact_kind_mapping(payload.get("statuses"), "statuses")
    for kind in KINDS:
        status = statuses[kind]
        if not isinstance(status, dict):
            raise RuntimeError(f"statuses.{kind} must be an object")
        status_success = _require_bool(status, "success", f"statuses.{kind}")
        generating = _require_bool(status, "generating", f"statuses.{kind}")
        generated = _require_bool(status, "generated", f"statuses.{kind}")
        if success and not status_success:
            raise RuntimeError(f"statuses.{kind}.success must be true")
        if success and pending and not (generating or generated):
            raise RuntimeError(f"statuses.{kind} must be generating or generated while pending")
        if success and complete and (generating or not generated):
            raise RuntimeError(f"statuses.{kind} must be terminally generated when complete")
    if success and complete:
        if pending:
            raise RuntimeError("complete finalize response cannot also be pending")
        actual_counts = _require_exact_kind_mapping(
            payload.get("actual_counts"), "actual_counts"
        )
        for kind, expected in EXPECTED_COUNTS.items():
            value = actual_counts[kind]
            if isinstance(value, bool) or value != expected:
                raise RuntimeError(
                    f"actual_counts.{kind} must be {expected}, got {value!r}"
                )
    elif success and pending:
        if complete:
            raise RuntimeError("pending finalize response cannot be complete")
    elif success:
        raise RuntimeError("successful finalize response must be pending or complete")


def _call_phase(client, phase: str, timeout_seconds: float) -> dict[str, Any]:
    if phase not in ("setup", "finalize"):
        raise ValueError(f"unsupported phase: {phase!r}")
    response = client.call_tool(
        "run_project_python_file",
        {
            "relative_path": ASSEMBLY_SCRIPT,
            "argv_json": json.dumps(["--phase", phase], separators=(",", ":")),
            "run_as_main": True,
        },
        toolset_name=MCP_TOOLSET,
        timeout=max(MIN_CALL_TIMEOUT_SECONDS, float(timeout_seconds)),
    )
    if isinstance(response, str):
        try:
            response = json.loads(response, parse_constant=_reject_json_constant)
        except (json.JSONDecodeError, ValueError) as error:
            raise RuntimeError("MCP run_project_python_file returned invalid JSON") from error
    payload = _decode_phase_response(response)
    if phase == "setup":
        _validate_setup_payload(payload)
    else:
        _validate_finalize_payload(payload)
    return payload


def _safe_error(error: Any) -> str:
    try:
        return str(error)
    except Exception:
        return type(error).__name__


def _result(
    *,
    success: bool,
    terminal_state: str,
    attempts: int = 0,
    elapsed_seconds: float = 0.0,
    task_ids: dict[str, int] | None = None,
    setup: dict[str, Any] | None = None,
    final: dict[str, Any] | None = None,
    error: Any = "",
) -> dict[str, Any]:
    return {
        "success": bool(success),
        "terminal_state": terminal_state,
        "attempts": int(attempts),
        "elapsed_seconds": float(elapsed_seconds),
        "task_ids": dict(task_ids or {}),
        "setup": setup,
        "final": final,
        "error": _safe_error(error),
    }


def run_whitebox_b0r(
    client,
    *,
    timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS,
    poll_interval: float = DEFAULT_POLL_INTERVAL,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> dict[str, Any]:
    try:
        timeout_seconds = float(timeout_seconds)
        poll_interval = float(poll_interval)
    except (TypeError, ValueError) as error:
        raise ValueError("timeout_seconds and poll_interval must be finite and positive") from error
    if (
        not math.isfinite(timeout_seconds)
        or not math.isfinite(poll_interval)
        or timeout_seconds <= 0.0
        or poll_interval <= 0.0
    ):
        raise ValueError("timeout_seconds and poll_interval must be finite and positive")

    clock = _ClockTracker(monotonic)
    try:
        started = clock.read()
    except _ClockError as error:
        return _result(success=False, terminal_state="clock_error", error=error)
    deadline = started + timeout_seconds
    if not math.isfinite(deadline):
        return _result(
            success=False, terminal_state="clock_error",
            error="monotonic deadline is nonfinite",
        )

    setup = None
    final = None
    task_ids: dict[str, int] = {}
    attempts = 0

    def clock_error_result(error):
        return _result(
            success=False, terminal_state="clock_error", attempts=attempts,
            elapsed_seconds=clock.elapsed(), task_ids=task_ids,
            setup=setup, final=final, error=error,
        )

    def timeout_result(now):
        return _result(
            success=False, terminal_state="timeout", attempts=attempts,
            elapsed_seconds=now - started, task_ids=task_ids,
            setup=setup, final=final,
            error=f"B0R finalize exceeded {timeout_seconds:.3f}s deadline",
        )

    try:
        now = clock.read()
    except _ClockError as error:
        return clock_error_result(error)
    if now >= deadline:
        return timeout_result(now)
    try:
        setup = _call_phase(client, "setup", deadline - now)
    except Exception as error:
        try:
            now = clock.read()
        except _ClockError as clock_error:
            return clock_error_result(clock_error)
        if now >= deadline:
            return timeout_result(now)
        return _result(
            success=False, terminal_state="setup_error", attempts=0,
            elapsed_seconds=now - started, task_ids={}, setup=None, final=None, error=error,
        )
    try:
        now = clock.read()
    except _ClockError as error:
        return clock_error_result(error)
    if now >= deadline:
        return timeout_result(now)
    if setup.get("success") is not True:
        return _result(
            success=False, terminal_state="setup_error", attempts=0,
            elapsed_seconds=now - started, task_ids={}, setup=setup,
            error=setup.get("error", "setup failed"),
        )
    task_ids = _validate_setup_payload(setup)

    while True:
        try:
            now = clock.read()
        except _ClockError as error:
            return clock_error_result(error)
        if now >= deadline:
            return timeout_result(now)
        attempts += 1
        try:
            final = _call_phase(client, "finalize", deadline - now)
        except Exception as error:
            try:
                now = clock.read()
            except _ClockError as clock_error:
                return clock_error_result(clock_error)
            if now >= deadline:
                return timeout_result(now)
            return _result(
                success=False, terminal_state="error", attempts=attempts,
                elapsed_seconds=now - started, task_ids=task_ids,
                setup=setup, final=final, error=error,
            )
        try:
            now = clock.read()
        except _ClockError as error:
            return clock_error_result(error)
        if now >= deadline:
            return timeout_result(now)
        if final.get("success") is True and final.get("complete") is True:
            return _result(
                success=True, terminal_state="complete", attempts=attempts,
                elapsed_seconds=now - started, task_ids=task_ids,
                setup=setup, final=final,
            )
        if final.get("success") is not True or final.get("pending") is not True:
            return _result(
                success=False, terminal_state="error", attempts=attempts,
                elapsed_seconds=now - started, task_ids=task_ids,
                setup=setup, final=final,
                error=final.get("error", "finalize failed"),
            )
        delay = min(poll_interval, deadline - now)
        if delay <= 0.0:
            return timeout_result(now)
        try:
            sleep(delay)
        except Exception as error:
            return clock_error_result(f"sleep failed: {_safe_error(error)}")


def _parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeout-seconds", type=float, default=DEFAULT_TIMEOUT_SECONDS)
    parser.add_argument("--poll-interval", type=float, default=DEFAULT_POLL_INTERVAL)
    args = parser.parse_args(argv)
    if not (
        math.isfinite(args.timeout_seconds)
        and MIN_TIMEOUT_SECONDS <= args.timeout_seconds <= MAX_TIMEOUT_SECONDS
    ):
        parser.error(
            f"--timeout-seconds must be between {MIN_TIMEOUT_SECONDS:g} "
            f"and {MAX_TIMEOUT_SECONDS:g}"
        )
    if not (
        math.isfinite(args.poll_interval)
        and MIN_POLL_INTERVAL <= args.poll_interval <= MAX_POLL_INTERVAL
    ):
        parser.error(
            f"--poll-interval must be between {MIN_POLL_INTERVAL:g} "
            f"and {MAX_POLL_INTERVAL:g}"
        )
    return args


def _load_client_class():
    try:
        from scripts.ue_mcp_client import UnrealMCPClient
    except ModuleNotFoundError:
        from ue_mcp_client import UnrealMCPClient
    return UnrealMCPClient


def _serialize_result(result: dict[str, Any]) -> str:
    try:
        return json.dumps(
            result, ensure_ascii=False, sort_keys=True,
            separators=(",", ":"), allow_nan=False,
        )
    except Exception:
        return SERIALIZATION_FALLBACK


def main(argv=None) -> int:
    args = _parse_args(argv)
    try:
        client_class = _load_client_class()
        client = client_class(timeout=max(1.0, min(60.0, args.timeout_seconds)))
        connected = client.connect()
    except Exception as error:
        result = _result(
            success=False, terminal_state="connection_error", error=error,
        )
    else:
        if not connected:
            result = _result(
                success=False, terminal_state="connection_error",
                error=f"cannot connect to {client.endpoint}",
            )
        else:
            try:
                result = run_whitebox_b0r(
                    client,
                    timeout_seconds=args.timeout_seconds,
                    poll_interval=args.poll_interval,
                )
            except Exception as error:
                result = _result(
                    success=False, terminal_state="error", error=error,
                )
    serialized = _serialize_result(result)
    print(serialized)
    if serialized == SERIALIZATION_FALLBACK:
        return 1
    try:
        return 0 if result.get("success") is True else 1
    except Exception:
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
