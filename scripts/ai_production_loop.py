#!/usr/bin/env python3
"""Run the lightweight GameXXK production loop."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
REPORT_ROOT = PROJECT_ROOT / "Saved" / "HarnessReports"


def run_step(name: str, command: list[str], pass_codes: tuple[int, ...] = (0,)) -> dict:
    result = subprocess.run(command, cwd=PROJECT_ROOT, capture_output=True, text=True)
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
            "```powershell",
            " ".join(item["command"]),
            "```",
            "",
        ])
        if item["stdout"].strip():
            lines.extend(["stdout:", "", "```text", item["stdout"].strip(), "```", ""])
        if item["stderr"].strip():
            lines.extend(["stderr:", "", "```text", item["stderr"].strip(), "```", ""])
    path.write_text("\n".join(lines), encoding="utf-8")
    return path


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--require-units", action="store_true")
    parser.add_argument("--run-real-flow", action="store_true", help="Run the UE MCP real PIE flow harness")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)

    results: list[dict] = []
    validator = [sys.executable, "scripts/harness_state_validator.py", "--json"]
    if args.require_units:
        validator.append("--require-units")
    results.append(run_step("Production state validation", validator))
    results.append(run_step("Diff whitespace check", ["git", "diff", "--check"]))

    if args.run_real_flow:
        results.append(run_step("Real PIE playable flow", [sys.executable, "scripts/gamexxk_real_play_flow_mcp.py"]))

    report_path = write_report(results)
    summary = {"ok": all(item["ok"] for item in results), "report": str(report_path), "results": results}
    if args.json:
        print(json.dumps(summary, ensure_ascii=False, indent=2))
    else:
        print(json.dumps({"ok": summary["ok"], "report": summary["report"]}, ensure_ascii=False))
    return 0 if summary["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
