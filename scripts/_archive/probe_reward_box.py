#!/usr/bin/env python3
"""Drive Content/Python/_probe_reward_box_geometry.py through UE MCP and print the dump."""

from __future__ import annotations

import argparse
import json
import sys

from gamexxk_content_assembly_check import parse_stdout_json
from ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, DEFAULT_PORT, UnrealMCPClient


def main() -> int:
    parser = argparse.ArgumentParser(description="Dump reward-row geometry from PIE")
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--path", default=DEFAULT_PATH)
    parser.add_argument("--timeout", type=float, default=120.0)
    args = parser.parse_args()

    client = UnrealMCPClient(host=args.host, port=args.port, path=args.path, timeout=args.timeout)
    try:
        if not client.connect():
            raise RuntimeError(f"Cannot connect to UE MCP at {client.endpoint}")
        response = client.run_project_python_file("Content/Python/_probe_reward_box_geometry.py")
        print(json.dumps(parse_stdout_json(str(response.get("stdout", ""))), ensure_ascii=False, indent=2))
        return 0
    except Exception as exc:
        print(json.dumps({"ok": False, "error": str(exc)}, ensure_ascii=False, indent=2))
        return 1


if __name__ == "__main__":
    sys.exit(main())
