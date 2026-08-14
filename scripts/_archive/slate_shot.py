#!/usr/bin/env python3
"""Slate screenshot that works for embedded-viewport PIE: snapshot the editor
window, then screenshot the deepest image ref inside it."""

from __future__ import annotations

import base64
import json
import re
import sys

from ue_mcp_client import UnrealMCPClient

SLATE = "SlateInspectorToolset.SlateInspectorToolset"
REF_PATTERN = re.compile(r"\[ref=([^\]]+)\]")


def main() -> int:
    out = sys.argv[1] if len(sys.argv) > 1 else "D:/UE5 demo/GameXXK/Saved/Codex/reward_repro_slate.png"
    client = UnrealMCPClient(timeout=120.0)
    if not client.connect():
        print(json.dumps({"ok": False, "error": "mcp_connect_failed"}, ensure_ascii=False))
        return 1

    # 1. Find the editor window.
    root = str(client.call_tool(
        "Snapshot", {"ref": "", "maxDepth": 3, "bIncludeSourceLocations": False},
        toolset_name=SLATE, timeout=client.timeout))
    window_refs = re.findall(r'window "[^"]*"[\s\S]*?\[ref=([^\]]+)\]', root[:4000])
    if not window_refs:
        print(json.dumps({"ok": False, "error": "no_window", "snapshot_head": root[:600]}, ensure_ascii=False))
        return 2

    # 2. Deep snapshot the first window and try every image ref bottom-up.
    candidates = []
    for wref in window_refs:
        snap = str(client.call_tool(
            "Snapshot", {"ref": wref, "maxDepth": 8, "bIncludeSourceLocations": False},
            toolset_name=SLATE, timeout=client.timeout))
        for m in REF_PATTERN.finditer(snap):
            candidates.append(m.group(1))
    if not candidates:
        print(json.dumps({"ok": False, "error": "no_image_refs"}, ensure_ascii=False))
        return 3

    saved = None
    for ref in reversed(candidates):
        payload = client.call_tool("Screenshot", {"ref": ref}, toolset_name=SLATE, timeout=client.timeout)
        if not isinstance(payload, dict):
            continue
        encoded = payload.get("data") or ""
        mime = str(payload.get("mimeType", ""))
        if "png" not in mime.lower() or not encoded:
            continue
        data = base64.b64decode(encoded, validate=True)
        if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
            continue
        with open(out, "wb") as fh:
            fh.write(data)
        saved = {"ref": ref, "bytes": len(data)}
        break
    if not saved:
        print(json.dumps({"ok": False, "error": "no_png_screenshot", "candidates": candidates[-6:]}, ensure_ascii=False))
        return 4
    print(json.dumps({"ok": True, **saved, "out": out}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
