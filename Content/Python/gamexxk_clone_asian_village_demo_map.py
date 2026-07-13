"""Duplicate the complete vendor demo map into the GameXXK map namespace.

Run only in an editor commandlet; it deliberately does not load the resulting
world, avoiding the full-scene viewport memory spike during migration.
"""

from __future__ import annotations

import json
import os
from pathlib import Path

import unreal


SOURCE = "/Game/Asian_Village/maps/Asian_Village_Demo"
TARGET = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
OUT = Path(os.environ["GAMEXXK_ASIAN_DEMO_CLONE_REPORT"])


def main() -> None:
    if not unreal.EditorAssetLibrary.does_asset_exist(SOURCE):
        raise RuntimeError(f"source demo map is absent: {SOURCE}")
    existed = unreal.EditorAssetLibrary.does_asset_exist(TARGET)
    if not existed:
        if not unreal.EditorAssetLibrary.does_directory_exist("/Game/GameXXK/Maps/Prototype"):
            unreal.EditorAssetLibrary.make_directory("/Game/GameXXK/Maps/Prototype")
        duplicated = unreal.EditorAssetLibrary.duplicate_asset(SOURCE, TARGET)
        if duplicated is None:
            raise RuntimeError(f"duplicate failed: {SOURCE} -> {TARGET}")
        if not unreal.EditorAssetLibrary.save_loaded_asset(duplicated, True):
            raise RuntimeError(f"could not save duplicated demo map: {TARGET}")
    if not unreal.EditorAssetLibrary.does_asset_exist(TARGET):
        raise RuntimeError(f"target map was not created: {TARGET}")
    report = {"ok": True, "source": SOURCE, "target": TARGET, "already_existed": existed}
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("[GAMEXXK] " + json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
