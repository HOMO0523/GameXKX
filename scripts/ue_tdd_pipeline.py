"""
UE TDD No-LiveCoding Pipeline through UE 5.8 built-in MCP.

Usage:
    python scripts/ue_tdd_pipeline.py                    # Full cycle
    python scripts/ue_tdd_pipeline.py --no-build          # Skip build, launch a fresh editor
    python scripts/ue_tdd_pipeline.py --no-build --no-launch
                                                          # Use an already running MCP editor
    python scripts/ue_tdd_pipeline.py --pie-duration 10   # Run PIE for 10 seconds
    python scripts/ue_tdd_pipeline.py --check-only        # Only capture/analyze logs (no build/launch)
"""

import subprocess
import sys
import time
import argparse
from pathlib import Path

# === Config ===
# Auto-detect project root from script location
_SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = _SCRIPT_DIR.parent
UPROJECT = PROJECT_ROOT / "GameXXK.uproject"

# UE 5.8 path — check common locations first, then older fallbacks for local triage.
_UE_CANDIDATES = [
    Path(r"E:\epic\UE_5.8"),
    Path(r"E:\UE_5.8"),
    Path(r"D:\UE_5.8"),
    Path(r"C:\Program Files\Epic Games\UE_5.8"),
    Path(r"E:\epic\UE_5.7"),
    Path(r"E:\UE_5.7"),
    Path(r"D:\UE_5.7"),
    Path(r"C:\Program Files\Epic Games\UE_5.7"),
]
UE_ROOT = None
for _cand in _UE_CANDIDATES:
    _ue_editor = _cand / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe"
    if _ue_editor.exists():
        UE_ROOT = _cand
        break

if UE_ROOT is None:
    # Fallback: try to find via registry or parent of UE_5.5
    import os as _os
    _ue55 = Path(r"E:\UE_5.5")
    if _ue55.exists():
        UE_ROOT = _ue55  # fallback to 5.5
    else:
        raise RuntimeError(
            "Cannot find Unreal Engine installation. "
            "Checked: " + ", ".join(str(c) for c in _UE_CANDIDATES)
        )

UE_EDITOR = UE_ROOT / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe"
UE_BUILD_BAT = UE_ROOT / "Engine" / "Build" / "BatchFiles" / "Build.bat"
BUILD_TARGET = "GameXXKEditor"
BUILD_CONFIG = "Development"
SCRIPT_DIR = Path(__file__).resolve().parent

sys.path.insert(0, str(SCRIPT_DIR))
from ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, DEFAULT_PORT, UnrealMCPClient


# === Editor Lifecycle ===

def kill_editor() -> bool:
    """Kill any running UnrealEditor process. Returns True if a process was killed."""
    result = subprocess.run(
        ["taskkill", "/f", "/im", "UnrealEditor.exe"],
        capture_output=True, text=True
    )
    killed = "SUCCESS" in result.stdout.upper() or result.returncode == 0
    if killed:
        print("[KILL] UnrealEditor terminated")
    else:
        print("[KILL] No running editor found")
    return killed


