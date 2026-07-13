"""Report dimensions for the complete-structure candidate meshes in Asian Village."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


PATHS = (
    "/Game/Asian_Village/meshes/building/SM_foundation_01.SM_foundation_01",
    "/Game/Asian_Village/meshes/building/SM_floor_01.SM_floor_01",
    "/Game/Asian_Village/meshes/building/SM_wall_01.SM_wall_01",
    "/Game/Asian_Village/meshes/building/SM_wall_window_01.SM_wall_window_01",
    "/Game/Asian_Village/meshes/building/SM_wall_door_01.SM_wall_door_01",
    "/Game/Asian_Village/meshes/building/SM_roof_03.SM_roof_03",
    "/Game/Asian_Village/meshes/building/SM_roof_canopy_01.SM_roof_canopy_01",
    "/Game/Asian_Village/meshes/building/SM_bridge_01.SM_bridge_01",
    "/Game/Asian_Village/meshes/props/SM_market_tent_01.SM_market_tent_01",
    "/Game/Asian_Village/meshes/trees/SM_tree_01.SM_tree_01",
    "/Game/Asian_Village/meshes/trees/SM_bamboo_01.SM_bamboo_01",
)
OUT = Path(__file__).resolve().parents[2] / "docs/production/evidence/asian-village-integration/mesh-bounds.json"


def main() -> None:
    entries = []
    for path in PATHS:
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if mesh is None:
            raise RuntimeError(f"missing mesh {path}")
        bounds = mesh.get_bounds()
        entries.append({
            "path": path,
            "size_cm": [round(float(axis) * 2.0, 3) for axis in (
                bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z)],
            "origin_cm": [round(float(axis), 3) for axis in (
                bounds.origin.x, bounds.origin.y, bounds.origin.z)],
        })
    report = {"ok": True, "meshes": entries}
    OUT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("[GAMEXXK] " + json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
