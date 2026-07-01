#!/usr/bin/env python3
"""Validate MVP hero PaperZD assets through UE MCP."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from gamexxk_content_assembly_check import parse_stdout_json
from ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, DEFAULT_PORT, UnrealMCPClient


PROJECT_ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = "Content/Python/gamexxk_validate_paperzd_character.py"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--path", default=DEFAULT_PATH)
    parser.add_argument("--report", type=Path, default=PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-paperzd-character-check.json")
    args = parser.parse_args()

    client = UnrealMCPClient(host=args.host, port=args.port, path=args.path, timeout=180.0)
    report: dict[str, Any] = {
        "ok": False,
        "endpoint": client.endpoint,
        "validator": VALIDATOR,
    }
    try:
        if not client.connect():
            raise RuntimeError(f"Cannot connect to UE MCP at {client.endpoint}")
        response = client.run_project_python_file(VALIDATOR)
        validation = parse_stdout_json(str(response.get("stdout", "")))
        report["validation"] = validation
        report["ok"] = bool(validation.get("ok"))
    except Exception as exc:
        report["error"] = str(exc)

    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
