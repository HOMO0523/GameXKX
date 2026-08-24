#!/usr/bin/env python3
"""Import and validate the approved gem textures through project UE MCP."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from ue_mcp_client import UnrealMCPClient


PROJECT_ROOT = Path(__file__).resolve().parents[1]
IMPORTER = "Content/Python/gamexxk_import_gem_icons.py"
VALIDATOR = "Content/Python/gamexxk_validate_gem_icons.py"


def parse_result(value: object) -> dict[str, object]:
    if isinstance(value, dict) and "stdout" in value:
        value = value["stdout"]
    elif isinstance(value, dict):
        return value
    if not isinstance(value, str):
        raise RuntimeError(f"unexpected UE Python result: {value!r}")
    for line in reversed(value.splitlines()):
        try:
            parsed = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(parsed, dict):
            return parsed
    raise RuntimeError("UE Python result did not contain JSON")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--timeout", type=float, default=240.0)
    args = parser.parse_args()
    client = UnrealMCPClient(timeout=args.timeout)
    client.require_connected()
    if client.is_in_pie():
        raise RuntimeError("stop PIE before importing gem textures")
    imported = parse_result(client.run_project_python_file(IMPORTER))
    validated = parse_result(client.run_project_python_file(VALIDATOR))
    if imported.get("imported_count") != 30 or not validated.get("ok"):
        raise RuntimeError(json.dumps({"imported": imported, "validated": validated}, ensure_ascii=False))
    print(json.dumps({"ok": True, "imported": imported, "validated": validated}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
