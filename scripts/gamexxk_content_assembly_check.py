#!/usr/bin/env python3
"""Validate MVP content assembly through the project UE MCP toolset."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, DEFAULT_PORT, UnrealMCPClient


PROJECT_ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = "Content/Python/gamexxk_validate_content_assembly.py"


def parse_stdout_json(stdout: str) -> dict[str, Any]:
    lines = [line for line in (stdout or "").splitlines() if line.strip()]
    for index in range(len(lines)):
        candidate = "\n".join(lines[index:])
        try:
            parsed = json.loads(candidate)
        except json.JSONDecodeError:
            continue
        if isinstance(parsed, dict):
            return parsed
    return {"ok": False, "error": "validator did not emit a JSON object", "stdout": stdout}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--path", default=DEFAULT_PATH)
    parser.add_argument("--report", type=Path, default=PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-content-assembly-check.json")
    args = parser.parse_args()

    client = UnrealMCPClient(host=args.host, port=args.port, path=args.path, timeout=120.0)
    result: dict[str, Any] = {
        "ok": False,
        "endpoint": client.endpoint,
        "validator": VALIDATOR,
    }
    try:
        if not client.connect():
            raise RuntimeError(f"Cannot connect to UE MCP at {client.endpoint}")
        response = client.run_project_python_file(VALIDATOR)
        validation = parse_stdout_json(str(response.get("stdout", "")))
        result.update(validation)
    except Exception as exc:
        result["error"] = str(exc)

    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
