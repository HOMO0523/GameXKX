"""Transient driver: chunked full 2K rollout of all 138 production atlases."""

import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, UnrealMCPClient  # noqa: E402

PORT = 12345
CHUNK = 20
TOTAL = 138


def _run_python(client, relative_path, argv=None):
    return client.run_project_python_file(relative_path, argv or [], True)


def main() -> int:
    client = UnrealMCPClient(host=DEFAULT_HOST, port=PORT, path=DEFAULT_PATH, timeout=1800.0)
    deadline = time.time() + 300
    while time.time() < deadline:
        if client.connect():
            break
        time.sleep(5)
    if not client.connect():
        print(json.dumps({"ok": False, "error": "mcp_timeout"}))
        return 1

    out = {"chunks": []}
    offset = 0
    while offset < TOTAL:
        result = _run_python(
            client,
            "Content/Python/gamexxk_import_battle_animation_production.py",
            ["--two-k", "--offset", str(offset), "--limit", str(CHUNK)],
        )
        payload = (result or {}).get("stdout") or ""
        try:
            data = json.loads(payload)
            chunk_ok = bool(data.get("ok"))
            imported = len(data.get("imported") or [])
        except Exception:
            chunk_ok = False
            imported = -1
        out["chunks"].append({"offset": offset, "ok": chunk_ok, "imported": imported, "tail": payload[-160:]})
        print(f"[rollout] offset={offset} ok={chunk_ok} imported={imported}", flush=True)
        if not chunk_ok or imported <= 0:
            break
        offset += CHUNK
        client.collect_garbage(full_purge=True)
    print(json.dumps(out, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
