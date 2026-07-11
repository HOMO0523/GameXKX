"""Bounded screenshot and PIE sampling probes for the Qingshan B1 review gate."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import sys
import time
import types
from typing import Any
import zlib

try:  # Host tests import this module without Unreal Engine.
    import unreal
except ModuleNotFoundError:  # pragma: no cover - Unreal supplies this at runtime.
    unreal = None


PROJECT_ROOT = Path(__file__).resolve().parents[2]
EVIDENCE_ROOT = (
    PROJECT_ROOT / "docs" / "production" / "evidence" / "qingshan-pcg-dress-b1"
).resolve()
B1_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1"
B1_MANAGED_TAG = "QingshanB1Managed"
ACCEPTANCE_MARKER = "GAMEXXK_QINGSHAN_B1_ACCEPTANCE"

ACTION_CAPTURE = "capture"
ACTION_PERFORMANCE_SAMPLE = "performance_sample"
CAPTURE_BEGIN = "begin"
CAPTURE_POLL = "poll"
CAPTURE_ABORT = "abort"
CAPTURE_WIDTH = 1920
CAPTURE_HEIGHT = 1080
CAPTURE_TASK_EXPIRY_NS = 120_000_000_000
_STATE_MODULE = "_gamexxk_qingshan_dress_b1_acceptance_state"

CAMERA_LABELS = (
    "CAM_QS_B1_GATE_ARRIVAL",
    "CAM_QS_B1_TOWN_CORE",
    "CAM_QS_B1_MAIN_BRIDGE",
    "CAM_QS_B1_SOUTH_DOCK",
)


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("B1 acceptance probes require Unreal Editor Python")


def _package_path(value: str) -> str:
    return str(value).split(".", 1)[0]


def _current_map() -> str:
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        return ""
    return _package_path(world.get_outermost().get_name())


def _require_b1_editor_world() -> None:
    if _current_map() != B1_MAP:
        raise RuntimeError(f"current editor world must be exact B1 map {B1_MAP}")


def _actor_tags(actor) -> set[str]:
    return {str(value) for value in actor.get_editor_property("tags")}


def _find_unique_camera(camera_label: str):
    if camera_label not in CAMERA_LABELS:
        raise RuntimeError(f"camera label is outside the four-camera B1 gate: {camera_label!r}")
    matches = [
        actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
        if str(actor.get_actor_label()) == camera_label
    ]
    if len(matches) != 1 or not isinstance(matches[0], unreal.CameraActor):
        raise RuntimeError(
            f"camera label must resolve to one CameraActor: {camera_label!r}, matches={len(matches)}"
        )
    actor = matches[0]
    if B1_MANAGED_TAG not in _actor_tags(actor):
        raise RuntimeError(f"camera is not B1-managed: {camera_label}")
    component = actor.get_editor_property("camera_component")
    if component is None:
        raise RuntimeError(f"camera has no CameraComponent: {camera_label}")
    field_of_view = float(component.get_editor_property("field_of_view"))
    if not math.isfinite(field_of_view):
        raise RuntimeError(f"camera FOV is nonfinite: {camera_label}")
    return actor


def _capture_tasks() -> dict[str, dict[str, Any]]:
    state = sys.modules.get(_STATE_MODULE)
    if state is None:
        state = types.ModuleType(_STATE_MODULE)
        state.tasks = {}
        sys.modules[_STATE_MODULE] = state
    return state.tasks


def _release_expired_capture(camera_label: str, now_ns: int) -> bool:
    tasks = _capture_tasks()
    entry = tasks.get(camera_label)
    if not isinstance(entry, dict):
        return False
    requested = entry.get("requested_wall_ns")
    if not isinstance(requested, int) or int(now_ns) - requested >= CAPTURE_TASK_EXPIRY_NS:
        status = _capture_task_status(entry)
        if status["terminal"]:
            tasks.pop(camera_label, None)
            return True
    return False


def _capture_task_status(entry: dict[str, Any]) -> dict[str, Any]:
    task = entry.get("task")
    if task is None:
        return {"valid": False, "done": False, "terminal": True, "state": "invalid"}
    try:
        valid = bool(task.is_valid_task())
    except Exception:
        return {"valid": True, "done": False, "terminal": False, "state": "unknown"}
    if not valid:
        return {"valid": False, "done": False, "terminal": True, "state": "invalid"}
    try:
        done = bool(task.is_task_done())
    except Exception:
        return {"valid": True, "done": False, "terminal": False, "state": "unknown"}
    return {
        "valid": True,
        "done": done,
        "terminal": done,
        "state": "done" if done else "pending",
    }


def _abort_capture_task(camera_label: str) -> dict[str, Any]:
    tasks = _capture_tasks()
    entry = tasks.get(camera_label)
    if not isinstance(entry, dict):
        return {
            "released": False, "pending": False, "complete": False,
            "uncancellable": False, "task_state": "missing",
        }
    status = _capture_task_status(entry)
    if status["terminal"]:
        tasks.pop(camera_label, None)
        return {
            "released": True,
            "pending": False,
            "complete": bool(status["done"]),
            "uncancellable": False,
            "task_state": status["state"],
        }
    return {
        "released": False,
        "pending": True,
        "complete": False,
        "uncancellable": True,
        "task_state": status["state"],
    }


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


def _capture_path(value: str) -> Path:
    path = Path(value)
    if not path.is_absolute() or path.suffix.lower() != ".png":
        raise RuntimeError("capture output must be an absolute .png path")
    resolved = path.resolve()
    try:
        resolved.relative_to(EVIDENCE_ROOT)
    except ValueError as error:
        raise RuntimeError(f"capture must stay under {EVIDENCE_ROOT}") from error
    return resolved


def _prepare_level_viewport_for_capture() -> dict[str, Any]:
    raw = str(
        unreal.GameXXKEditorCaptureAutomationLibrary.prepare_level_viewport_for_capture()
    )
    try:
        payload = json.loads(raw)
    except json.JSONDecodeError as error:
        raise RuntimeError("Level Editor viewport preflight returned invalid JSON") from error
    if not isinstance(payload, dict):
        raise RuntimeError("Level Editor viewport preflight must return an object")
    if payload.get("success") is not True or payload.get("error") != "":
        raise RuntimeError(f"Level Editor viewport preflight failed: {payload}")
    for field in (
        "level_editor_tab_foreground", "level_editor_tab_visible",
        "slate_window_active", "viewport_visible", "viewport_focused", "invalidated",
    ):
        if payload.get(field) is not True:
            raise RuntimeError(f"Level Editor viewport preflight {field} is not true")
    for field in ("viewport_width", "viewport_height"):
        value = payload.get(field)
        if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
            raise RuntimeError(f"Level Editor viewport preflight {field} is invalid")
    return payload


def capture_camera(camera_label: str, output_path: str, capture_phase: str) -> dict[str, Any]:
    """Begin or poll one exact named CameraActor capture without blocking the editor tick."""

    _require_unreal()
    _require_b1_editor_world()
    camera_actor = _find_unique_camera(camera_label)
    path = _capture_path(output_path)
    tasks = _capture_tasks()

    if capture_phase == CAPTURE_ABORT:
        entry = tasks.get(camera_label)
        if isinstance(entry, dict) and entry.get("path") != str(path):
            raise RuntimeError(f"capture abort path does not match active task: {path}")
        abort = _abort_capture_task(camera_label)
        return {
            "success": True,
            "action": ACTION_CAPTURE,
            "capture_phase": CAPTURE_ABORT,
            "camera_label": camera_label,
            "output_path": str(path),
            **abort,
            "error": "",
        }

    if capture_phase == CAPTURE_BEGIN:
        if path.exists():
            raise RuntimeError(f"capture target must not exist before begin: {path}")
        _release_expired_capture(camera_label, time.time_ns())
        if camera_label in tasks:
            raise RuntimeError(f"capture task is already active for {camera_label}")
        path.parent.mkdir(parents=True, exist_ok=True)
        viewport_preflight = _prepare_level_viewport_for_capture()
        requested_wall_ns = time.time_ns()
        task = unreal.AutomationLibrary.take_high_res_screenshot(
            1920,
            1080,
            str(path),
            camera=camera_actor,
            mask_enabled=False,
            capture_hdr=False,
            comparison_tolerance=unreal.ComparisonTolerance.LOW,
            comparison_notes="Qingshan B1 four-camera review gate",
            delay=0.0,
            force_game_view=True,
        )
        if task is None or not bool(task.is_valid_task()):
            raise RuntimeError(f"screenshot task is invalid for {camera_label}")
        tasks[camera_label] = {
            "task": task,
            "path": str(path),
            "requested_wall_ns": requested_wall_ns,
            "viewport_preflight": viewport_preflight,
        }
        return {
            "success": True,
            "action": ACTION_CAPTURE,
            "capture_phase": CAPTURE_BEGIN,
            "camera_label": camera_label,
            "output_path": str(path),
            "pending": True,
            "complete": False,
            "viewport_preflight": viewport_preflight,
            "error": "",
        }

    if capture_phase != CAPTURE_POLL:
        raise RuntimeError(f"unsupported capture phase: {capture_phase!r}")
    entry = tasks.get(camera_label)
    if not isinstance(entry, dict) or entry.get("path") != str(path):
        raise RuntimeError(f"no matching screenshot task for {camera_label}: {path}")
    task = entry.get("task")
    if task is None or not bool(task.is_valid_task()):
        tasks.pop(camera_label, None)
        raise RuntimeError(f"screenshot task became invalid for {camera_label}")
    if not bool(task.is_task_done()):
        return {
            "success": True,
            "action": ACTION_CAPTURE,
            "capture_phase": CAPTURE_POLL,
            "camera_label": camera_label,
            "output_path": str(path),
            "pending": True,
            "complete": False,
            "viewport_preflight": entry["viewport_preflight"],
            "error": "",
        }
    try:
        if not path.is_file() or path.stat().st_size <= 0:
            raise RuntimeError(f"completed screenshot task produced no file: {path}")
        stat = path.stat()
        if stat.st_mtime_ns <= int(entry["requested_wall_ns"]):
            raise RuntimeError(f"completed screenshot is stale: {path}")
        dimensions = _png_dimensions(path)
        if dimensions != [CAPTURE_WIDTH, CAPTURE_HEIGHT]:
            raise RuntimeError(f"capture dimensions drifted: {dimensions}")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
    finally:
        tasks.pop(camera_label, None)
    return {
        "success": True,
        "action": ACTION_CAPTURE,
        "capture_phase": CAPTURE_POLL,
        "camera_label": camera_label,
        "output_path": str(path),
        "pending": False,
        "complete": True,
        "dimensions": dimensions,
        "size_bytes": int(stat.st_size),
        "mtime_ns": int(stat.st_mtime_ns),
        "sha256": digest,
        "viewport_preflight": entry["viewport_preflight"],
        "error": "",
    }


def _game_world():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if subsystem is None:
        return None
    return subsystem.get_game_world()


def _normalized_world_package(world) -> str:
    package = _package_path(world.get_outermost().get_name())
    head, separator, tail = package.rpartition("/")
    normalized = re.sub(r"^UEDPIE_\d+_", "", tail)
    return f"{head}{separator}{normalized}" if separator else normalized


def _world_counts(world) -> dict[str, int]:
    actors = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor))
    component_count = 0
    tick_enabled_count = 0
    for actor in actors:
        try:
            tick_enabled_count += int(bool(actor.is_actor_tick_enabled()))
        except Exception:
            pass
        components = list(actor.get_components_by_class(unreal.ActorComponent))
        component_count += len(components)
        for component in components:
            try:
                tick_enabled_count += int(bool(component.is_component_tick_enabled()))
            except Exception:
                pass
    return {
        "actor_count": len(actors),
        "component_count": component_count,
        "tick_enabled_count": tick_enabled_count,
    }


def performance_sample() -> dict[str, Any]:
    """Read one independent PIE frame/stat sample; the host controls the five-second window."""

    _require_unreal()
    world = _game_world()
    if world is None:
        raise RuntimeError("performance_sample requires an active PIE world")
    pie_map = _normalized_world_package(world)
    if pie_map != B1_MAP:
        raise RuntimeError(f"performance_sample PIE map mismatch: {pie_map} != {B1_MAP}")
    frame_delta_seconds = float(unreal.GameplayStatics.get_world_delta_seconds(world))
    platform_time_seconds = float(unreal.SystemLibrary.get_platform_time_seconds())
    game_time_seconds = float(unreal.GameplayStatics.get_time_seconds(world))
    stat_name = unreal.Name("STAT_FrameTime")
    stat_average_ms = float(unreal.AutomationLibrary.get_stat_inc_average(stat_name))
    stat_max_ms = float(unreal.AutomationLibrary.get_stat_inc_max(stat_name))
    values = (
        frame_delta_seconds,
        platform_time_seconds,
        game_time_seconds,
        stat_average_ms,
        stat_max_ms,
    )
    if not all(math.isfinite(value) for value in values):
        raise RuntimeError(f"performance sample contains nonfinite values: {values}")
    return {
        "success": True,
        "action": ACTION_PERFORMANCE_SAMPLE,
        "pie_map": pie_map,
        "frame_delta_seconds": frame_delta_seconds,
        "frame_time_ms": frame_delta_seconds * 1000.0,
        "platform_time_seconds": platform_time_seconds,
        "game_time_seconds": game_time_seconds,
        "stat_frame_average_ms": stat_average_ms if stat_average_ms > 0.0 else None,
        "stat_frame_max_ms": stat_max_ms if stat_max_ms > 0.0 else None,
        **_world_counts(world),
        "error": "",
    }


def _parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--action", required=True, choices=(ACTION_CAPTURE, ACTION_PERFORMANCE_SAMPLE))
    parser.add_argument("--camera-label", default="")
    parser.add_argument("--output-path", default="")
    parser.add_argument(
        "--capture-phase",
        choices=(CAPTURE_BEGIN, CAPTURE_POLL, CAPTURE_ABORT),
        default=CAPTURE_BEGIN,
    )
    args = parser.parse_args(argv)
    if args.action == ACTION_CAPTURE and (not args.camera_label or not args.output_path):
        parser.error("capture requires --camera-label and --output-path")
    if args.action == ACTION_PERFORMANCE_SAMPLE and (args.camera_label or args.output_path):
        parser.error("performance_sample does not accept capture arguments")
    return args


def _compact(payload: Any) -> str:
    return json.dumps(
        payload,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    )


def main() -> None:
    try:
        args = _parse_args()
        if args.action == ACTION_CAPTURE:
            result = capture_camera(args.camera_label, args.output_path, args.capture_phase)
        else:
            result = performance_sample()
    except Exception as error:
        result = {
            "success": False,
            "error": str(error),
            "error_type": type(error).__name__,
        }
    print(f"{ACCEPTANCE_MARKER} {_compact(result)}")


if __name__ == "__main__":
    main()
