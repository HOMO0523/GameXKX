"""Run the V2 town-terrain capture only inside UnrealEditor-Cmd.

This commandlet entry point deliberately owns no UE assets and never attaches a
material to the battle floor.  It exists because loading the source Landscape
through the user's live editor triggers a UE 5.8 render-thread assertion.
"""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path
from typing import Any


SCRIPT_DIRECTORY = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIRECTORY.parents[1]
V2_MANIFEST = (
    PROJECT_ROOT
    / "SourceAssets"
    / "PartyDeck"
    / "battle-town-terrain"
    / "battle-town-terrain-manifest-v2.json"
)
DEFAULT_CAPTURE_REPORT = (
    PROJECT_ROOT / "Saved" / "HarnessReports" / "battle-town-v2-commandlet-dryrun.json"
)
CAPTURE_REPORT_ENV = "GAMEXXK_BATTLE_TERRAIN_CAPTURE_REPORT"


def _capture_report_path() -> Path:
    configured = os.environ.get(CAPTURE_REPORT_ENV, "").strip()
    return Path(configured) if configured else DEFAULT_CAPTURE_REPORT


def main() -> dict[str, Any]:
    """Run a V2 capture without creating or binding any Content asset."""
    script_directory = str(SCRIPT_DIRECTORY)
    if script_directory not in sys.path:
        sys.path.insert(0, script_directory)

    # The audit module uses this marker to restore from its transient commandlet
    # world to the battle map, never to the map open in the user's editor.
    os.environ["GAMEXXK_BATTLE_TERRAIN_AUDIT_COMMANDLET"] = "1"

    import gamexxk_bake_battle_town_terrain_v2 as pipeline

    result = pipeline.main(
        [
            "--manifest",
            str(V2_MANIFEST),
            "--dry-run-capture",
            "--capture-report",
            str(_capture_report_path()),
        ]
    )
    print(json.dumps({"ok": True, **result}, ensure_ascii=False, sort_keys=True))
    return result


if __name__ == "__main__":
    main()
