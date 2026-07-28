"""V2 driver for a validated local Qingshan terrain capture.

V1 remains intact and bound until this driver completes its explicit dry-run
and execute phases.  The shared guarded baker is configured only in-memory so
this module cannot overwrite V1's three owned output assets.
"""

from __future__ import annotations

import json
import sys
from typing import Any

import gamexxk_bake_battle_town_terrain as core


OUTPUT_ROOT = "/Game/GameXXK/Environment/Battle/TownTerrainV2"
OUTPUT_MESH = OUTPUT_ROOT + "/SM_Battle_QingshanGround_02"
OUTPUT_MATERIAL = OUTPUT_ROOT + "/M_Battle_QingshanGround_02"
OUTPUT_ALBEDO = OUTPUT_ROOT + "/T_Battle_QingshanGround_Albedo_02"


def _configure_v2_output_contract() -> None:
    """Point the shared guarded writer at a separate, rollback-safe V2 root."""
    core.OUTPUT_ROOT = OUTPUT_ROOT
    core.OUTPUT_MESH = OUTPUT_MESH
    core.OUTPUT_MATERIAL = OUTPUT_MATERIAL
    core.OUTPUT_ALBEDO = OUTPUT_ALBEDO


def main(argv: list[str] | None = None) -> dict[str, Any]:
    _configure_v2_output_contract()
    return core.main(argv)


if __name__ == "__main__":
    try:
        print(json.dumps(main(sys.argv[1:]), ensure_ascii=False, sort_keys=True))
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1) from exc
