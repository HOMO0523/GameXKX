"""Bounded host coordinator for Qingshan B1 assembly phases."""

from __future__ import annotations

import argparse
import json
import math
import time
from typing import Any, Callable


ASSEMBLY_SCRIPT = "Content/Python/gamexxk_assemble_qingshan_dress_b1.py"
ASSEMBLY_MARKER = "GAMEXXK_QINGSHAN_B1_ASSEMBLY"
MCP_TOOLSET = "gamexxk_mcp_tdd_toolset.GameXXKTDDToolset"
PHASES = ("setup", "infrastructure", "finalize_begin", "finalize_poll")

DEFAULT_TIMEOUT_SECONDS = 120.0
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


def _require_finite_json(value: Any, field: str = "payload") -> None:
    if isinstance(value, float) and not math.isfinite(value):
        raise RuntimeError(f"{field} contains a nonfinite number")
    if isinstance(value, dict):
        for key, item in value.items():
            _require_finite_json(item, f"{field}.{key}")
    elif isinstance(value, list):
        for index, item in enumerate(value):
            _require_finite_json(item, f"{field}[{index}]")


def _decode_json_object(value: Any, context: str) -> dict[str, Any]:
    if isinstance(value, str):
        try:
            value = json.loads(value, parse_constant=_reject_json_constant)
        except (json.JSONDecodeError, ValueError) as error:
            raise RuntimeError(f"{context} returned invalid JSON") from error
    if not isinstance(value, dict):
        raise RuntimeError(f"{context} must be an object")
    _require_finite_json(value, context)
    return value


def _require_bool(payload: dict[str, Any], field: str, context: str) -> bool:
    value = payload.get(field)
    if not isinstance(value, bool):
        raise RuntimeError(f"{context} field {field!r} must be boolean")
    return value


def _validate_outer_response(
    response: Any,
    phase: str,
) -> dict[str, Any]:
    outer = _decode_json_object(response, "outer MCP response")
    if outer.get("success") is not True:
        raise RuntimeError("outer MCP response success must be true")
    if outer.get("relative_path") != ASSEMBLY_SCRIPT:
        raise RuntimeError("outer MCP response relative_path does not match the B1 assembler")
    expected_argv = ["--phase", phase]
    if outer.get("argv") != expected_argv:
        raise RuntimeError(
            f"outer MCP response argv must be {expected_argv!r}"
        )
    if outer.get("run_as_main") is not True:
        raise RuntimeError("outer MCP response run_as_main must be true")
    if not isinstance(outer.get("stdout"), str):
        raise RuntimeError("outer MCP response stdout must be a string")
    return outer


def _decode_marker_payload(stdout: str) -> dict[str, Any]:
    prefix = f"{ASSEMBLY_MARKER} "
    marker_lines = []
    for raw_line in stdout.splitlines():
        line = raw_line.strip()
        if line == ASSEMBLY_MARKER or line.startswith(prefix):
            marker_lines.append(line)
    if len(marker_lines) != 1:
        raise RuntimeError(
            f"assembly stdout must contain exactly one {ASSEMBLY_MARKER} marker, "
            f"got {len(marker_lines)}"
        )
    marker_line = marker_lines[0]
    if not marker_line.startswith(prefix):
        raise RuntimeError("assembly marker must be followed by a JSON object")
    encoded = marker_line[len(prefix):]
    try:
        payload = json.loads(encoded, parse_constant=_reject_json_constant)
    except (json.JSONDecodeError, ValueError) as error:
        raise RuntimeError("assembly marker returned invalid JSON") from error
    if not isinstance(payload, dict):
        raise RuntimeError("assembly marker JSON must be an object")
    _require_finite_json(payload, "assembly marker payload")
    return payload


def _validate_phase_payload(payload: dict[str, Any], phase: str) -> None:
    if phase not in PHASES:
        raise ValueError(f"unsupported phase: {phase!r}")
    context = f"{phase} response"
    success = _require_bool(payload, "success", context)
    complete = _require_bool(payload, "complete", context)
    pending = _require_bool(payload, "pending", context)
    if payload.get("phase") != phase:
        raise RuntimeError(f"{context} phase must be {phase!r}")
    state = payload.get("state")
    if state not in {"pending", "complete", "failed"}:
        raise RuntimeError(f"{context} state must be pending, complete, or failed")
    if not isinstance(payload.get("error"), str):
        raise RuntimeError(f"{context} error must be a string")

    expected_flags = {
        "pending": (True, False, True),
        "complete": (True, True, False),
        "failed": (False, False, False),
    }
    if (success, complete, pending) != expected_flags[state]:
        raise RuntimeError(
            f"{context} has inconsistent success/complete/pending flags for {state!r}"
        )
    if state == "failed" and not payload["error"].strip():
        raise RuntimeError(f"{context} failed state requires a nonblank error")
    if state != "failed" and payload["error"]:
        raise RuntimeError(f"{context} successful state requires an empty error")

    allowed_states = {
        "setup": {"complete", "failed"},
        "infrastructure": {"complete", "failed"},
        "finalize_begin": {"pending", "failed"},
        "finalize_poll": {"pending", "complete", "failed"},
    }[phase]
    if state not in allowed_states:
        raise RuntimeError(f"{context} may not report state {state!r}")


