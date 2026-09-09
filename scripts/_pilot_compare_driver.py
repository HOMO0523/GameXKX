"""Transient driver: fully automated 2K/1K variant import + side-by-side PIE capture."""

import json
import shutil
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, UnrealMCPClient  # noqa: E402

PORT = 12345
IDLE_ASSET_IDS = [
    "character_00_hero_idle",
    "enemy_01_rooster_idle",
]


def _run_python(client, relative_path, argv=None):
    return client.run_project_python_file(relative_path, argv or [], True)


def _import_variants(client, flag, suffix):
    argv = [f"--{flag}", "--variant-suffix", suffix, "--textures-only"]
    for asset in IDLE_ASSET_IDS:
        argv += ["--asset-id", asset]
    return _run_python(client, "Content/Python/gamexxk_import_battle_animation_production.py", argv)


def _latest_shot() -> Path | None:
    shot_dir = ROOT / "Saved" / "Screenshots" / "WindowsEditor"
    if not shot_dir.exists():
        return None
    shots = sorted(shot_dir.glob("HighresScreenshot*.png"), key=lambda p: p.stat().st_mtime)
    return shots[-1] if shots else None


def main() -> int:
    client = UnrealMCPClient(host=DEFAULT_HOST, port=PORT, path=DEFAULT_PATH, timeout=600.0)
    deadline = time.time() + 300
    while time.time() < deadline:
        if client.connect():
            break
        time.sleep(5)
    if not client.connect():
        print(json.dumps({"ok": False, "error": "mcp_timeout"}))
        return 1

    out = {}
    out["delete_placeholders"] = (_run_python(
        client, "Content/Python/gamexxk_delete_placeholder_variants.py"
    ) or {}).get("stdout", "")[:300]
    out["import_2k"] = (_import_variants(client, "two-k", "_2k") or {}).get("stdout", "")[:300]
    out["import_1k"] = (_import_variants(client, "one-k", "_1k") or {}).get("stdout", "")[:300]
    out["sizes"] = (_run_python(client, "Content/Python/_probe_sizes_tmp.py") or {}).get("stdout", "")[:600]

    client.start_pie(warmup_seconds=2.0)
    time.sleep(6.0)
    _run_python(client, "Content/Python/gamexxk_pilot_2k_session.py", ["real_entry"])
    time.sleep(3.0)
    _run_python(client, "Content/Python/gamexxk_pilot_2k_session.py", ["fixture"])
    time.sleep(4.0)
    _run_python(client, "Content/Python/gamexxk_pilot_2k_session.py", ["idle_shot"])
    time.sleep(3.0)
    latest = _latest_shot()
    target = ROOT / "Saved" / "Codex" / "pilot_compare_side_by_side.png"
    if latest:
        shutil.copyfile(latest, target)
    out["shot"] = str(target) if latest else None
    client.stop_pie()
    print(json.dumps(out, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
