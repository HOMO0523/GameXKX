"""Bounded host coordinator for Qingshan B0R whitebox setup/finalize phases."""

from __future__ import annotations

import argparse
import json
import math
import time
from typing import Any, Callable


ASSEMBLY_SCRIPT = "Content/Python/gamexxk_assemble_qingshan_whitebox_b0r.py"
DEFAULT_TIMEOUT_SECONDS = 60.0
DEFAULT_POLL_INTERVAL = 0.75
MIN_TIMEOUT_SECONDS = 1.0
MAX_TIMEOUT_SECONDS = 300.0
MIN_POLL_INTERVAL = 0.01
MAX_POLL_INTERVAL = 60.0


def _decode_phase_response(response: dict[str, Any]) -> dict[str, Any]:
    stdout = str(response.get("stdout", "")).strip()
    if not stdout:
        raise RuntimeError(f"assembly phase returned no JSON stdout: {response}")
    try:
        payload = json.loads(stdout.splitlines()[-1])
    except json.JSONDecodeError as error:
        raise RuntimeError(
            f"assembly phase returned invalid JSON stdout: {stdout!r}"
        ) from error
    if not isinstance(payload, dict):
        raise RuntimeError(f"assembly phase JSON must be an object: {payload!r}")
    return payload


def _call_phase(client, phase: str) -> dict[str, Any]:
    response = client.run_project_python_file(ASSEMBLY_SCRIPT, ["--phase", phase])
    return _decode_phase_response(response)


def _result(
    *,
    success: bool,
    terminal_state: str,
    attempts: int,
    elapsed_seconds: float,
    task_id: Any,
    setup: dict[str, Any] | None,
    final: dict[str, Any] | None,
    error: Any,
) -> dict[str, Any]:
    return {
        "success": success,
        "terminal_state": terminal_state,
        "attempts": attempts,
        "elapsed_seconds": elapsed_seconds,
        "task_id": task_id,
        "setup": setup,
        "final": final,
        "error": str(error),
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

    started = monotonic()
    deadline = started + timeout_seconds
    attempts = 0
    final = None

    try:
        setup = _call_phase(client, "setup")
    except Exception as error:
        return _result(
            success=False,
            terminal_state="setup_error",
            attempts=0,
            elapsed_seconds=monotonic() - started,
            task_id=None,
            setup=None,
            final=None,
            error=error,
        )

    generate = setup.get("generate")
    task_id = generate.get("task_id") if isinstance(generate, dict) else None
    if setup.get("success") is not True:
        return _result(
            success=False,
            terminal_state="setup_error",
            attempts=0,
            elapsed_seconds=monotonic() - started,
            task_id=task_id,
            setup=setup,
            final=None,
            error=setup.get("error", "setup failed"),
        )

    while True:
        attempts += 1
        try:
            final = _call_phase(client, "finalize")
        except Exception as error:
            return _result(
                success=False,
                terminal_state="error",
                attempts=attempts,
                elapsed_seconds=monotonic() - started,
                task_id=task_id,
                setup=setup,
                final=final,
                error=error,
            )

        if final.get("success") is True and final.get("complete") is True:
            return _result(
                success=True,
                terminal_state="complete",
                attempts=attempts,
                elapsed_seconds=monotonic() - started,
                task_id=task_id,
                setup=setup,
                final=final,
                error="",
            )
        if final.get("success") is not True or final.get("pending") is not True:
            return _result(
                success=False,
                terminal_state="error",
                attempts=attempts,
                elapsed_seconds=monotonic() - started,
                task_id=task_id,
                setup=setup,
                final=final,
                error=final.get("error", "finalize failed"),
            )

        now = monotonic()
        if now >= deadline:
            return _result(
                success=False,
                terminal_state="timeout",
                attempts=attempts,
                elapsed_seconds=now - started,
                task_id=task_id,
                setup=setup,
                final=final,
                error=f"B0R finalize exceeded {timeout_seconds:.3f}s deadline",
            )
        delay = min(poll_interval, deadline - now)
        if delay > 0.0:
            sleep(delay)


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


def main(argv=None) -> int:
    args = _parse_args(argv)
    client_class = _load_client_class()
    client_timeout = max(60.0, args.timeout_seconds + 15.0)
    try:
        client = client_class(timeout=client_timeout)
        connected = client.connect()
    except Exception as error:
        result = _result(
            success=False,
            terminal_state="connection_error",
            attempts=0,
            elapsed_seconds=0.0,
            task_id=None,
            setup=None,
            final=None,
            error=error,
        )
    else:
        if not connected:
            result = _result(
                success=False,
                terminal_state="connection_error",
                attempts=0,
                elapsed_seconds=0.0,
                task_id=None,
                setup=None,
                final=None,
                error=f"cannot connect to {client.endpoint}",
            )
        else:
            result = run_whitebox_b0r(
                client,
                timeout_seconds=args.timeout_seconds,
                poll_interval=args.poll_interval,
            )
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
