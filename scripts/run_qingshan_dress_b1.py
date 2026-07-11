"""Bounded host coordinator for Qingshan B1 assembly phases."""

from __future__ import annotations

import argparse
import contextlib
import ctypes
from ctypes import wintypes
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import statistics
import time
from typing import Any, Callable
import zlib


ASSEMBLY_SCRIPT = "Content/Python/gamexxk_assemble_qingshan_dress_b1.py"
ASSEMBLY_MARKER = "GAMEXXK_QINGSHAN_B1_ASSEMBLY"
VALIDATOR_SCRIPT = "Content/Python/gamexxk_validate_qingshan_dress_b1.py"
VALIDATION_MARKER = "GAMEXXK_QINGSHAN_B1_VALIDATION"
ACCEPTANCE_SCRIPT = "Content/Python/gamexxk_qingshan_dress_b1_acceptance.py"
ACCEPTANCE_MARKER = "GAMEXXK_QINGSHAN_B1_ACCEPTANCE"
MCP_TOOLSET = "gamexxk_mcp_tdd_toolset.GameXXKTDDToolset"
PHASES = ("setup", "infrastructure", "finalize_begin", "finalize_poll")

PROJECT_ROOT = Path(__file__).resolve().parents[1]
B1_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1"
EVIDENCE_ROOT = (
    PROJECT_ROOT / "docs" / "production" / "evidence" / "qingshan-pcg-dress-b1"
).resolve()
CSV_PROFILE_ROOT = (PROJECT_ROOT / "Saved" / "Profiling" / "CSV").resolve()
CAMERA_LABELS = (
    "CAM_QS_B1_GATE_ARRIVAL",
    "CAM_QS_B1_TOWN_CORE",
    "CAM_QS_B1_MAIN_BRIDGE",
    "CAM_QS_B1_SOUTH_DOCK",
)
CAPTURE_FILENAMES = {
    "CAM_QS_B1_GATE_ARRIVAL": "camera-gate-arrival.png",
    "CAM_QS_B1_TOWN_CORE": "camera-town-core.png",
    "CAM_QS_B1_MAIN_BRIDGE": "camera-main-bridge.png",
    "CAM_QS_B1_SOUTH_DOCK": "camera-south-dock.png",
}
PERFORMANCE_CSV_FILENAME = "qingshan-b1-performance.csv"
FOREGROUND_PERFORMANCE_CSV_FILENAME = "qingshan-b1-performance-foreground.csv"
FORMAL_ACCEPTANCE_FILENAMES = (
    "live-manifest-run-1.json",
    "live-manifest-run-2.json",
    "captures.json",
    *(CAPTURE_FILENAMES[label] for label in CAMERA_LABELS),
    "performance.json",
    "acceptance-summary.json",
    PERFORMANCE_CSV_FILENAME,
    "performance-foreground.json",
    "acceptance-summary-final.json",
    FOREGROUND_PERFORMANCE_CSV_FILENAME,
)
CAPTURE_WIDTH = 1920
CAPTURE_HEIGHT = 1080
CAPTURE_TIMEOUT_SECONDS = 45.0
PERFORMANCE_DURATION_SECONDS = 5.0
PERFORMANCE_SAMPLE_INTERVAL = 0.5
FOREGROUND_FRAME_TIME_LIMIT_MS = 100.0
_SHA256 = re.compile(r"^[0-9a-f]{64}$")

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


