"""Bounded host-side coordinator for Qingshan town PCG setup/finalize phases."""

from __future__ import annotations

import argparse
import json
import time
from typing import Any, Callable

try:
    from scripts.ue_mcp_client import UnrealMCPClient
except ModuleNotFoundError:
    from ue_mcp_client import UnrealMCPClient


ASSEMBLY_SCRIPT = "Content/Python/gamexxk_assemble_qingshan_town_pcg_vertical_slice.py"
DEFAULT_TIMEOUT_SECONDS = 30.0
DEFAULT_POLL_INTERVAL = 0.75
MIN_TIMEOUT_SECONDS = 1.0
MAX_TIMEOUT_SECONDS = 300.0


def _decode_phase_response(response: dict[str, Any]) -> dict[str, Any]:
    stdout = str(response.get("stdout", "")).strip()
    if not stdout:
        raise RuntimeError(f"assembly phase returned no JSON stdout: {response}")
    try:
        payload = json.loads(stdout.splitlines()[-1])
    except json.JSONDecodeError as error:
        raise RuntimeError(f"assembly phase returned invalid JSON stdout: {stdout!r}") from error
    if not isinstance(payload, dict):
        raise RuntimeError(f"assembly phase JSON must be an object: {payload!r}")
    return payload


def _call_phase(client, phase: str) -> dict[str, Any]:
    response = client.run_project_python_file(ASSEMBLY_SCRIPT, ["--phase", phase])
    return _decode_phase_response(response)


def run_vertical_slice(
    client,
    *,
    timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS,
    poll_interval: float = DEFAULT_POLL_INTERVAL,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> dict[str, Any]:
    if timeout_seconds <= 0.0 or poll_interval <= 0.0:
        raise ValueError("timeout_seconds and poll_interval must be positive")
    started = monotonic()
    deadline = started + float(timeout_seconds)
    attempts = 0
    final = None

    try:
        setup = _call_phase(client, "setup")
    except Exception as error:
        return {
            "success": False,
            "terminal_state": "setup_error",
            "attempts": 0,
            "elapsed_seconds": monotonic() - started,
            "task_id": None,
            "setup": None,
            "final": None,
            "error": str(error),
        }
    task_id = setup.get("generate", {}).get("task_id")
    if setup.get("success") is not True:
        return {
            "success": False,
            "terminal_state": "setup_error",
            "attempts": 0,
            "elapsed_seconds": monotonic() - started,
            "task_id": task_id,
            "setup": setup,
            "final": None,
            "error": setup.get("error", "setup failed"),
        }

    while True:
        attempts += 1
        try:
            final = _call_phase(client, "finalize")
        except Exception as error:
            return {
                "success": False,
                "terminal_state": "error",
                "attempts": attempts,
                "elapsed_seconds": monotonic() - started,
                "task_id": task_id,
                "setup": setup,
                "final": final,
                "error": str(error),
            }
        if final.get("success") is True and final.get("complete") is True:
            return {
                "success": True,
                "terminal_state": "complete",
                "attempts": attempts,
                "elapsed_seconds": monotonic() - started,
                "task_id": task_id,
                "setup": setup,
                "final": final,
                "error": "",
            }
        if final.get("success") is not True or final.get("pending") is not True:
            return {
                "success": False,
                "terminal_state": "error",
                "attempts": attempts,
                "elapsed_seconds": monotonic() - started,
                "task_id": task_id,
                "setup": setup,
                "final": final,
                "error": final.get("error", "finalize failed"),
            }

        now = monotonic()
        if now >= deadline:
            return {
                "success": False,
                "terminal_state": "timeout",
                "attempts": attempts,
                "elapsed_seconds": now - started,
                "task_id": task_id,
                "setup": setup,
                "final": final,
                "error": f"PCG finalize exceeded {timeout_seconds:.3f}s deadline",
            }
        sleep(min(float(poll_interval), deadline - now))


def _parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--timeout-seconds", type=float, default=DEFAULT_TIMEOUT_SECONDS)
    args = parser.parse_args(argv)
    if not MIN_TIMEOUT_SECONDS <= args.timeout_seconds <= MAX_TIMEOUT_SECONDS:
        parser.error(
            f"--timeout-seconds must be between {MIN_TIMEOUT_SECONDS:g} and {MAX_TIMEOUT_SECONDS:g}"
        )
    return args


def main(argv=None) -> int:
    args = _parse_args(argv)
    client = UnrealMCPClient(timeout=max(60.0, args.timeout_seconds + 15.0))
    if not client.connect():
        result = {
            "success": False,
            "terminal_state": "connection_error",
            "attempts": 0,
            "elapsed_seconds": 0.0,
            "task_id": None,
            "setup": None,
            "final": None,
            "error": f"cannot connect to {client.endpoint}",
        }
    else:
        result = run_vertical_slice(client, timeout_seconds=args.timeout_seconds)
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return 0 if result["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
