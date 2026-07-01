"""
GameXXK UE TDD no-LiveCoding pipeline.

This is adapted from the Ocean automation workflow for the GameXXK UE 5.8
project. It saves a running editor through UnrealBridge before cold builds,
then can relaunch the editor and collect [TDD] logs from PIE.
"""

import argparse
import subprocess
import sys
import time
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
UPROJECT = PROJECT_ROOT / "GameXXK.uproject"

UE_CANDIDATES = [
    Path(r"D:\UE_5.8"),
    Path(r"E:\UE_5.8"),
    Path(r"C:\Program Files\Epic Games\UE_5.8"),
]

UE_ROOT = None
for candidate in UE_CANDIDATES:
    if (candidate / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe").exists():
        UE_ROOT = candidate
        break

if UE_ROOT is None:
    raise RuntimeError("Cannot find UE 5.8. Checked: " + ", ".join(str(p) for p in UE_CANDIDATES))

UE_EDITOR = UE_ROOT / "Engine" / "Binaries" / "Win64" / "UnrealEditor.exe"
UE_BUILD_BAT = UE_ROOT / "Engine" / "Build" / "BatchFiles" / "Build.bat"
BUILD_TARGET = "GameXXKEditor"
BUILD_CONFIG = "Development"

sys.path.insert(0, str(SCRIPT_DIR))
from ue_tdd_bridge import BridgeClient  # noqa: E402


def is_editor_running() -> bool | None:
    result = subprocess.run(
        ["tasklist", "/FI", "IMAGENAME eq UnrealEditor.exe"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return None
    output = (result.stdout or "") + (result.stderr or "")
    return "UnrealEditor.exe" in output


def save_running_editor_before_close() -> bool:
    running = is_editor_running()
    if running is False:
        print("[SAVE] No running editor detected")
        return True

    print("[SAVE] Editor may be running; attempting UnrealBridge save before close...")
    client = BridgeClient()
    if not client.connect():
        print("[SAVE] FAILED: editor is running or state is unknown, but bridge is unavailable")
        return False

    script = r"""
import unreal
dirty_before = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
dirty_before += [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
save_result = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
dirty_after = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
dirty_after += [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
print(f"save_result={save_result} dirty_before={dirty_before} dirty_after={dirty_after}")
"""
    response = client.send(script, timeout=120)
    output = response.get("output", "").strip()
    if output:
        print(f"[SAVE] {output}")
    return bool(response.get("success")) and "save_result=True" in output


def kill_editor() -> None:
    subprocess.run(["taskkill", "/f", "/im", "UnrealEditor.exe"], capture_output=True, text=True)


def build_project() -> bool:
    cmd = [
        str(UE_BUILD_BAT),
        BUILD_TARGET,
        "Win64",
        BUILD_CONFIG,
        f"-Project={UPROJECT.as_posix()}",
        "-NoHotReload",
    ]
    print(f"[BUILD] {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=str(UE_BUILD_BAT.parent.parent.parent))
    print("[BUILD] exit=" + str(result.returncode))
    return result.returncode == 0


def launch_editor() -> subprocess.Popen | None:
    try:
        proc = subprocess.Popen(
            [str(UE_EDITOR), UPROJECT.as_posix(), "-UnrealBridgeForceReady"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        print(f"[LAUNCH] Editor PID: {proc.pid}")
        return proc
    except Exception as exc:
        print(f"[LAUNCH] Failed: {exc}")
        return None


def wait_for_bridge(timeout: float = 120.0, interval: float = 3.0) -> BridgeClient | None:
    deadline = time.time() + timeout
    client = BridgeClient()
    while time.time() < deadline:
        if client.connect():
            response = client.send("print('GameXXK bridge ready')")
            if response.get("success"):
                print(f"[WAIT] Bridge ready on port {client.port}")
                return client
        time.sleep(interval)
    print("[WAIT] Bridge connection timed out")
    return None


def run_cycle(build: bool, launch: bool, pie_duration: float, log_lines: int) -> int:
    if build or launch:
        if not save_running_editor_before_close():
            return 1
        kill_editor()
        time.sleep(2)

    if build and not build_project():
        return 1

    if not launch:
        return 0

    launch_editor()
    client = wait_for_bridge()
    if client is None:
        return 1

    if client.is_in_pie():
        client.stop_pie()
        time.sleep(1)

    client.clear_log_buffer()
    client.write_log("[TDD] GameXXKPipeline: cycle_start result=PASS", "Log")
    if not client.start_pie():
        print("[PIE] Failed to start PIE")
        return 1

    time.sleep(pie_duration)
    tdd_lines = client.filter_tdd_lines(num_lines=log_lines)
    client.stop_pie()

    failed = [line for line in tdd_lines if "FAIL" in line.upper() or "ERROR" in line.upper()]
    print(f"[TDD] lines={len(tdd_lines)} failed={len(failed)}")
    for line in tdd_lines:
        print(line)
    return 0 if not failed else 1


def main() -> None:
    parser = argparse.ArgumentParser(description="GameXXK UE no-LiveCoding pipeline")
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--no-launch", action="store_true")
    parser.add_argument("--check-only", action="store_true")
    parser.add_argument("--pie-duration", type=float, default=5.0)
    parser.add_argument("--log-lines", type=int, default=300)
    args = parser.parse_args()

    if args.check_only:
        client = BridgeClient()
        if not client.connect():
            print("FATAL: Cannot connect to UnrealBridge")
            sys.exit(1)
        lines = client.filter_tdd_lines(num_lines=args.log_lines)
        for line in lines:
            print(line)
        sys.exit(0)

    sys.exit(run_cycle(not args.no_build, not args.no_launch, args.pie_duration, args.log_lines))


if __name__ == "__main__":
    main()
