"""Transient driver: Slate-level screenshot of the PIE viewport (window-independent)."""

import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from gamexxk_real_play_flow_mcp import (  # noqa: E402
    SLATE_TOOLSET,
    _decode_slate_screenshot_png,
    _slate_preview_window_ref,
)
from ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, UnrealMCPClient  # noqa: E402

PORT = 12345


def _run_python(client, relative_path, argv=None):
    return client.run_project_python_file(relative_path, argv or [], True)


def main() -> int:
    client = UnrealMCPClient(host=DEFAULT_HOST, port=PORT, path=DEFAULT_PATH, timeout=300.0)
    deadline = time.time() + 240
    while time.time() < deadline:
        if client.connect():
            break
        time.sleep(5)
    if not client.connect():
        print(json.dumps({"ok": False, "error": "mcp_timeout"}))
        return 1

    out = {}
    client.stop_pie()
    time.sleep(3.0)
    client.start_pie(warmup_seconds=2.0)
    time.sleep(6.0)
    _run_python(client, "Content/Python/gamexxk_pilot_2k_session.py", ["real_entry"])
    time.sleep(3.0)
    _run_python(client, "Content/Python/gamexxk_pilot_2k_session.py", ["fixture"])
    time.sleep(4.0)

    root_snapshot = str(client.call_tool(
        "Snapshot", {"ref": "", "maxDepth": 3, "bIncludeSourceLocations": False},
        toolset_name=SLATE_TOOLSET, timeout=client.timeout,
    ))
    preview_ref = _slate_preview_window_ref(root_snapshot)
    out["preview_ref"] = preview_ref
    if not preview_ref:
        print(json.dumps(out, ensure_ascii=False, indent=2))
        return 2
    payload = client.call_tool("Screenshot", {"ref": preview_ref}, toolset_name=SLATE_TOOLSET, timeout=client.timeout)
    data = _decode_slate_screenshot_png(payload)
    target = ROOT / "Saved" / "Codex" / "pilot_compare_slate.png"
    target.write_bytes(data)
    out["shot"] = str(target)
    out["bytes"] = len(data)
    client.stop_pie()
    print(json.dumps(out, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
