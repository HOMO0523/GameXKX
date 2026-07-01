from __future__ import annotations

import contextlib
import ctypes
import io
import json
import platform
import sys
from pathlib import Path

import toolset_registry
import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
ALLOWED_SCRIPT_ROOTS = (
    (PROJECT_ROOT / "scripts").resolve(),
    (PROJECT_ROOT / "Content" / "Python").resolve(),
)


def _resolve_project_script(relative_path: str) -> Path:
    candidate = (PROJECT_ROOT / relative_path).resolve()
    if candidate.suffix.lower() != ".py":
        raise RuntimeError(f"Only .py files can be executed through GameXXKTDDToolset: {relative_path}")
    if not candidate.exists():
        raise RuntimeError(f"Project Python file does not exist: {relative_path}")
    if not any(candidate.is_relative_to(root) for root in ALLOWED_SCRIPT_ROOTS):
        raise RuntimeError(f"Project Python file is outside allowed roots: {relative_path}")
    return candidate


def _get_editor_subsystem():
    try:
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    except Exception:
        return None


def _get_game_world():
    subsystem = _get_editor_subsystem()
    if subsystem is None:
        return None
    try:
        return subsystem.get_game_world()
    except Exception:
        return None


def _get_editor_world():
    subsystem = _get_editor_subsystem()
    if subsystem is not None:
        try:
            return subsystem.get_editor_world()
        except Exception:
            pass
    try:
        return unreal.EditorLevelLibrary.get_editor_world()
    except Exception:
        return None


def _get_active_world():
    return _get_game_world() or _get_editor_world()


def _get_process_working_set_mb() -> float:
    if platform.system().lower() != "windows":
        return 0.0

    class ProcessMemoryCounters(ctypes.Structure):
        _fields_ = [
            ("cb", ctypes.c_ulong),
            ("page_fault_count", ctypes.c_ulong),
            ("peak_working_set_size", ctypes.c_size_t),
            ("working_set_size", ctypes.c_size_t),
            ("quota_peak_paged_pool_usage", ctypes.c_size_t),
            ("quota_paged_pool_usage", ctypes.c_size_t),
            ("quota_peak_non_paged_pool_usage", ctypes.c_size_t),
            ("quota_non_paged_pool_usage", ctypes.c_size_t),
            ("pagefile_usage", ctypes.c_size_t),
            ("peak_pagefile_usage", ctypes.c_size_t),
        ]

    counters = ProcessMemoryCounters()
    counters.cb = ctypes.sizeof(ProcessMemoryCounters)
    process = ctypes.windll.kernel32.GetCurrentProcess()
    ok = ctypes.windll.psapi.GetProcessMemoryInfo(process, ctypes.byref(counters), counters.cb)
    if not ok:
        return 0.0
    return float(counters.working_set_size) / (1024.0 * 1024.0)