def _call_phase(client, phase: str, timeout_seconds: float) -> dict[str, Any]:
    if phase not in PHASES:
        raise ValueError(f"unsupported phase: {phase!r}")
    argv = ["--phase", phase]
    response = client.call_tool(
        "run_project_python_file",
        {
            "relative_path": ASSEMBLY_SCRIPT,
            "argv_json": json.dumps(argv, separators=(",", ":")),
            "run_as_main": True,
        },
        toolset_name=MCP_TOOLSET,
        timeout=max(MIN_CALL_TIMEOUT_SECONDS, float(timeout_seconds)),
    )
    outer = _validate_outer_response(response, phase)
    payload = _decode_marker_payload(outer["stdout"])
    _validate_phase_payload(payload, phase)
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
    poll_attempts: int = 0,
    elapsed_seconds: float = 0.0,
    setup: dict[str, Any] | None = None,
    infrastructure: dict[str, Any] | None = None,
    finalize_begin: dict[str, Any] | None = None,
    final: dict[str, Any] | None = None,
    error: Any = "",
) -> dict[str, Any]:
    return {
        "success": bool(success),
        "terminal_state": terminal_state,
        "poll_attempts": int(poll_attempts),
        "elapsed_seconds": float(elapsed_seconds),
        "setup": setup,
        "infrastructure": infrastructure,
        "finalize_begin": finalize_begin,
        "final": final,
        "error": _safe_error(error),
    }


def run_dress_b1(
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
            success=False,
            terminal_state="clock_error",
            error="monotonic deadline is nonfinite",
        )

    setup = None
    infrastructure = None
    finalize_begin = None
    final = None
    poll_attempts = 0

    def current_result(
        success: bool,
        terminal_state: str,
        *,
        error: Any = "",
        elapsed_seconds: float | None = None,
    ) -> dict[str, Any]:
        return _result(
            success=success,
            terminal_state=terminal_state,
            poll_attempts=poll_attempts,
            elapsed_seconds=(
                clock.elapsed() if elapsed_seconds is None else elapsed_seconds
            ),
            setup=setup,
            infrastructure=infrastructure,
            finalize_begin=finalize_begin,
            final=final,
            error=error,
        )

    def read_clock() -> tuple[float | None, dict[str, Any] | None]:
        try:
            return clock.read(), None
        except _ClockError as error:
            return None, current_result(False, "clock_error", error=error)

    def timeout_result(now: float) -> dict[str, Any]:
        return current_result(
            False,
            "timeout",
            elapsed_seconds=now - started,
            error=f"B1 assembly exceeded {timeout_seconds:.3f}s deadline",
        )

    def invoke(phase: str, error_state: str, *, count_poll: bool = False):
        nonlocal poll_attempts
        now, clock_failure = read_clock()
        if clock_failure is not None:
            return None, clock_failure
        assert now is not None
        if now >= deadline:
            return None, timeout_result(now)
        if count_poll:
            poll_attempts += 1
        try:
            payload = _call_phase(client, phase, deadline - now)
        except Exception as error:
            after, after_failure = read_clock()
            if after_failure is not None:
                return None, after_failure
            assert after is not None
            if after >= deadline:
                return None, timeout_result(after)
            return None, current_result(False, error_state, error=error)
        after, after_failure = read_clock()
        if after_failure is not None:
            return None, after_failure
        assert after is not None
        if after >= deadline:
            return None, timeout_result(after)
        return payload, None

    setup, terminal = invoke("setup", "setup_error")
    if terminal is not None:
        return terminal
    if setup["success"] is not True:
        return current_result(False, "setup_error", error=setup["error"])

    infrastructure, terminal = invoke("infrastructure", "infrastructure_error")
    if terminal is not None:
        return terminal
    if infrastructure["success"] is not True:
        return current_result(
            False, "infrastructure_error", error=infrastructure["error"]
        )

    finalize_begin, terminal = invoke("finalize_begin", "finalize_begin_error")
    if terminal is not None:
        return terminal
    if finalize_begin["success"] is not True:
        return current_result(
            False, "finalize_begin_error", error=finalize_begin["error"]
        )

    while True:
        final, terminal = invoke("finalize_poll", "error", count_poll=True)
        if terminal is not None:
            return terminal
        if final["success"] is not True:
            return current_result(False, "error", error=final["error"])
        if final["state"] == "complete":
            return current_result(True, "complete")

        now, clock_failure = read_clock()
        if clock_failure is not None:
            return clock_failure
        assert now is not None
        if now >= deadline:
            return timeout_result(now)
        delay = min(poll_interval, deadline - now)
        if delay <= 0.0:
            return timeout_result(now)
        try:
            sleep(delay)
        except Exception as error:
            return current_result(
                False,
                "clock_error",
                error=f"sleep failed: {_safe_error(error)}",
            )


def _parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assemble-once", action="store_true")
    parser.add_argument("--timeout-seconds", type=float, default=DEFAULT_TIMEOUT_SECONDS)
    parser.add_argument("--poll-interval", type=float, default=DEFAULT_POLL_INTERVAL)
    args = parser.parse_args(argv)
    if not args.assemble_once:
        parser.error("--assemble-once is required")
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
            result,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
            allow_nan=False,
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
            success=False,
            terminal_state="connection_error",
            error=error,
        )
    else:
        if not connected:
            result = _result(
                success=False,
                terminal_state="connection_error",
                error=f"cannot connect to {client.endpoint}",
            )
        else:
            try:
                result = run_dress_b1(
                    client,
                    timeout_seconds=args.timeout_seconds,
                    poll_interval=args.poll_interval,
                )
            except Exception as error:
                result = _result(success=False, terminal_state="error", error=error)
    serialized = _serialize_result(result)
    print(serialized)
    if serialized == SERIALIZATION_FALLBACK:
        return 1
    return 0 if result.get("success") is True else 1


if __name__ == "__main__":
    raise SystemExit(main())
