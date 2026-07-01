#!/usr/bin/env python3
"""Run the scripted GameXXK MVP playtest through Unreal Automation."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import subprocess
import sys
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
UPROJECT = PROJECT_ROOT / "GameXXK.uproject"
BUILD_TARGET = "GameXXKEditor"
BUILD_CONFIG = "Development"
TEST_FILTER = "GameXXK.MVP"

UE_CANDIDATES = [
    Path(r"D:\UE_5.8"),
    Path(r"E:\UE_5.8"),
    Path(r"C:\Program Files\Epic Games\UE_5.8"),
]

CHECKLIST = [
    "main menu opens world map",
    "world map enters unlocked Qingshan town",
    "locked Tanjiang cannot be entered before Boss",
    "quest NPC accepts Huangshan route quest",
    "follower joins and remains after failure",
    "merchant buy/sell changes gold and inventory",
    "accepted quest opens fixed node dungeon",
    "normal battle grants XP/gold/items",
    "dungeon failure returns to town and keeps earned XP/gold",
    "used consumables stay deducted after failure",
    "retry reaches event, camp, and Boss nodes",
    "Boss victory completes quest and unlocks Tanjiang",
    "true SaveGame slot roundtrip persists unlocks, quest, level, XP, and gold",
    "StartGame and load-or-create APIs load saved slots and open world map",
    "SaveGame load-or-create opens world map and missing slots create new state",
    "inventory and equipment details are intentionally not persisted",
    "town player shell binds WASD, arrow keys, and F interaction",
    "town NPC overlap focuses F interaction for quest acceptance, follower activation, and merchant buying",
    "town quest, merchant, and follower NPC roles are test-covered",
    "town exit F interaction blocks before quest and opens the dungeon after quest acceptance",
    "town F interactions autosave quest acceptance and merchant gold changes to the default slot",
    "UMG widget bases drive main menu, world map, quest, trade, inventory, dungeon, and battle rule APIs",
    "playable HUD commands drive start, region clicks, town actions, dungeon nodes, battle, Boss clear, and Tanjiang unlock",
    "playable HUD autosaves route progress so Start and Continue can restore cleared unlocks",
    "default GameMode wires MVP PlayerController, clickable HUD, and Paper2D town pawn for Play mode",
    "Qingshan town map has fixed Landscape terrain, boundaries, PlayerStart, lighting, no Water actors, and MVP GameMode override",
    "selected hero, follower, and merchant walk sprite sheets are stored in project content and imported as Texture2D assets",
    "hero deep-blue high-ponytail sheet expands into 8-direction Paper2D walk flipbooks and the town pawn loads South walk by default",
    "town pawn switches hero Paper2D walk flipbooks across 8 directions from WASD, arrow-key, and axis movement intent",
    "hero PaperZD flipbook animation source, AnimBP, and 8-direction directional walk sequence reference the Paper2D flipbooks",
]


def find_ue_root() -> Path:
    for candidate in UE_CANDIDATES:
        if (candidate / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe").exists():
            return candidate
    raise RuntimeError("Cannot find UE 5.8. Checked: " + ", ".join(str(p) for p in UE_CANDIDATES))


def default_report_path() -> Path:
    timestamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    return PROJECT_ROOT / "Saved" / "HarnessReports" / f"gamexxk-mvp-playtest-{timestamp}.json"


def run_command(command: list[str], cwd: Path, timeout: int) -> tuple[int, str]:
    process = subprocess.run(
        command,
        cwd=str(cwd),
        text=True,
        encoding="utf-8",
        errors="replace",
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
    )
    return process.returncode, process.stdout or ""


def build_project(ue_root: Path, timeout: int) -> tuple[int, str]:
    build_bat = ue_root / "Engine" / "Build" / "BatchFiles" / "Build.bat"
    return run_command(
        [
            str(build_bat),
            BUILD_TARGET,
            "Win64",
            BUILD_CONFIG,
            f"-Project={UPROJECT.as_posix()}",
            "-WaitMutex",
            "-NoHotReloadFromIDE",
        ],
        cwd=PROJECT_ROOT,
        timeout=timeout,
    )


def run_automation(ue_root: Path, timeout: int) -> tuple[int, str]:
    editor_cmd = ue_root / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"
    exec_cmds = f"Automation RunTests {TEST_FILTER};Quit"
    return run_command(
        [
            str(editor_cmd),
            str(UPROJECT),
            "-Unattended",
            "-NoSplash",
            "-NoSound",
            "-NullRHI",
            f"-ExecCmds={exec_cmds}",
            "-TestExit=Automation Test Queue Empty",
            "-log",
            "-stdout",
            "-FullStdOutLogOutput",
        ],
        cwd=PROJECT_ROOT,
        timeout=timeout,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--skip-build", action="store_true", help="Run automation without compiling first.")
    parser.add_argument("--build-timeout", type=int, default=600)
    parser.add_argument("--test-timeout", type=int, default=600)
    parser.add_argument("--report", type=Path, default=default_report_path())
    args = parser.parse_args()

    report: dict[str, object] = {
        "ok": False,
        "project": str(UPROJECT),
        "test_filter": TEST_FILTER,
        "checklist": CHECKLIST,
        "steps": [],
    }

    try:
        ue_root = find_ue_root()
        report["ue_root"] = str(ue_root)

        if not args.skip_build:
            code, output = build_project(ue_root, args.build_timeout)
            report["steps"].append({"name": "build", "exit_code": code, "tail": output[-4000:]})
            if code != 0:
                report["error"] = "build failed"
                return 1

        code, output = run_automation(ue_root, args.test_timeout)
        report["steps"].append({"name": "automation", "exit_code": code, "tail": output[-8000:]})
        failed_markers = ["Automation Test Failed", "Error:", "No automation tests matched"]
        report["ok"] = code == 0 and not any(marker in output for marker in failed_markers)
        if not report["ok"]:
            report["error"] = "automation playtest failed"
            return 1
        return 0
    except Exception as exc:
        report["error"] = str(exc)
        return 1
    finally:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
        print(json.dumps(report, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    sys.exit(main())
