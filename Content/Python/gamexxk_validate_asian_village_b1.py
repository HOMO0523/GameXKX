"""Validate the reversible Eastern Village layer currently placed in B1."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


B1_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1"
TAG = "QingshanAsianVillageIntegrated"
OUT = Path(__file__).resolve().parents[2] / "docs/production/evidence/asian-village-integration/b1-validation-report.json"


def main() -> None:
    world = unreal.EditorLevelLibrary.get_editor_world()
    current = "" if world is None else str(world.get_outermost().get_path_name()).split(".", 1)[0]
    if current != B1_MAP:
        raise RuntimeError(f"expected open B1 map, got {current}")
    actors = [
        actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
        if TAG in {str(tag) for tag in actor.get_editor_property("tags")}
    ]
    labels = [str(actor.get_actor_label()) for actor in actors]
    expected_templates = {
        "AV_B1_CoreThatchedShop_": 35,
        "AV_B1_CoreCanopyShop_": 33,
        "AV_B1_ApproachLowHouse_": 23,
        "AV_B1_BridgeHouse_": 41,
        "AV_B1_SouthTower_": 38,
    }
    expected_total = 14 + sum(expected_templates.values())
    if len(labels) != expected_total or len(set(labels)) != expected_total:
        raise RuntimeError(f"expected {expected_total} unique integrated actors, got {len(labels)}/{len(set(labels))}")
    for prefix, expected_count in expected_templates.items():
        count = sum(label.startswith(prefix) for label in labels)
        if count != expected_count:
            raise RuntimeError(f"expected {expected_count} parts for {prefix}, got {count}")
    required = {"AV_B1_Bridge_Main", "AV_B1_MarketTent_West", "AV_B1_MarketTent_East", "AV_B1_Tree_Approach"}
    if not required.issubset(labels):
        raise RuntimeError(f"missing required integration actors: {sorted(required - set(labels))}")
    protected = {}
    for label in ("QingshanInn_TownExit", "PlayerStart_QingshanInn"):
        matches = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if str(actor.get_actor_label()) == label]
        if len(matches) != 1:
            raise RuntimeError(f"protected actor {label} has {len(matches)} matches")
        location = matches[0].get_actor_location()
        protected[label] = [round(float(location.x), 3), round(float(location.y), 3), round(float(location.z), 3)]
    report = {"ok": True, "map": current, "integrated_actor_count": len(labels), "protected_locations": protected}
    OUT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("[GAMEXXK] " + json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