@unreal.uclass()
class GameXXKTDDToolset(unreal.ToolsetDefinition):
    """GameXXK project MCP tools for no-Bridge TDD automation."""

    @toolset_registry.tool_call
    def execute_console_command(command: str) -> str:
        """Execute one Unreal console command in the active PIE world when available.

        Args:
            command: Console command, for example "GameXXK.RunMVPPlaytest".

        Returns:
            Dict with command, success, and context world kind.
        """
        world = _get_active_world()
        if world is None:
            raise RuntimeError("No active editor or PIE world is available")
        unreal.SystemLibrary.execute_console_command(world, command)
        return json.dumps({
            "success": True,
            "command": command,
            "world": "PIE" if _get_game_world() is not None else "Editor",
        }, ensure_ascii=False)

    @toolset_registry.tool_call
    def save_dirty_packages() -> str:
        """Save dirty content and map packages.

        Returns:
            Dict with save_result and dirty package names before/after save.
        """
        dirty_before = [pkg.get_name() for pkg in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
        dirty_before += [pkg.get_name() for pkg in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
        save_result = bool(unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True))
        dirty_after = [pkg.get_name() for pkg in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
        dirty_after += [pkg.get_name() for pkg in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
        return json.dumps({
            "save_result": save_result,
            "dirty_before": dirty_before,
            "dirty_after": dirty_after,
        }, ensure_ascii=False)

    @toolset_registry.tool_call
    def get_pie_world_time() -> float:
        """Return current PIE world time in seconds, or -1 if PIE is not active."""
        world = _get_game_world()
        if world is None:
            return -1.0
        try:
            return float(world.get_time_seconds())
        except Exception:
            try:
                return float(unreal.GameplayStatics.get_time_seconds(world))
            except Exception:
                return -1.0

    @toolset_registry.tool_call
    def get_editor_memory_mb() -> float:
        """Return the current UnrealEditor process working set in MB, or 0 if unavailable."""
        try:
            return round(_get_process_working_set_mb(), 1)
        except Exception as exc:
            unreal.log_warning("GameXXKTDDToolset memory probe failed: %s" % exc)
            return 0.0

    @toolset_registry.tool_call
    def collect_garbage(full_purge: bool = True) -> str:
        """Run Unreal garbage collection after long automation sessions."""
        try:
            unreal.SystemLibrary.collect_garbage()
            return json.dumps({"success": True, "full_purge": bool(full_purge)}, ensure_ascii=False)
        except Exception as exc:
            unreal.log_warning("GameXXKTDDToolset garbage collection failed: %s" % exc)
            return json.dumps({"success": False, "error": str(exc), "full_purge": bool(full_purge)}, ensure_ascii=False)

    @toolset_registry.tool_call
    def write_log(message: str, severity: str = "Log") -> str:
        """Write a project automation marker to the Unreal log.

        Args:
            message: Message to write.
            severity: Log, Display, Warning, or Error.

        Returns:
            Dict with success and severity.
        """
        normalized = (severity or "Log").lower()
        if normalized == "error":
            unreal.log_error(message)
        elif normalized == "warning":
            unreal.log_warning(message)
        else:
            unreal.log(message)
        return json.dumps({"success": True, "severity": severity or "Log"}, ensure_ascii=False)

    @toolset_registry.tool_call
    def run_project_python_file(relative_path: str, argv_json: str = "[]", run_as_main: bool = True) -> str:
        """Execute a project Python file under scripts/ or Content/Python/.

        Args:
            relative_path: Project-relative script path.
            argv_json: JSON list of string arguments exposed to the script as sys.argv[1:].
            run_as_main: Whether to execute with __name__ == "__main__".

        Returns:
            Dict with success, relative_path, and captured stdout.
        """
        script_path = _resolve_project_script(relative_path)
        try:
            parsed_argv = json.loads(argv_json or "[]")
        except Exception as exc:
            raise RuntimeError(f"argv_json must be a JSON string list: {exc}") from exc
        if not isinstance(parsed_argv, list) or not all(isinstance(item, str) for item in parsed_argv):
            raise RuntimeError("argv_json must decode to a list of strings")

        source = script_path.read_text(encoding="utf-8")
        stdout = io.StringIO()
        globals_dict = {
            "__file__": str(script_path),
            "__name__": "__main__" if bool(run_as_main) else "__gamexxk_mcp_exec__",
            "unreal": unreal,
        }
        old_argv = sys.argv[:]
        sys.argv = [str(script_path), *parsed_argv]
        try:
            with contextlib.redirect_stdout(stdout):
                exec(compile(source, str(script_path), "exec"), globals_dict, globals_dict)
        finally:
            sys.argv = old_argv
        return json.dumps({
            "success": True,
            "relative_path": relative_path,
            "argv": parsed_argv,
            "run_as_main": bool(run_as_main),
            "stdout": stdout.getvalue(),
        }, ensure_ascii=False)
