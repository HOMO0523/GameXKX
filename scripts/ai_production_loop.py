#!/usr/bin/env python3
"""Run the lightweight GameXXK production loop.

Gate order follows docs/design/agent-operating-guide.md:

1. Production state validation (units + loose reports)
2. git diff --check
3. Script self-tests (optional, no UE needed)
4. Cold UBT build (optional)
5. Automation suite via UnrealEditor-Cmd (optional, index.json is authority)
6. Real PIE playable flow through UE MCP (optional)
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_DIR = PROJECT_ROOT / "scripts"
REPORT_ROOT = PROJECT_ROOT / "Saved" / "HarnessReports"
UPROJECT = PROJECT_ROOT / "GameXXK.uproject"

sys.path.insert(0, str(SCRIPT_DIR))
import ue_paths  # noqa: E402

BUILD_TARGET = "GameXXKEditor"
BUILD_CONFIG = "Development"
DEFAULT_SCRIPT_TESTS = (
    "test_harness_state_validator.py",
    "test_ue_tdd_pipeline.py",
    "test_parse_automation_index.py",
)
SCRIPT_TEST_MANIFEST = SCRIPT_DIR / "script-test-manifest.json"
SCRIPT_TEST_TAGS = ("headless", "asset-contract", "mcp-live")


def run_step(
    name: str,
    command: list[str],
    pass_codes: tuple[int, ...] = (0,),
    timeout: float | None = None,
) -> dict:
    try:
        result = subprocess.run(
            command,
            cwd=PROJECT_ROOT,
            capture_output=True,
            text=True,
            timeout=timeout,
            encoding="utf-8",
            errors="replace",
        )
    except subprocess.TimeoutExpired:
        return {
            "name": name,
            "command": command,
            "returncode": -1,
            "ok": False,
            "stdout": "",
            "stderr": f"step timed out after {timeout}s",
        }
    return {
        "name": name,
        "command": command,
        "returncode": result.returncode,
        "ok": result.returncode in pass_codes,
        "stdout": result.stdout,
        "stderr": result.stderr,
    }


def write_report(results: list[dict]) -> Path:
    REPORT_ROOT.mkdir(parents=True, exist_ok=True)
    path = REPORT_ROOT / f"{time.strftime('%Y%m%d-%H%M%S')}-ai-production-loop.md"
    lines = [
        "# GameXXK Production Loop Report",
        "",
        f"- generated_at: {time.strftime('%Y-%m-%d %H:%M:%S')}",
        "",
        "| Step | Result | Exit |",
        "|---|---:|---:|",
    ]
    for item in results:
        lines.append(f"| {item['name']} | {'PASS' if item['ok'] else 'FAIL'} | {item['returncode']} |")
    lines.extend(["", "## Details", ""])
    for item in results:
        lines.extend([
            f"### {item['name']}",
            "",
            "```text",
            " ".join(item["command"]),
            "```",
            "",
        ])
        stdout = item.get("stdout") or ""
        stderr = item.get("stderr") or ""
        if stdout.strip():
            lines.extend(["stdout:", "", "```text", stdout.strip(), "```", ""])
        if stderr.strip():
            lines.extend(["stderr:", "", "```text", stderr.strip(), "```", ""])
    path.write_text("\n".join(lines), encoding="utf-8")
    return path


def _ue_root(args) -> Path | None:
    if args.ue_root:
        candidate = Path(args.ue_root)
        if not ue_paths.is_valid_ue_root(candidate):
            raise RuntimeError(f"--ue-root is not a valid UE root: {args.ue_root}")
        return candidate
    return ue_paths.find_ue_root()


def run_ubt_step(ue_root: Path) -> dict:
    command = [
        str(ue_paths.ue_build_bat(ue_root)),
        BUILD_TARGET,
        "Win64",
        BUILD_CONFIG,
        f"-Project={UPROJECT.as_posix()}",
        "-WaitMutex",
        "-NoHotReload",
        "-NoHotReloadFromIDE",
    ]
    return run_step(f"Cold UBT build ({BUILD_TARGET})", command, timeout=3600)


def run_script_test_step(test_file: str, tag: str = "focused") -> dict:
    environment = os.environ.copy()
    environment["PYTHONPATH"] = os.pathsep.join(
        filter(None, [str(PROJECT_ROOT), environment.get("PYTHONPATH", "")])
    )
    command = [sys.executable, str(SCRIPT_DIR / test_file)]
    try:
        result = subprocess.run(
            command,
            cwd=PROJECT_ROOT,
            capture_output=True,
            text=True,
            timeout=600,
            env=environment,
            encoding="utf-8",
            errors="replace",
        )
    except subprocess.TimeoutExpired:
        return {
            "name": f"Script self-test [{tag}]: {test_file}",
            "command": command,
            "returncode": -1,
            "ok": False,
            "stdout": "",
            "stderr": "step timed out after 600s",
        }
    return {
        "name": f"Script self-test [{tag}]: {test_file}",
        "command": command,
        "returncode": result.returncode,
        "ok": result.returncode == 0,
        "stdout": result.stdout,
        "stderr": result.stderr,
    }


def run_automation_step(ue_root: Path, tests: str, report_name: str, fail_on_warnings: bool) -> dict:
    editor_cmd = ue_paths.ue_editor_cmd_exe(ue_root)
    report_dir = PROJECT_ROOT / "Saved" / "Automation" / report_name
    command = [
        str(editor_cmd),
        UPROJECT.as_posix(),
        "-Unattended",
        "-NoSound",
        "-NullRHI",
        "-NoSplash",
        "-NoPause",
        f"-ReportOutputPath={report_dir.as_posix()}",
        f"-ExecCmds=Automation RunTests {tests}; Quit",
    ]
    step = run_step(f"Automation tests ({tests})", command, timeout=7200)
    index_path = report_dir / "index.json"
    if not index_path.is_file():
        step["ok"] = False
        step["stdout"] = (step.get("stdout") or "") + f"\nMISSING report index: {index_path}"
        return step

    parse_command = [
        sys.executable,
        str(SCRIPT_DIR / "parse_automation_index.py"),
        "--index",
        str(index_path),
        "--json",
    ]
    if fail_on_warnings:
        parse_command.append("--fail-on-warnings")
    parsed = subprocess.run(
        parse_command,
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        timeout=120,
        encoding="utf-8",
        errors="replace",
    )
    summary: dict | None = None
    try:
        summary = json.loads(parsed.stdout.strip().splitlines()[-1])
    except (json.JSONDecodeError, IndexError):
        summary = None
    step["ok"] = step["ok"] and parsed.returncode == 0 and bool(summary and summary.get("ok"))
    step["stdout"] = (
        (step.get("stdout") or "") + "\n" + (parsed.stdout or "") + (parsed.stderr or "")
    )
    return step


def load_script_test_manifest() -> dict:
    manifest = json.loads(SCRIPT_TEST_MANIFEST.read_text(encoding="utf-8"))
    if manifest.get("schema") != 1 or set(manifest.get("tags", {})) != set(SCRIPT_TEST_TAGS):
        raise RuntimeError(f"invalid script test manifest: {SCRIPT_TEST_MANIFEST}")
    for tag in SCRIPT_TEST_TAGS:
        if not isinstance(manifest["tags"][tag], list) or any(
            not isinstance(name, str) or not name.endswith(".py")
            for name in manifest["tags"][tag]
        ):
            raise RuntimeError(f"script test tag entries must be .py filenames: {tag}")
    names = [
        name
        for tag in ("asset-contract", "mcp-live")
        for name in manifest["tags"][tag]
    ]
    if len(names) != len(set(names)):
        raise RuntimeError("script test tags overlap")
    if any(not isinstance(name, str) for name in names):
        raise RuntimeError("script test tag entries must be filenames")
    return manifest


def discover_script_tests(tag: str = "headless") -> list[str]:
    """Discover tests for a tag; unlisted tests are headless by default."""
    manifest = load_script_test_manifest()
    discovered = {
        path.name
        for path in SCRIPT_DIR.glob("test_*.py")
        if path.is_file() and "_archive" not in path.parts
    }
    tagged = set(manifest["tags"]["asset-contract"]) | set(manifest["tags"]["mcp-live"])
    missing = sorted(tagged - discovered)
    if missing:
        raise RuntimeError(f"script test manifest references missing files: {missing}")
    if tag == "headless":
        return sorted(discovered - tagged)
    if tag not in SCRIPT_TEST_TAGS:
        raise ValueError(f"unknown script test tag: {tag}")
    return sorted(manifest["tags"][tag])


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--require-units", action="store_true")
    parser.add_argument("--run-real-flow", action="store_true", help="Run the UE MCP real PIE flow harness")
    parser.add_argument("--run-script-tests", action="store_true", help="Run the headless script self-tests")
    parser.add_argument("--script-tests", default=",".join(DEFAULT_SCRIPT_TESTS), help="Comma-separated script test files, or 'all' to run the manifest-defined headless set")
    parser.add_argument("--script-test-tag", choices=SCRIPT_TEST_TAGS, help="Run all tests in one manifest tag")
    parser.add_argument("--run-ubt", action="store_true", help="Run the cold UBT build")
    parser.add_argument("--run-automation", action="store_true", help="Run the automation suite via UnrealEditor-Cmd")
    parser.add_argument("--automation-tests", default="GameXXK", help="Automation test filter (default: GameXXK)")
    parser.add_argument("--automation-report", default="AiProductionLoop", help="Report directory name under Saved/Automation")
    parser.add_argument("--fail-on-automation-warnings", action="store_true")
    parser.add_argument("--ue-root", default=None, help="Override the UE root (defaults to GAMEXXK_UE_ROOT / candidates / registry)")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)

    results: list[dict] = []
    validator = [sys.executable, "scripts/harness_state_validator.py", "--json"]
    if args.require_units:
        validator.append("--require-units")
    results.append(run_step("Production state validation", validator))
    results.append(run_step("Diff whitespace check", ["git", "diff", "--check"]))

    need_ue = args.run_ubt or args.run_automation or args.run_real_flow
    if need_ue:
        try:
            ue_root = _ue_root(args)
        except RuntimeError as exc:
            results.append({
                "name": "UE root resolution",
                "command": ["ue_paths.find_ue_root()"],
                "returncode": 1,
                "ok": False,
                "stdout": "",
                "stderr": str(exc),
            })
            ue_root = None
    else:
        ue_root = None

    if args.run_script_tests:
        if args.script_test_tag:
            test_files = discover_script_tests(args.script_test_tag)
            test_tag = args.script_test_tag
        elif args.script_tests.strip().lower() == "all":
            test_files = discover_script_tests("headless")
            test_tag = "headless"
        else:
            test_files = [name.strip() for name in args.script_tests.split(",") if name.strip()]
            test_tag = "focused"
        for test_file in test_files:
            results.append(run_script_test_step(test_file, test_tag))

    if args.run_ubt and ue_root is not None:
        results.append(run_ubt_step(ue_root))

    if args.run_automation and ue_root is not None:
        results.append(
            run_automation_step(ue_root, args.automation_tests, args.automation_report, args.fail_on_automation_warnings)
        )

    if args.run_real_flow:
        results.append(run_step("Real PIE playable flow", [sys.executable, "scripts/gamexxk_real_play_flow_mcp.py"], timeout=7200))

    report_path = write_report(results)
    summary = {"ok": all(item["ok"] for item in results), "report": str(report_path), "results": results}
    if args.json:
        print(json.dumps(summary, ensure_ascii=True, indent=2))
    else:
        print(json.dumps({"ok": summary["ok"], "report": summary["report"]}, ensure_ascii=True))
    return 0 if summary["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