class _Win32ForegroundBackend:
    SW_RESTORE = 9

    def __init__(self):
        if not hasattr(ctypes, "WinDLL") or not hasattr(ctypes, "WINFUNCTYPE"):
            raise RuntimeError("B1 review captures require Win32 foreground control")
        self.user32 = ctypes.WinDLL("user32", use_last_error=True)
        self.user32.GetForegroundWindow.argtypes = []
        self.user32.GetForegroundWindow.restype = wintypes.HWND
        self.user32.IsWindow.argtypes = [wintypes.HWND]
        self.user32.IsWindow.restype = wintypes.BOOL
        self.user32.IsWindowVisible.argtypes = [wintypes.HWND]
        self.user32.IsWindowVisible.restype = wintypes.BOOL
        self.user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
        self.user32.ShowWindow.restype = wintypes.BOOL
        self.user32.SetForegroundWindow.argtypes = [wintypes.HWND]
        self.user32.SetForegroundWindow.restype = wintypes.BOOL
        self.user32.GetWindowTextLengthW.argtypes = [wintypes.HWND]
        self.user32.GetWindowTextLengthW.restype = ctypes.c_int
        self.user32.GetWindowTextW.argtypes = [
            wintypes.HWND, wintypes.LPWSTR, ctypes.c_int,
        ]
        self.user32.GetWindowTextW.restype = ctypes.c_int
        self._enum_callback_type = ctypes.WINFUNCTYPE(
            wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
        self.user32.EnumWindows.argtypes = [self._enum_callback_type, wintypes.LPARAM]
        self.user32.EnumWindows.restype = wintypes.BOOL

    def get_foreground_window(self) -> int:
        return int(self.user32.GetForegroundWindow() or 0)

    def visible_unreal_editor_windows(self) -> list[int]:
        matches: list[int] = []

        @self._enum_callback_type
        def visit(handle, _parameter):
            if not bool(self.user32.IsWindowVisible(handle)):
                return True
            length = int(self.user32.GetWindowTextLengthW(handle))
            if length <= 0:
                return True
            buffer = ctypes.create_unicode_buffer(length + 1)
            self.user32.GetWindowTextW(handle, buffer, length + 1)
            if buffer.value.endswith(" - Unreal Editor"):
                matches.append(int(handle))
            return True

        if not bool(self.user32.EnumWindows(visit, 0)):
            raise RuntimeError("could not enumerate Win32 windows for B1 capture")
        return matches

    def restore_window(self, handle: int) -> bool:
        if not self.is_window(handle):
            return False
        self.user32.ShowWindow(wintypes.HWND(handle), self.SW_RESTORE)
        return self.is_window(handle)

    def set_foreground_window(self, handle: int) -> bool:
        return bool(self.user32.SetForegroundWindow(wintypes.HWND(handle)))

    def is_window(self, handle: int) -> bool:
        return bool(handle and self.user32.IsWindow(wintypes.HWND(handle)))


def _wait_for_foreground_window(
        backend, expected_handle: int, *, description: str,
        monotonic=time.monotonic, sleep=time.sleep,
        timeout_seconds: float = 1.0, poll_interval: float = 0.05) -> None:
    deadline = float(monotonic()) + float(timeout_seconds)
    while int(backend.get_foreground_window() or 0) != int(expected_handle):
        now = float(monotonic())
        if now >= deadline:
            raise RuntimeError(f"{description} did not become foreground within 1 second")
        sleep(min(float(poll_interval), deadline - now))


def _activate_foreground_window(
        backend, handle: int, *, description: str, restore_first: bool,
        monotonic=time.monotonic, sleep=time.sleep,
        timeout_seconds: float = 1.0, poll_interval: float = 0.05) -> None:
    if restore_first and not backend.restore_window(handle):
        raise RuntimeError(f"could not restore {description}")
    deadline = float(monotonic()) + float(timeout_seconds)
    attempts = 0
    while int(backend.get_foreground_window() or 0) != int(handle):
        attempts += 1
        # SetForegroundWindow can transiently return false even for the
        # previously focused valid window.  Retry within the same bounded
        # one-second gate, and prove success by reading the actual foreground.
        backend.set_foreground_window(handle)
        if int(backend.get_foreground_window() or 0) == int(handle):
            return
        now = float(monotonic())
        if now >= deadline or attempts >= 20:
            raise RuntimeError(f"{description} did not become foreground within 1 second")
        sleep(min(float(poll_interval), deadline - now))


@contextlib.contextmanager
def _foreground_unreal_editor_window(
        backend=None, *, monotonic=time.monotonic, sleep=time.sleep):
    """Temporarily foreground the sole UE main window so its viewport actually draws."""

    backend = backend or _Win32ForegroundBackend()
    previous = int(backend.get_foreground_window() or 0)
    matches = [int(handle) for handle in backend.visible_unreal_editor_windows()]
    if len(matches) != 1:
        raise RuntimeError(
            f"B1 capture requires exactly one visible Unreal Editor window, found {matches}"
        )
    target = matches[0]
    primary_error = None
    try:
        _activate_foreground_window(
            backend, target, restore_first=True,
            description="Unreal Editor capture window",
            monotonic=monotonic, sleep=sleep,
        )
        yield target
    except BaseException as error:
        primary_error = error
        raise
    finally:
        if previous and previous != target and backend.is_window(previous):
            try:
                _activate_foreground_window(
                    backend, previous, restore_first=False,
                    description="previous foreground window",
                    monotonic=monotonic, sleep=sleep,
                )
            except Exception as restore_error:
                if primary_error is None:
                    raise
                add_note = getattr(primary_error, "add_note", None)
                if callable(add_note):
                    add_note(f"foreground restoration also failed: {restore_error}")


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


def _decode_unique_marker(stdout: str, marker: str) -> dict[str, Any]:
    if not isinstance(stdout, str):
        raise RuntimeError(f"{marker} outer stdout must be a string")
    prefix = f"{marker} "
    lines = []
    for raw_line in stdout.splitlines():
        line = raw_line.strip()
        if line == marker or line.startswith(prefix):
            lines.append(line)
    if len(lines) != 1 or not lines[0].startswith(prefix):
        raise RuntimeError(f"stdout must contain exactly one JSON {marker} marker")
    try:
        payload = json.loads(
            lines[0][len(prefix):],
            parse_constant=_reject_json_constant,
        )
    except (json.JSONDecodeError, ValueError) as error:
        raise RuntimeError(f"{marker} returned invalid JSON") from error
    if not isinstance(payload, dict):
        raise RuntimeError(f"{marker} payload must be an object")
    _require_finite_json(payload, f"{marker} payload")
    return payload


def _call_marked_project_script(
    client,
    *,
    relative_path: str,
    argv: list[str],
    marker: str,
    timeout_seconds: float,
) -> dict[str, Any]:
    response = client.call_tool(
        "run_project_python_file",
        {
            "relative_path": relative_path,
            "argv_json": json.dumps(argv, separators=(",", ":")),
            "run_as_main": True,
        },
        toolset_name=MCP_TOOLSET,
        timeout=max(MIN_CALL_TIMEOUT_SECONDS, float(timeout_seconds)),
    )
    outer = _decode_json_object(response, f"{marker} outer MCP response")
    if outer.get("success") is not True:
        raise RuntimeError(f"{marker} outer MCP success must be true")
    if outer.get("relative_path") != relative_path:
        raise RuntimeError(f"{marker} outer relative_path mismatch")
    if outer.get("argv") != argv:
        raise RuntimeError(f"{marker} outer argv mismatch")
    if outer.get("run_as_main") is not True:
        raise RuntimeError(f"{marker} outer run_as_main must be true")
    payload = _decode_unique_marker(outer.get("stdout"), marker)
    if payload.get("success") is not True:
        error = payload.get("error")
        if not isinstance(error, str) or not error.strip():
            error = f"{marker} inner success must be true"
        raise RuntimeError(str(error))
    if "error" not in payload or not isinstance(payload.get("error"), str):
        raise RuntimeError(f"{marker} inner error must be a string")
    if payload["error"]:
        raise RuntimeError(f"{marker} successful payload requires an empty error")
    return payload


def _png_dimensions(path: Path) -> list[int]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise RuntimeError(f"capture is not a PNG: {path}")
    offset = 8
    width = height = bit_depth = color_type = interlace = None
    compressed = bytearray()
    saw_end = False
    while offset + 12 <= len(data):
        length = int.from_bytes(data[offset:offset + 4], "big")
        chunk_type = data[offset + 4:offset + 8]
        chunk_start = offset + 8
        chunk_end = chunk_start + length
        crc_end = chunk_end + 4
        if crc_end > len(data):
            raise RuntimeError(f"capture PNG has a truncated chunk: {path}")
        chunk = data[chunk_start:chunk_end]
        observed_crc = int.from_bytes(data[chunk_end:crc_end], "big")
        expected_crc = zlib.crc32(chunk_type + chunk) & 0xFFFFFFFF
        if observed_crc != expected_crc:
            raise RuntimeError(f"capture PNG CRC mismatch: {path}")
        if chunk_type == b"IHDR":
            if length != 13 or width is not None:
                raise RuntimeError(f"capture PNG has invalid IHDR: {path}")
            width = int.from_bytes(chunk[0:4], "big")
            height = int.from_bytes(chunk[4:8], "big")
            bit_depth, color_type, interlace = chunk[8], chunk[9], chunk[12]
        elif chunk_type == b"IDAT":
            compressed.extend(chunk)
        elif chunk_type == b"IEND":
            if length != 0:
                raise RuntimeError(f"capture PNG has invalid IEND: {path}")
            saw_end = True
            offset = crc_end
            break
        offset = crc_end
    if not saw_end or offset != len(data) or width is None or not compressed:
        raise RuntimeError(f"capture PNG is incomplete: {path}")
    channels = {0: 1, 2: 3, 4: 2, 6: 4}.get(color_type)
    if channels is None or bit_depth not in (8, 16) or interlace != 0:
        raise RuntimeError(f"capture PNG format is unsupported: {path}")
    row_bytes = (int(width) * channels * int(bit_depth) + 7) // 8
    try:
        raw = zlib.decompress(bytes(compressed))
    except zlib.error as error:
        raise RuntimeError(f"capture PNG pixel stream is invalid: {path}") from error
    if len(raw) != (row_bytes + 1) * int(height):
        raise RuntimeError(f"capture PNG pixel stream length is invalid: {path}")
    if any(raw[row * (row_bytes + 1)] > 4 for row in range(int(height))):
        raise RuntimeError(f"capture PNG contains an invalid row filter: {path}")
    return [int(width), int(height)]


def _wait_for_fresh_capture(
    path: Path,
    *,
    not_before_ns: int,
    timeout_seconds: float,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> dict[str, Any]:
    path = Path(path)
    deadline = float(monotonic()) + float(timeout_seconds)
    while True:
        if path.is_file():
            stat = path.stat()
            if stat.st_size > 0 and stat.st_mtime_ns > int(not_before_ns):
                dimensions = _png_dimensions(path)
                if dimensions != [CAPTURE_WIDTH, CAPTURE_HEIGHT]:
                    raise RuntimeError(
                        f"capture dimensions must be {CAPTURE_WIDTH}x{CAPTURE_HEIGHT}: "
                        f"{path} -> {dimensions}"
                    )
                return {
                    "path": str(path.resolve()),
                    "dimensions": dimensions,
                    "size_bytes": int(stat.st_size),
                    "mtime_ns": int(stat.st_mtime_ns),
                    "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                }
        now = float(monotonic())
        if now >= deadline:
            raise TimeoutError(f"capture did not become fresh before timeout: {path}")
        sleep(min(0.1, max(0.0, deadline - now)))


def _require_matching_manifest_hashes(values: list[str]) -> str:
    if len(values) != 2 or any(
        not isinstance(value, str) or not _SHA256.fullmatch(value)
        for value in values
    ):
        raise RuntimeError("acceptance requires exactly two lowercase live manifest SHA-256 values")
    if values[0] != values[1]:
        raise RuntimeError(f"two-run live manifest SHA mismatch: {values}")
    return values[0]


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


def _write_json(path: Path, payload: Any) -> None:
    encoded = json.dumps(
        payload,
        ensure_ascii=False,
        sort_keys=True,
        indent=2,
        allow_nan=False,
    ) + "\n"
    path.write_text(encoded, encoding="utf-8")


def _validate_live_payload(payload: dict[str, Any]) -> str:
    digest = payload.get("live_scene_manifest_sha256")
    if not isinstance(digest, str) or not _SHA256.fullmatch(digest):
        raise RuntimeError("validator omitted a lowercase live_scene_manifest_sha256")
    manifest = payload.get("manifest")
    if not isinstance(manifest, dict) or not manifest:
        raise RuntimeError("validator omitted the canonical live manifest")
    try:
        encoded = json.dumps(
            manifest,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
            allow_nan=False,
        ).encode("utf-8")
    except (TypeError, ValueError) as error:
        raise RuntimeError("validator manifest is not canonical JSON data") from error
    recomputed = hashlib.sha256(encoded).hexdigest()
    if recomputed != digest:
        raise RuntimeError(
            f"validator live manifest SHA mismatch: returned={digest}, recomputed={recomputed}"
        )
    return digest


def _capture_one_camera(
    client,
    *,
    camera_label: str,
    path: Path,
    timeout_seconds: float,
    monotonic: Callable[[], float],
    wall_time_ns: Callable[[], int],
    sleep: Callable[[float], None],
) -> dict[str, Any]:
    if camera_label not in CAMERA_LABELS:
        raise RuntimeError(f"unsupported B1 capture camera: {camera_label}")
    if path.exists():
        raise RuntimeError(f"capture target already exists and is stale: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    not_before_ns = int(wall_time_ns())
    argv_base = [
        "--action", "capture",
        "--camera-label", camera_label,
        "--output-path", str(path.resolve()),
    ]
    begin_attempted = False
    deadline = float(monotonic()) + float(timeout_seconds)
    poll_attempts = 0
    final = None
    try:
        begin_attempted = True
        begin = _call_marked_project_script(
            client,
            relative_path=ACCEPTANCE_SCRIPT,
            argv=[*argv_base, "--capture-phase", "begin"],
            marker=ACCEPTANCE_MARKER,
            timeout_seconds=timeout_seconds,
        )
        if begin.get("pending") is not True or begin.get("complete") is not False:
            raise RuntimeError(f"capture begin must be pending for {camera_label}: {begin}")
        while True:
            now = float(monotonic())
            if now >= deadline:
                raise TimeoutError(f"capture task timed out for {camera_label}")
            poll_attempts += 1
            final = _call_marked_project_script(
                client,
                relative_path=ACCEPTANCE_SCRIPT,
                argv=[*argv_base, "--capture-phase", "poll"],
                marker=ACCEPTANCE_MARKER,
                timeout_seconds=max(MIN_CALL_TIMEOUT_SECONDS, deadline - now),
            )
            pending = final.get("pending")
            complete = final.get("complete")
            if pending is False and complete is True:
                break
            if pending is not True or complete is not False:
                raise RuntimeError(f"capture poll flags are invalid for {camera_label}: {final}")
            now = float(monotonic())
            if now >= deadline:
                raise TimeoutError(f"capture task timed out for {camera_label}")
            sleep(min(0.25, deadline - now))
        assert final is not None
        fresh = _wait_for_fresh_capture(
            path,
            not_before_ns=not_before_ns,
            timeout_seconds=max(MIN_CALL_TIMEOUT_SECONDS, deadline - float(monotonic())),
            monotonic=monotonic,
            sleep=sleep,
        )
    except Exception:
        try:
            if not begin_attempted:
                raise RuntimeError("capture begin was not attempted")
            _call_marked_project_script(
                client,
                relative_path=ACCEPTANCE_SCRIPT,
                argv=[*argv_base, "--capture-phase", "abort"],
                marker=ACCEPTANCE_MARKER,
                timeout_seconds=max(MIN_CALL_TIMEOUT_SECONDS, min(5.0, timeout_seconds)),
            )
        except Exception:
            pass
        raise
    if final.get("sha256") != fresh["sha256"] or final.get("dimensions") != fresh["dimensions"]:
        raise RuntimeError(f"UE and host capture evidence disagree for {camera_label}")
    return {
        "camera_label": camera_label,
        "poll_attempts": poll_attempts,
        "viewport_preflight": final.get("viewport_preflight"),
        **fresh,
    }


def _snapshot_csv_files() -> dict[Path, tuple[int, int]]:
    if not CSV_PROFILE_ROOT.is_dir():
        return {}
    return {
        path.resolve(): (path.stat().st_mtime_ns, path.stat().st_size)
        for path in CSV_PROFILE_ROOT.rglob("*.csv")
        if path.is_file()
    }


def _wait_for_fresh_csv(
    before: dict[Path, tuple[int, int]],
    *,
    not_before_ns: int,
    timeout_seconds: float,
    monotonic: Callable[[], float],
    sleep: Callable[[float], None],
) -> Path:
    deadline = float(monotonic()) + float(timeout_seconds)
    stable_candidate: tuple[Path, tuple[int, int]] | None = None
    while True:
        candidates = []
        for path, state in _snapshot_csv_files().items():
            old = before.get(path)
            if state[1] > 0 and state[0] > int(not_before_ns) and old != state:
                candidates.append((path, state))
        if len(candidates) > 1:
            raise RuntimeError(
                f"CSV profile evidence is ambiguous; multiple fresh files: "
                f"{[str(path) for path, _ in candidates]}"
            )
        if len(candidates) == 1:
            candidate = candidates[0]
            if stable_candidate == candidate:
                return candidate[0]
            stable_candidate = candidate
        else:
            stable_candidate = None
        now = float(monotonic())
        if now >= deadline:
            raise TimeoutError("UE CSV profiler did not produce a fresh CSV file")
        sleep(min(0.25, deadline - now))


def _best_effort_performance_cleanup(client) -> list[str]:
    errors = []
    for command in ("csvprofile stop", "stat none"):
        try:
            client.execute_console_command(command)
        except Exception as error:
            errors.append(f"{command}: {_safe_error(error)}")
    try:
        if client.is_in_pie():
            client.stop_pie()
    except Exception as error:
        errors.append(f"StopPIE: {_safe_error(error)}")
    return errors


def _require_unthrottled_performance(
        frame_times: list[float], csv_metrics: Any, *,
        limit_ms: float = FOREGROUND_FRAME_TIME_LIMIT_MS) -> dict[str, float]:
    limit = float(limit_ms)
    values = [float(value) for value in frame_times]
    if (
        not math.isfinite(limit) or limit <= 0.0
        or len(values) < 2
        or any(not math.isfinite(value) or value <= 0.0 for value in values)
        or not isinstance(csv_metrics, dict)
    ):
        raise RuntimeError("foreground performance gate received invalid evidence")
    sample_median = float(statistics.median(values))
    try:
        csv_median = float(csv_metrics["median_frame_time_ms"])
        csv_average_fps = float(csv_metrics["average_fps"])
    except (KeyError, TypeError, ValueError) as error:
        raise RuntimeError("foreground CSV lacks frame-time/FPS evidence") from error
    if any(
        not math.isfinite(value) or value <= 0.0
        for value in (sample_median, csv_median, csv_average_fps)
    ):
        raise RuntimeError("foreground performance evidence is nonfinite or nonpositive")
    if sample_median >= limit or csv_median >= limit:
        raise RuntimeError(
            f"foreground performance remained throttled: sample median={sample_median:.4f} ms, "
            f"CSV median={csv_median:.4f} ms, limit={limit:.4f} ms"
        )
    return {
        "limit_ms": limit,
        "sample_median_frame_time_ms": sample_median,
        "csv_median_frame_time_ms": csv_median,
        "csv_average_fps": csv_average_fps,
    }


def _parse_csv_profile_file(path: Path) -> dict[str, Any]:
    try:
        from scripts.qingshan_town_acceptance import parse_csv_profile
    except ModuleNotFoundError:
        from qingshan_town_acceptance import parse_csv_profile
    metrics = parse_csv_profile(path)
    if not isinstance(metrics, dict):
        raise RuntimeError(f"CSV parser returned invalid metrics for {path}")
    _require_finite_json(metrics, "CSV performance metrics")
    return metrics


def _collect_performance_evidence(
    client,
    *,
    evidence_dir: Path,
    duration_seconds: float,
    monotonic: Callable[[], float],
    wall_time_ns: Callable[[], int],
    sleep: Callable[[float], None],
    csv_filename: str = PERFORMANCE_CSV_FILENAME,
    max_median_frame_time_ms: float | None = None,
) -> dict[str, Any]:
    if float(duration_seconds) < PERFORMANCE_DURATION_SECONDS:
        raise ValueError("B1 performance window must run for at least five seconds")
    if csv_filename not in (
            PERFORMANCE_CSV_FILENAME, FOREGROUND_PERFORMANCE_CSV_FILENAME):
        raise ValueError(f"unsupported B1 performance CSV filename: {csv_filename!r}")
    evidence_dir = Path(evidence_dir).resolve()
    raw_csv_path = (evidence_dir / csv_filename).resolve()
    if raw_csv_path.parent != evidence_dir:
        raise RuntimeError("B1 performance CSV escaped the evidence directory")
    if raw_csv_path.exists():
        raise RuntimeError(f"B1 performance CSV already exists: {raw_csv_path}")
    if client.is_in_pie():
        raise RuntimeError("B1 performance gate requires PIE to be stopped before sampling")
    csv_before = _snapshot_csv_files()
    csv_not_before_ns = int(wall_time_ns())
    samples = []
    memory_start_mb = 0.0
    memory_end_mb = 0.0
    started = float(monotonic())
    cleanup_errors = []
    try:
        client.start_pie(warmup_seconds=1.0)
        if not client.is_in_pie():
            raise RuntimeError("PIE did not start for the B1 performance gate")
        for command in ("stat unit", "stat gpu", "stat rhi", "csvprofile start"):
            client.execute_console_command(command)
        memory_start_mb = float(client.get_editor_memory_mb())
        if not math.isfinite(memory_start_mb) or memory_start_mb <= 0.0:
            raise RuntimeError(f"editor memory probe failed at sample start: {memory_start_mb}")
        started = float(monotonic())
        deadline = started + float(duration_seconds)
        while True:
            sample = _call_marked_project_script(
                client,
                relative_path=ACCEPTANCE_SCRIPT,
                argv=["--action", "performance_sample"],
                marker=ACCEPTANCE_MARKER,
                timeout_seconds=max(5.0, deadline - float(monotonic()) + 5.0),
            )
            samples.append(sample)
            if sample.get("pie_map") != B1_MAP:
                raise RuntimeError(f"performance sample came from the wrong PIE map: {sample}")
            now = float(monotonic())
            if now >= deadline:
                break
            sleep(min(PERFORMANCE_SAMPLE_INTERVAL, deadline - now))
        memory_end_mb = float(client.get_editor_memory_mb())
        if not math.isfinite(memory_end_mb) or memory_end_mb <= 0.0:
            raise RuntimeError(f"editor memory probe failed at sample end: {memory_end_mb}")
    finally:
        cleanup_errors = _best_effort_performance_cleanup(client)
    if cleanup_errors:
        raise RuntimeError(f"performance cleanup was incomplete: {cleanup_errors}")
    if client.is_in_pie():
        raise RuntimeError("PIE remained active after the B1 performance gate")
    elapsed = float(monotonic()) - started
    if elapsed < float(duration_seconds):
        raise RuntimeError(f"performance window ended early: {elapsed}")
    if len(samples) < 2:
        raise RuntimeError("performance gate requires at least two independent samples")
    frame_times = [float(sample["frame_time_ms"]) for sample in samples]
    if any(not math.isfinite(value) or value <= 0.0 for value in frame_times):
        raise RuntimeError(f"invalid frame-time sample: {frame_times}")

    fresh_csv = _wait_for_fresh_csv(
        csv_before,
        not_before_ns=csv_not_before_ns,
        timeout_seconds=15.0,
        monotonic=monotonic,
        sleep=sleep,
    )
    csv_metrics = _parse_csv_profile_file(fresh_csv)
    foreground_gate = None
    if max_median_frame_time_ms is not None:
        foreground_gate = _require_unthrottled_performance(
            frame_times, csv_metrics, limit_ms=max_median_frame_time_ms)
    shutil.copy2(fresh_csv, raw_csv_path)
    result = {
        "duration_seconds": elapsed,
        "sample_count": len(samples),
        "frame_time_ms": {
            "average": statistics.fmean(frame_times),
            "median": statistics.median(frame_times),
            "max": max(frame_times),
        },
        "editor_memory_mb": {
            "start": memory_start_mb,
            "end": memory_end_mb,
            "delta": memory_end_mb - memory_start_mb,
        },
        "samples": samples,
        "csv_source_path": str(fresh_csv),
        "csv_evidence_path": str(raw_csv_path.resolve()),
        "csv_sha256": hashlib.sha256(raw_csv_path.read_bytes()).hexdigest(),
        "csv_metrics": csv_metrics,
    }
    if foreground_gate is not None:
        result["foreground_gate"] = foreground_gate
    return result


def _load_json_value(path: Path, context: str) -> Any:
    if not path.is_file():
        raise RuntimeError(f"{context} is missing: {path}")
    try:
        value = json.loads(
            path.read_text(encoding="utf-8"),
            parse_constant=_reject_json_constant,
        )
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as error:
        raise RuntimeError(f"{context} is not valid UTF-8 JSON: {path}") from error
    _require_finite_json(value, context)
    return value


def _stable_resume_manifest(manifest: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(manifest, dict):
        raise RuntimeError("resume manifest must be an object")
    stable = json.loads(json.dumps(
        manifest, ensure_ascii=False, sort_keys=True,
        separators=(",", ":"), allow_nan=False,
    ))
    for section_name in ("quickroad", "landscape"):
        section = stable.get(section_name)
        if not isinstance(section, dict) or not isinstance(
                section.get("edit_layer_active"), bool):
            raise RuntimeError(
                f"resume manifest lacks boolean {section_name}.edit_layer_active"
            )
        section["edit_layer_active"] = "transient_editor_session_state"
    return stable


def _validate_viewport_preflight(payload: Any, camera_label: str) -> None:
    if not isinstance(payload, dict):
        raise RuntimeError(f"{camera_label} viewport preflight is missing")
    if payload.get("success") is not True or payload.get("error") != "":
        raise RuntimeError(f"{camera_label} viewport preflight failed: {payload}")
    for field in (
        "invalidated", "level_editor_tab_foreground", "level_editor_tab_visible",
        "slate_window_active", "viewport_visible", "viewport_focused",
    ):
        if payload.get(field) is not True:
            raise RuntimeError(f"{camera_label} viewport preflight {field} is not true")
    for field in ("viewport_width", "viewport_height"):
        value = payload.get(field)
        if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
            raise RuntimeError(f"{camera_label} viewport preflight {field} is invalid")


def _load_existing_capture_evidence(evidence_dir: Path) -> list[dict[str, Any]]:
    evidence_dir = Path(evidence_dir).resolve()
    try:
        evidence_dir.relative_to(EVIDENCE_ROOT)
    except ValueError as error:
        raise RuntimeError(f"B1 capture evidence escaped {EVIDENCE_ROOT}") from error
    records = _load_json_value(evidence_dir / "captures.json", "B1 captures evidence")
    if not isinstance(records, list) or len(records) != len(CAMERA_LABELS):
        raise RuntimeError("B1 captures evidence must contain exactly four records")
    if [record.get("camera_label") for record in records if isinstance(record, dict)] != list(
            CAMERA_LABELS):
        raise RuntimeError("B1 captures evidence camera order/labels drifted")
    for record, camera_label in zip(records, CAMERA_LABELS):
        if not isinstance(record, dict):
            raise RuntimeError(f"capture record for {camera_label} is not an object")
        expected_path = (evidence_dir / CAPTURE_FILENAMES[camera_label]).resolve()
        recorded_path = Path(str(record.get("path", ""))).resolve()
        if str(recorded_path).casefold() != str(expected_path).casefold():
            raise RuntimeError(f"capture path drifted for {camera_label}")
        if not expected_path.is_file():
            raise RuntimeError(f"capture file is missing for {camera_label}: {expected_path}")
        stat = expected_path.stat()
        dimensions = _png_dimensions(expected_path)
        digest = hashlib.sha256(expected_path.read_bytes()).hexdigest()
        if (
            dimensions != [CAPTURE_WIDTH, CAPTURE_HEIGHT]
            or record.get("dimensions") != dimensions
            or int(record.get("size_bytes", -1)) != int(stat.st_size)
            or int(record.get("mtime_ns", -1)) != int(stat.st_mtime_ns)
            or record.get("sha256") != digest
        ):
            raise RuntimeError(f"capture bytes/metadata drifted for {camera_label}")
        _validate_viewport_preflight(record.get("viewport_preflight"), camera_label)
    return records


def _require_exact_b1_map_manifest(manifest: Any) -> None:
    if not isinstance(manifest, dict):
        raise RuntimeError("saved B1 manifest is not an object")
    map_state = manifest.get("map")
    if not isinstance(map_state, dict) or any(
        map_state.get(field) != B1_MAP
        for field in ("world_package", "current_level_package")
    ):
        raise RuntimeError("saved B1 manifest map packages drifted")


def _load_existing_manifest_evidence(
        evidence_dir: Path) -> tuple[list[dict[str, Any]], str]:
    paths = [
        evidence_dir / "live-manifest-run-1.json",
        evidence_dir / "live-manifest-run-2.json",
    ]
    raw = [path.read_bytes() if path.is_file() else b"" for path in paths]
    if not raw[0] or raw[0] != raw[1]:
        raise RuntimeError("two saved B1 live manifest files are not byte-identical")
    payloads = [
        _load_json_value(path, f"B1 saved manifest run {index}")
        for index, path in enumerate(paths, start=1)
    ]
    if not all(isinstance(payload, dict) for payload in payloads):
        raise RuntimeError("saved B1 validator payloads must be objects")
    digests = [_validate_live_payload(payload) for payload in payloads]
    digest = _require_matching_manifest_hashes(digests)
    manifest = payloads[-1]["manifest"]
    _require_exact_b1_map_manifest(manifest)
    protected = manifest.get("protected_file_hashes")
    if not isinstance(protected, dict) or not protected:
        raise RuntimeError("saved B1 manifest omitted protected file hashes")
    for relative_path, expected_digest in protected.items():
        path = (PROJECT_ROOT / relative_path).resolve()
        try:
            path.relative_to(PROJECT_ROOT)
        except ValueError as error:
            raise RuntimeError(f"protected resume path escaped project: {relative_path}") from error
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected_digest:
            raise RuntimeError(f"protected file changed after B1 capture: {relative_path}")
    return payloads, digest


def _evidence_state(paths: list[Path]) -> dict[Path, tuple[int, int, str]]:
    state: dict[Path, tuple[int, int, str]] = {}
    for value in paths:
        path = Path(value).resolve()
        if not path.is_file():
            raise RuntimeError(f"required B1 evidence is missing: {path}")
        stat = path.stat()
        state[path] = (
            int(stat.st_size),
            int(stat.st_mtime_ns),
            hashlib.sha256(path.read_bytes()).hexdigest(),
        )
    return state


def _require_evidence_unchanged(
        before: dict[Path, tuple[int, int, str]]) -> None:
    if not isinstance(before, dict) or not before:
        raise RuntimeError("original B1 evidence snapshot is empty")
    after = _evidence_state(list(before))
    if after != before:
        changed = [str(path) for path in before if after.get(path) != before[path]]
        raise RuntimeError(f"original B1 evidence changed during foreground recheck: {changed}")


def _require_performance_payload(payload: Any, *, context: str) -> None:
    if not isinstance(payload, dict):
        raise RuntimeError(f"{context} is not an object")
    samples = payload.get("samples")
    sample_count = payload.get("sample_count")
    if (
        isinstance(sample_count, bool) or not isinstance(sample_count, int)
        or sample_count < 2 or not isinstance(samples, list)
        or len(samples) != sample_count
    ):
        raise RuntimeError(f"{context} has invalid sample count")
    duration = payload.get("duration_seconds")
    if (
        isinstance(duration, bool) or not isinstance(duration, (int, float))
        or not math.isfinite(float(duration))
        or float(duration) < PERFORMANCE_DURATION_SECONDS
    ):
        raise RuntimeError(f"{context} ended before the five-second gate")
    try:
        frame_times = [float(sample["frame_time_ms"]) for sample in samples]
    except (KeyError, TypeError, ValueError) as error:
        raise RuntimeError(f"{context} lacks frame-time samples") from error
    if any(not math.isfinite(value) or value <= 0.0 for value in frame_times):
        raise RuntimeError(f"{context} has invalid frame-time samples")
    aggregate = payload.get("frame_time_ms")
    expected = {
        "average": statistics.fmean(frame_times),
        "median": statistics.median(frame_times),
        "max": max(frame_times),
    }
    if not isinstance(aggregate, dict) or any(
        not math.isclose(
            float(aggregate.get(field, math.nan)), value,
            rel_tol=0.0, abs_tol=1e-9,
        )
        for field, value in expected.items()
    ):
        raise RuntimeError(f"{context} frame-time aggregate drifted")
    memory = payload.get("editor_memory_mb")
    if not isinstance(memory, dict):
        raise RuntimeError(f"{context} lacks editor memory evidence")
    try:
        memory_start = float(memory["start"])
        memory_end = float(memory["end"])
        memory_delta = float(memory["delta"])
    except (KeyError, TypeError, ValueError) as error:
        raise RuntimeError(f"{context} editor memory evidence is malformed") from error
    if (
        not all(math.isfinite(value) for value in (memory_start, memory_end, memory_delta))
        or memory_start <= 0.0 or memory_end <= 0.0
        or not math.isclose(memory_delta, memory_end - memory_start, rel_tol=0.0, abs_tol=1e-9)
    ):
        raise RuntimeError(f"{context} editor memory evidence drifted")
    if not isinstance(payload.get("live_scene"), dict):
        raise RuntimeError(f"{context} lacks live scene metrics")


def _load_existing_performance_evidence(
        evidence_dir: Path, *, manifest_sha256: str,
        captures: list[dict[str, Any]]) -> tuple[
            dict[str, Any], dict[str, Any], dict[Path, tuple[int, int, str]]]:
    evidence_dir = Path(evidence_dir).resolve()
    performance_path = evidence_dir / "performance.json"
    summary_path = evidence_dir / "acceptance-summary.json"
    csv_path = (evidence_dir / PERFORMANCE_CSV_FILENAME).resolve()
    performance = _load_json_value(performance_path, "B1 original performance evidence")
    summary = _load_json_value(summary_path, "B1 original acceptance summary")
    _require_performance_payload(performance, context="B1 original performance evidence")
    if (
        not isinstance(summary, dict)
        or summary.get("success") is not True
        or summary.get("terminal_state") != "complete"
        or summary.get("error") != ""
        or summary.get("live_scene_manifest_sha256") != manifest_sha256
        or summary.get("captures") != captures
        or summary.get("performance") != performance
        or str(Path(str(summary.get("evidence_dir", ""))).resolve()).casefold()
            != str(evidence_dir).casefold()
    ):
        raise RuntimeError("B1 original acceptance summary is inconsistent")
    recorded_csv_path = Path(str(performance.get("csv_evidence_path", ""))).resolve()
    if str(recorded_csv_path).casefold() != str(csv_path).casefold():
        raise RuntimeError("B1 original performance CSV path drifted")
    csv_digest = hashlib.sha256(csv_path.read_bytes()).hexdigest() if csv_path.is_file() else ""
    if performance.get("csv_sha256") != csv_digest or not _SHA256.fullmatch(csv_digest):
        raise RuntimeError("B1 original performance CSV bytes drifted")
    parsed_csv = _parse_csv_profile_file(csv_path)
    if performance.get("csv_metrics") != parsed_csv:
        raise RuntimeError("B1 original performance CSV metrics drifted")
    source_csv = Path(str(performance.get("csv_source_path", ""))).resolve()
    try:
        source_csv.relative_to(CSV_PROFILE_ROOT)
    except ValueError as error:
        raise RuntimeError("B1 original CSV source escaped the profiler root") from error
    if not source_csv.is_file() or hashlib.sha256(source_csv.read_bytes()).hexdigest() != csv_digest:
        raise RuntimeError("B1 original CSV source is missing or changed")
    original_paths = [
        evidence_dir / "live-manifest-run-1.json",
        evidence_dir / "live-manifest-run-2.json",
        evidence_dir / "captures.json",
        *(evidence_dir / CAPTURE_FILENAMES[label] for label in CAMERA_LABELS),
        performance_path,
        summary_path,
        csv_path,
    ]
    return performance, summary, _evidence_state(original_paths)


def _add_live_scene_metrics(
        performance: dict[str, Any], manifest: dict[str, Any]) -> None:
    population = manifest["population_counts"]
    pcg_instance_count = sum(
        int(population[key])
        for key in ("buildings", "props", "static_plants", "mountains")
    )
    world_metrics = manifest.get("world_metrics", manifest.get("performance"))
    if not isinstance(world_metrics, dict):
        raise RuntimeError("validator manifest omitted Actor/Component/Tick metrics")
    performance["live_scene"] = {
        "road_triangle_count": manifest["quickroad"]["road_triangle_count"],
        "pcg_instance_count": pcg_instance_count,
        **world_metrics,
    }


def _require_fresh_formal_acceptance_evidence(evidence_dir: Path) -> None:
    evidence_dir = Path(evidence_dir).resolve()
    try:
        evidence_dir.relative_to(EVIDENCE_ROOT)
    except ValueError as error:
        raise RuntimeError(f"B1 acceptance evidence escaped {EVIDENCE_ROOT}") from error
    existing = [
        path for path in (
            evidence_dir / filename for filename in FORMAL_ACCEPTANCE_FILENAMES
        )
        if path.exists() or path.is_symlink()
    ]
    if existing:
        raise RuntimeError(
            f"formal B1 acceptance outputs already exist: "
            f"{[str(path) for path in existing]}"
        )


def resume_b1_performance(
    client,
    *,
    timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS,
    evidence_dir: Path = EVIDENCE_ROOT,
    monotonic: Callable[[], float] = time.monotonic,
    wall_time_ns: Callable[[], int] = time.time_ns,
    sleep: Callable[[float], None] = time.sleep,
) -> dict[str, Any]:
    evidence_dir = Path(evidence_dir).resolve()
    try:
        evidence_dir.relative_to(EVIDENCE_ROOT)
    except ValueError as error:
        raise RuntimeError(f"B1 resume evidence escaped {EVIDENCE_ROOT}") from error
    output_paths = (
        evidence_dir / "performance.json",
        evidence_dir / "acceptance-summary.json",
        evidence_dir / "qingshan-b1-performance.csv",
    )
    stale = [str(path) for path in output_paths if path.exists()]
    if stale:
        raise RuntimeError(f"resume performance outputs already exist: {stale}")

    saved_validations, manifest_sha256 = _load_existing_manifest_evidence(evidence_dir)
    captures = _load_existing_capture_evidence(evidence_dir)
    current_validation = _call_marked_project_script(
        client,
        relative_path=VALIDATOR_SCRIPT,
        argv=[],
        marker=VALIDATION_MARKER,
        timeout_seconds=timeout_seconds,
    )
    current_digest = _validate_live_payload(current_validation)
    saved_manifest = saved_validations[-1]["manifest"]
    current_manifest = current_validation["manifest"]
    if _stable_resume_manifest(saved_manifest) != _stable_resume_manifest(current_manifest):
        raise RuntimeError("current B1 map differs from saved capture manifest")

    with _foreground_unreal_editor_window():
        performance = _collect_performance_evidence(
            client,
            evidence_dir=evidence_dir,
            duration_seconds=PERFORMANCE_DURATION_SECONDS,
            monotonic=monotonic,
            wall_time_ns=wall_time_ns,
            sleep=sleep,
            max_median_frame_time_ms=FOREGROUND_FRAME_TIME_LIMIT_MS,
        )
    population = current_manifest["population_counts"]
    pcg_instance_count = sum(
        int(population[key])
        for key in ("buildings", "props", "static_plants", "mountains")
    )
    world_metrics = current_manifest.get(
        "world_metrics", current_manifest.get("performance"))
    if not isinstance(world_metrics, dict):
        raise RuntimeError("validator manifest omitted Actor/Component/Tick metrics")
    performance["live_scene"] = {
        "road_triangle_count": current_manifest["quickroad"]["road_triangle_count"],
        "pcg_instance_count": pcg_instance_count,
        **world_metrics,
    }
    _write_json(evidence_dir / "performance.json", performance)
    result = {
        "success": True,
        "terminal_state": "complete",
        "resumed_performance": True,
        "live_scene_manifest_sha256": manifest_sha256,
        "resume_live_scene_manifest_sha256": current_digest,
        "assembly_runs": [
            {"run_index": index, "manifest_sha256": manifest_sha256, "reused": True}
            for index in (1, 2)
        ],
        "captures": captures,
        "performance": performance,
        "evidence_dir": str(evidence_dir),
        "error": "",
    }
    _write_json(evidence_dir / "acceptance-summary.json", result)
    return result


def recheck_b1_performance(
    client,
    *,
    timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS,
    evidence_dir: Path = EVIDENCE_ROOT,
    monotonic: Callable[[], float] = time.monotonic,
    wall_time_ns: Callable[[], int] = time.time_ns,
    sleep: Callable[[float], None] = time.sleep,
) -> dict[str, Any]:
    evidence_dir = Path(evidence_dir).resolve()
    try:
        evidence_dir.relative_to(EVIDENCE_ROOT)
    except ValueError as error:
        raise RuntimeError(f"B1 foreground recheck escaped {EVIDENCE_ROOT}") from error
    output_paths = (
        evidence_dir / "performance-foreground.json",
        evidence_dir / "qingshan-b1-performance-foreground.csv",
        evidence_dir / "acceptance-summary-final.json",
    )
    stale = [str(path) for path in output_paths if path.exists()]
    if stale:
        raise RuntimeError(f"foreground performance outputs already exist: {stale}")

    saved_validations, manifest_sha256 = _load_existing_manifest_evidence(evidence_dir)
    captures = _load_existing_capture_evidence(evidence_dir)
    original_performance, original_summary, original_state = (
        _load_existing_performance_evidence(
            evidence_dir,
            manifest_sha256=manifest_sha256,
            captures=captures,
        )
    )
    current_validation = _call_marked_project_script(
        client,
        relative_path=VALIDATOR_SCRIPT,
        argv=[],
        marker=VALIDATION_MARKER,
        timeout_seconds=timeout_seconds,
    )
    current_digest = _validate_live_payload(current_validation)
    saved_manifest = saved_validations[-1]["manifest"]
    current_manifest = current_validation["manifest"]
    if _stable_resume_manifest(saved_manifest) != _stable_resume_manifest(current_manifest):
        raise RuntimeError("current B1 map differs from saved capture manifest")

    with _foreground_unreal_editor_window():
        performance = _collect_performance_evidence(
            client,
            evidence_dir=evidence_dir,
            duration_seconds=PERFORMANCE_DURATION_SECONDS,
            monotonic=monotonic,
            wall_time_ns=wall_time_ns,
            sleep=sleep,
            csv_filename="qingshan-b1-performance-foreground.csv",
            max_median_frame_time_ms=100.0,
        )
    _add_live_scene_metrics(performance, current_manifest)
    _require_evidence_unchanged(original_state)
    _write_json(evidence_dir / "performance-foreground.json", performance)
    result = {
        "success": True,
        "terminal_state": "complete",
        "foreground_performance_recheck": True,
        "background_performance_diagnostic_only": True,
        "live_scene_manifest_sha256": manifest_sha256,
        "recheck_live_scene_manifest_sha256": current_digest,
        "assembly_runs": [
            {"run_index": index, "manifest_sha256": manifest_sha256, "reused": True}
            for index in (1, 2)
        ],
        "captures": captures,
        "original_performance_sha256": hashlib.sha256(
            (evidence_dir / "performance.json").read_bytes()).hexdigest(),
        "original_acceptance_summary_sha256": hashlib.sha256(
            (evidence_dir / "acceptance-summary.json").read_bytes()).hexdigest(),
        "original_performance": original_performance,
        "original_acceptance_summary_terminal_state": original_summary["terminal_state"],
        "performance": performance,
        "evidence_dir": str(evidence_dir),
        "error": "",
    }
    _write_json(evidence_dir / "acceptance-summary-final.json", result)
    _require_evidence_unchanged(original_state)
    return result


def run_b1_acceptance(
    client,
    *,
    timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS,
    poll_interval: float = DEFAULT_POLL_INTERVAL,
    evidence_dir: Path = EVIDENCE_ROOT,
    monotonic: Callable[[], float] = time.monotonic,
    wall_time_ns: Callable[[], int] = time.time_ns,
    sleep: Callable[[float], None] = time.sleep,
) -> dict[str, Any]:
    evidence_dir = Path(evidence_dir).resolve()
    try:
        evidence_dir.relative_to(EVIDENCE_ROOT)
    except ValueError as error:
        raise RuntimeError(f"B1 acceptance evidence escaped {EVIDENCE_ROOT}") from error
    _require_fresh_formal_acceptance_evidence(evidence_dir)
    evidence_dir.mkdir(parents=True, exist_ok=True)

    assembly_runs = []
    validations = []
    manifest_hashes = []
    for run_index in range(1, 3):
        assembled = run_dress_b1(
            client,
            timeout_seconds=timeout_seconds,
            poll_interval=poll_interval,
            monotonic=monotonic,
            sleep=sleep,
        )
        if assembled.get("success") is not True:
            raise RuntimeError(f"B1 assembly run {run_index} failed: {assembled}")
        validated = _call_marked_project_script(
            client,
            relative_path=VALIDATOR_SCRIPT,
            argv=[],
            marker=VALIDATION_MARKER,
            timeout_seconds=timeout_seconds,
        )
        digest = _validate_live_payload(validated)
        assembly_runs.append({
            "run_index": run_index,
            "terminal_state": assembled["terminal_state"],
            "poll_attempts": assembled["poll_attempts"],
            "elapsed_seconds": assembled["elapsed_seconds"],
            "manifest_sha256": digest,
        })
        validations.append(validated)
        manifest_hashes.append(digest)
        _write_json(evidence_dir / f"live-manifest-run-{run_index}.json", validated)
    manifest_sha256 = _require_matching_manifest_hashes(manifest_hashes)

    captures = []
    for camera_label in CAMERA_LABELS:
        with _foreground_unreal_editor_window():
            captures.append(_capture_one_camera(
                client,
                camera_label=camera_label,
                path=evidence_dir / CAPTURE_FILENAMES[camera_label],
                timeout_seconds=CAPTURE_TIMEOUT_SECONDS,
                monotonic=monotonic,
                wall_time_ns=wall_time_ns,
                sleep=sleep,
            ))
    _write_json(evidence_dir / "captures.json", captures)

    with _foreground_unreal_editor_window():
        performance = _collect_performance_evidence(
            client,
            evidence_dir=evidence_dir,
            duration_seconds=PERFORMANCE_DURATION_SECONDS,
            monotonic=monotonic,
            wall_time_ns=wall_time_ns,
            sleep=sleep,
            max_median_frame_time_ms=FOREGROUND_FRAME_TIME_LIMIT_MS,
        )
    final_manifest = validations[-1]["manifest"]
    population = final_manifest["population_counts"]
    pcg_instance_count = sum(
        int(population[key])
        for key in ("buildings", "props", "static_plants", "mountains")
    )
    world_metrics = final_manifest.get("world_metrics", final_manifest.get("performance"))
    if not isinstance(world_metrics, dict):
        raise RuntimeError("validator manifest omitted Actor/Component/Tick metrics")
    performance["live_scene"] = {
        "road_triangle_count": final_manifest["quickroad"]["road_triangle_count"],
        "pcg_instance_count": pcg_instance_count,
        **world_metrics,
    }
    _write_json(evidence_dir / "performance.json", performance)

    result = {
        "success": True,
        "terminal_state": "complete",
        "live_scene_manifest_sha256": manifest_sha256,
        "assembly_runs": assembly_runs,
        "captures": captures,
        "performance": performance,
        "evidence_dir": str(evidence_dir),
        "error": "",
    }
    _write_json(evidence_dir / "acceptance-summary.json", result)
    return result


def _parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--assemble-once", action="store_true")
    mode.add_argument("--acceptance", action="store_true")
    mode.add_argument("--resume-performance", action="store_true")
    mode.add_argument("--recheck-performance", action="store_true")
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
    connection_stage = "preflight" if args.acceptance else "connection"
    try:
        if args.acceptance:
            _require_fresh_formal_acceptance_evidence(EVIDENCE_ROOT)
        connection_stage = "connection"
        client_class = _load_client_class()
        client = client_class(timeout=max(1.0, min(60.0, args.timeout_seconds)))
        connected = client.connect()
    except Exception as error:
        result = _result(
            success=False,
            terminal_state=f"{connection_stage}_error",
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
                if args.recheck_performance:
                    result = recheck_b1_performance(
                        client,
                        timeout_seconds=args.timeout_seconds,
                    )
                elif args.resume_performance:
                    result = resume_b1_performance(
                        client,
                        timeout_seconds=args.timeout_seconds,
                    )
                elif args.acceptance:
                    result = run_b1_acceptance(
                        client,
                        timeout_seconds=args.timeout_seconds,
                        poll_interval=args.poll_interval,
                    )
                else:
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
