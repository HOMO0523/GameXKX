#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from gamexxk_content_assembly_check import parse_stdout_json
from ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, DEFAULT_PORT, UnrealMCPClient

PROJECT_ROOT = Path(__file__).resolve().parents[1]
IMPORTER = "Content/Python/gamexxk_import_battle_ui_assets.py"
VALIDATOR = "Content/Python/gamexxk_validate_battle_ui_assets.py"


def main() -> int:
    parser = argparse.ArgumentParser(description="Import GameXXK battle UI textures through UE MCP")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--path", default=DEFAULT_PATH)
    parser.add_argument("--timeout", type=float, default=240.0)
    parser.add_argument(
        "--report",
        type=Path,
        default=PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-battle-ui-assets-apply.json",
    )
    args = parser.parse_args()

    client = UnrealMCPClient(host=args.host, port=args.port, path=args.path, timeout=args.timeout)
    result: dict[str, Any] = {
        "ok": False,
        "endpoint": client.endpoint,
        "importer": IMPORTER,
        "validator": VALIDATOR,
    }
    try:
        if not client.connect():
            raise RuntimeError(f"Cannot connect to UE MCP at {client.endpoint}")
        import_response = client.run_project_python_file(IMPORTER)
        validate_response = client.run_project_python_file(VALIDATOR)
        result["import"] = parse_stdout_json(str(import_response.get("stdout", "")))
        result["validation"] = parse_stdout_json(str(validate_response.get("stdout", "")))
        result["ok"] = bool(result["import"].get("ok")) and bool(result["validation"].get("ok"))
    except Exception as exc:
        result["error"] = str(exc)

    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
