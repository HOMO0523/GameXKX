#!/usr/bin/env python3
"""Import and validate the GameXXK body font through UE MCP.

Launches an unattended MCP editor when none is listening, runs
Content/Python/gamexxk_import_body_font.py followed by
Content/Python/gamexxk_validate_body_font.py, and writes a harness report.

Usage:
    python scripts/gamexxk_body_font_apply.py             # launch editor if needed
    python scripts/gamexxk_body_font_apply.py --no-launch # require a running MCP editor
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent))

from ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, DEFAULT_PORT, UnrealMCPClient  # noqa: E402
import ue_tdd_pipeline  # noqa: E402

PROJECT_ROOT = Path(__file__).resolve().parents[1]
IMPORTER = "Content/Python/gamexxk_import_body_font.py"
VALIDATOR = "Content/Python/gamexxk_validate_body_font.py"
MARKER = "GAMEXXK_BODY_FONT_RESULT="
VALIDATE_MARKER = "GAMEXXK_BODY_FONT_VALIDATE="


def marker_json(stdout: str, marker: str) -> dict[str, Any]:
    for line in (stdout or "").splitlines():
        if line.startswith(marker):
            return json.loads(line[len(marker) :])
    return {"ok": False, "error": f"marker {marker!r} not found in stdout", "stdout": stdout}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--path", default=DEFAULT_PATH)
    parser.add_argument("--timeout", type=float, default=300.0)
    parser.add_argument("--no-launch", action="store_true", help="require an already running MCP editor")
    parser.add_argument(
        "--report",
        type=Path,
        default=PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-body-font-apply.json",
    )
    args = parser.parse_args()

    client = UnrealMCPClient(host=args.host, port=args.port, path=args.path, timeout=args.timeout)
    result: dict[str, Any] = {
        "ok": False,
        "endpoint": client.endpoint,
        "importer": IMPORTER,
        "validator": VALIDATOR,
        "launched_editor": False,
    }
    try:
        if not client.connect():
            if args.no_launch:
                raise RuntimeError(f"Cannot connect to UE MCP at {client.endpoint}")
            if not ue_tdd_pipeline.kill_editor():
                raise RuntimeError("Could not confirm this project's editor is closed before launch")
            time.sleep(2)
            if ue_tdd_pipeline.launch_editor(mcp_port=args.port) is None:
                raise RuntimeError("Editor launch failed")
            result["launched_editor"] = True
            client = ue_tdd_pipeline.wait_for_mcp(host=args.host, port=args.port, path=args.path)
            if client is None:
                raise RuntimeError("UE MCP never became ready")

        import_response = client.run_project_python_file(IMPORTER)
        result["import"] = marker_json(str(import_response.get("stdout", "")), MARKER)
        validate_response = client.run_project_python_file(VALIDATOR)
        result["validation"] = marker_json(str(validate_response.get("stdout", "")), VALIDATE_MARKER)
        result["ok"] = bool(result["import"].get("ok")) and bool(result["validation"].get("ok"))
        if not result["ok"]:
            result["error"] = "import or validation reported failure"
    except Exception as exc:  # noqa: BLE001 - the report is the product
        result["error"] = str(exc)

    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