def is_editor_running() -> bool | None:
    """Return whether UnrealEditor is running, or None if process query is unavailable."""
    result = subprocess.run(
        ["tasklist", "/FI", "IMAGENAME eq UnrealEditor.exe"],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        ps_result = subprocess.run(
            [
                "powershell",
                "-NoProfile",
                "-Command",
                "if (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue) { 'RUNNING' } else { 'NOT_RUNNING' }",
            ],
            capture_output=True, text=True
        )
        if ps_result.returncode != 0:
            return None
        ps_output = (ps_result.stdout or "") + (ps_result.stderr or "")
        if "NOT_RUNNING" in ps_output:
            return False
        if "RUNNING" in ps_output:
            return True
        return None
    output = (result.stdout or "") + (result.stderr or "")
    return "UnrealEditor.exe" in output


def save_running_editor_before_close(host: str = DEFAULT_HOST, port: int = DEFAULT_PORT, path: str = DEFAULT_PATH) -> bool:
    """Save dirty editor packages through UE MCP before closing the editor."""
    running = is_editor_running()
    if running is False:
        print("[SAVE] No running editor detected")
        return True

    print("[SAVE] Editor may be running; attempting UE MCP save before close...")
    client = UnrealMCPClient(host=host, port=port, path=path, timeout=30.0)
    if not client.connect():
        if running is True:
            print("[SAVE] FAILED: editor is running but UE MCP is unavailable; refusing to close before compile")
            return False
        print("[SAVE] UE MCP unavailable and editor state unknown; refusing to close before compile")
        return False

    response = client.save_dirty_packages()
    print(f"[SAVE] save_result={response.get('save_result')} dirty_before={response.get('dirty_before')} dirty_after={response.get('dirty_after')}")
    if not response.get("save_result") or response.get("dirty_after"):
        print("[SAVE] FAILED: dirty package save did not report success; refusing to close before compile")
        return False
    return True


def build_project() -> bool:
    """Run UnrealBuildTool. Returns True on success."""
    cmd = [
        str(UE_BUILD_BAT),
        BUILD_TARGET, "Win64", BUILD_CONFIG,
        f'-Project={UPROJECT.as_posix()}'
    ]
    print(f"[BUILD] {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=str(UE_BUILD_BAT.parent.parent.parent))
    success = result.returncode == 0
    if success:
        print("[BUILD] Compile succeeded")
    else:
        print(f"[BUILD] Compile FAILED (exit {result.returncode})")
    return success


def launch_editor(mcp_port: int = DEFAULT_PORT) -> subprocess.Popen | None:
    """Launch UE editor in background. Returns the Popen handle."""
    print(f"[LAUNCH] Starting editor...")
    try:
        proc = subprocess.Popen(
            [
                str(UE_EDITOR),
                UPROJECT.as_posix(),
                "-ModelContextProtocolStartServer",
                f"-ModelContextProtocolPort={int(mcp_port)}",
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        print(f"[LAUNCH] Editor PID: {proc.pid}")
        return proc
    except Exception as e:
        print(f"[LAUNCH] Failed: {e}")
        return None


def wait_for_mcp(
    host: str = DEFAULT_HOST,
    port: int = DEFAULT_PORT,
    path: str = DEFAULT_PATH,
    timeout: float = 120.0,
    interval: float = 3.0,
) -> UnrealMCPClient | None:
    """Wait for UE MCP to become ready. Returns connected client or None."""
    print(f"[WAIT] Waiting for UE MCP at http://{host}:{port}{path} (timeout={timeout}s)...")
    deadline = time.time() + timeout
    client = UnrealMCPClient(host=host, port=port, path=path, timeout=30.0)

    while time.time() < deadline:
        if client.connect():
            print(f"[WAIT] UE MCP ready at {client.endpoint}")
            return client
        print(f"  ... waiting ({interval}s)")
        time.sleep(interval)

    print("[WAIT] UE MCP connection timed out")
    return None


# === TDD Pipeline ===

def run_tdd_cycle(
    build: bool = True,
    launch: bool = True,
    pie_duration: float = 5.0,
    log_lines: int = 300,
    tdd_filter: str = "[TDD]",
    mcp_host: str = DEFAULT_HOST,
    mcp_port: int = DEFAULT_PORT,
    mcp_path: str = DEFAULT_PATH,
) -> dict:
    """Run the full TDD cycle.

    Returns:
        dict with keys: success, tdd_lines, all_lines, pie_started, pie_duration
    """
    result = {"success": False, "tdd_lines": [], "all_lines": [], "pie_started": False}

    if build and not launch:
        result["error"] = "--no-launch cannot be combined with a build; use --no-build --no-launch for an existing MCP editor"
        return result

    # Phase 4.1: Save and close editor before cold compile/launch
    if build or launch:
        if not save_running_editor_before_close(host=mcp_host, port=mcp_port, path=mcp_path):
            result["error"] = "Could not save running editor before close"
            return result
        kill_editor()
        time.sleep(2)

    # Phase 4.2: Build
    if build:
        if not build_project():
            result["error"] = "Build failed"
            return result

    # Phase 4.3: Launch editor
    if launch:
        launch_editor(mcp_port=mcp_port)
    else:
        print("[LAUNCH] --no-launch set; connecting to an already running UE MCP editor")

    # Phase 4.4: Wait for UE MCP
    client = wait_for_mcp(host=mcp_host, port=mcp_port, path=mcp_path)
    if client is None:
        result["error"] = "UE MCP never became ready"
        return result

    # Give the editor a moment to fully settle
    time.sleep(3)

    # Phase 4.5: Clear logs + start PIE
    print("[TDD] Setting log marker and starting PIE...")
    client.clear_log_buffer()
    client.write_log(f"[TDD] Pipeline cycle start at {time.strftime('%H:%M:%S')}", "Log")

    if client.is_in_pie():
        print("[TDD] PIE was already running, stopping first...")
        client.stop_pie()
        time.sleep(2)

    if not client.start_pie(warmup_seconds=1.0):
        result["error"] = "Failed to start PIE"
        return result

    result["pie_started"] = True
    print(f"[TDD] PIE started, running for {pie_duration}s...")

    # Phase 4.6: Wait for simulation
    time.sleep(pie_duration)

    # Phase 4.7: Verify PIE is still running + capture logs
    world_time = client.get_pie_world_time()
    print(f"[TDD] PIE world time: {world_time:.2f}s")

    tdd_lines = client.filter_tdd_lines(num_lines=log_lines)
    all_lines = client.get_recent_log_lines(num_lines=log_lines)

    # Phase 4.8: Stop PIE
    client.stop_pie()
    print("[TDD] PIE stopped")

    # Additional wait for any post-PIE log flush
    time.sleep(1)
    tdd_lines2 = client.filter_tdd_lines(num_lines=log_lines)
    all_lines2 = client.get_recent_log_lines(num_lines=log_lines)

    # Merge
    result["tdd_lines"] = list(dict.fromkeys(tdd_lines + tdd_lines2))
    result["all_lines"] = all_lines + all_lines2
    result["success"] = True
    result["pie_duration"] = world_time

    return result


def analyze_logs(tdd_lines: list[str]) -> dict:
    """Analyze captured [TDD] log lines and produce a report.

    Expected format: [TDD] CheckName: actual=%d expected=%d
    Also supports: [TDD] CheckName: <any descriptive assertion>
    """
    report = {"total": len(tdd_lines), "passed": 0, "failed": 0, "details": []}

    for line in tdd_lines:
        # Try to parse structured assertion: actual=%d expected=%d
        detail = {"line": line, "status": "info"}
        b_structured_assertion = False
        explicit_result = None
        import re

        result_match = re.search(r"\bresult\s*=\s*(PASS|FAIL|OK|ERROR)\b", line, re.IGNORECASE)
        if result_match:
            explicit_result = result_match.group(1).upper()

        if "actual=" in line.lower() and "expected=" in line.lower():
            actual_match = re.search(r"(?<![A-Za-z_])actual[=:]\s*([-\d.]+)", line, re.IGNORECASE)
            expected_match = re.search(r"(?<![A-Za-z_])expected[=:]\s*([-\d.]+)", line, re.IGNORECASE)
            if actual_match and expected_match:
                b_structured_assertion = True
                actual_val = actual_match.group(1)
                expected_val = expected_match.group(1)
                detail["actual"] = actual_val
                detail["expected"] = expected_val
                if explicit_result in ("PASS", "OK"):
                    detail["status"] = "pass"
                    report["passed"] += 1
                elif explicit_result in ("FAIL", "ERROR"):
                    detail["status"] = "fail"
                    report["failed"] += 1
                elif actual_val == expected_val:
                    detail["status"] = "pass"
                    report["passed"] += 1
                else:
                    detail["status"] = "fail"
                    report["failed"] += 1
                detail["check"] = line.split("actual=")[0].strip()

        if not b_structured_assertion and explicit_result in ("PASS", "OK"):
            detail["status"] = "pass"
            report["passed"] += 1
        elif not b_structured_assertion and explicit_result in ("FAIL", "ERROR"):
            detail["status"] = "fail"
            report["failed"] += 1
        elif not b_structured_assertion and ("PASS" in line.upper() or "OK" in line.upper()):
            detail["status"] = "pass"
            report["passed"] += 1
        elif not b_structured_assertion and ("FAIL" in line.upper() or "ERROR" in line.upper()):
            detail["status"] = "fail"
            report["failed"] += 1

        report["details"].append(detail)

    return report


# === CLI ===

def main():
    parser = argparse.ArgumentParser(description="UE TDD No-LiveCoding Pipeline")
    parser.add_argument("--no-build", action="store_true", help="Skip build step")
    parser.add_argument("--no-launch", action="store_true", help="Skip editor launch; not valid as compile proof with an open editor")
    parser.add_argument("--check-only", action="store_true", help="Only capture and analyze logs")
    parser.add_argument("--pie-duration", type=float, default=5.0, help="Seconds to run PIE (default: 5)")
    parser.add_argument("--log-lines", type=int, default=300, help="Log lines to capture (default: 300)")
    parser.add_argument("--filter", type=str, default="[TDD]", help="Log filter pattern (default: [TDD])")
    parser.add_argument("--mcp-host", default=DEFAULT_HOST, help="UE MCP host")
    parser.add_argument("--mcp-port", type=int, default=DEFAULT_PORT, help="UE MCP port")
    parser.add_argument("--mcp-path", default=DEFAULT_PATH, help="UE MCP URL path")
    args = parser.parse_args()

    print("=" * 60)
    print("  UE TDD No-LiveCoding Pipeline")
    print(f"  Project: {UPROJECT}")
    print("=" * 60)

    if args.check_only:
        print("[PIPELINE] Check-only mode - capturing logs from running editor")
        client = UnrealMCPClient(host=args.mcp_host, port=args.mcp_port, path=args.mcp_path, timeout=30.0)
        if not client.connect():
            print("FATAL: Cannot connect to UE MCP. Is editor running with -ModelContextProtocolStartServer?")
            sys.exit(1)
        tdd_lines = client.filter_tdd_lines(num_lines=args.log_lines)
        report = analyze_logs(tdd_lines)
    else:
        result = run_tdd_cycle(
            build=not args.no_build,
            launch=not args.no_launch,
            pie_duration=args.pie_duration,
            log_lines=args.log_lines,
            tdd_filter=args.filter,
            mcp_host=args.mcp_host,
            mcp_port=args.mcp_port,
            mcp_path=args.mcp_path,
        )

        if not result.get("success"):
            print(f"\n[PIPELINE] FAILED: {result.get('error', 'Unknown error')}")
            sys.exit(1)

        print(f"\n[PIPELINE] Captured {len(result['all_lines'])} log lines total")
        report = analyze_logs(result["tdd_lines"])

    # Print report
    print("\n" + "=" * 60)
    print("  TDD Report")
    print("=" * 60)
    print(f"  Total [TDD] lines: {report['total']}")
    print(f"  Passed:            {report['passed']}")
    print(f"  Failed:            {report['failed']}")

    if report["details"]:
        print("\n  --- Details ---")
        for d in report["details"]:
            status_icon = "PASS" if d["status"] == "pass" else ("FAIL" if d["status"] == "fail" else "INFO")
            print(f"  [{status_icon}] {d['line'][:120]}")
            if d["status"] == "fail":
                print(f"           expected={d.get('expected', '?')}  actual={d.get('actual', '?')}")

    print("=" * 60)

    exit_code = 0 if report["failed"] == 0 else 1
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
