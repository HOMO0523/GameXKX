"""Save via project MCP, close only this project, then perform a real cold UBT check."""
import json
import sys
import time
from pathlib import Path

import ue_tdd_pipeline as pipeline
from ue_mcp_client import UnrealMCPClient

root = Path(__file__).resolve().parents[1]
report_dir = root / "Saved/Codex/EssentialSfx-20260909"
report_dir.mkdir(parents=True, exist_ok=True)
client = UnrealMCPClient(timeout=30)
running = pipeline.is_editor_running()
if running:
    if not client.connect():
        raise SystemExit("MCP unavailable; editor was not closed.")
    saved = client.save_dirty_packages()
    print("SAVED_BEFORE_CLOSE " + json.dumps(saved), flush=True)
    if not saved.get("save_result") or saved.get("dirty_after"):
        raise SystemExit("Dirty packages remain; editor was not closed.")
    if client.is_in_pie():
        client.stop_pie()
        if not client.wait_for_pie_state(False, timeout=30):
            raise SystemExit("PIE did not stop; editor was not closed.")
    saved = client.save_dirty_packages()
    if not saved.get("save_result") or saved.get("dirty_after"):
        raise SystemExit("Post-PIE save failed; editor was not closed.")
    (report_dir / "save-before-build.json").write_text(json.dumps(saved, indent=2), encoding="utf-8")
    try:
        client.execute_console_command("QUIT_EDITOR")
    except Exception as exc:
        print("Graceful editor exit response: " + str(exc), flush=True)
    for _ in range(30):
        if not pipeline._project_editor_pids():
            break
        time.sleep(0.5)
    if pipeline._project_editor_pids():
        raise SystemExit("Editor has not exited after its save; refusing to compile over the live editor.")
raise SystemExit(0 if pipeline.build_project() else 1)
