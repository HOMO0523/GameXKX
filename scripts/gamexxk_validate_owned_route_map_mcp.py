#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from gamexxk_content_assembly_check import parse_stdout_json
from ue_mcp_client import UnrealMCPClient

VALIDATOR = "Content/Python/gamexxk_validate_owned_route_map.py"


def main() -> int:
    client = UnrealMCPClient()
    if not client.connect():
        print(json.dumps({"ok": False, "error": f"Cannot connect to {client.endpoint}"}, ensure_ascii=False))
        return 2
    response = client.run_project_python_file(VALIDATOR)
    result = response if "ok" in response else parse_stdout_json(str(response.get("stdout", "")))
    if not result.get("ok") and not response.get("success", False):
        result["mcp_response"] = response
    ok = bool(result.get("ok", False))
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
