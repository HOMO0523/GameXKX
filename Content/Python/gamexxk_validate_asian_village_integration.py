"""Verify the Asian Village integration review map after editor-side creation."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


MAP_PATH = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Integration"
MANAGED_TAG = "QingshanAsianVillageReview"
EXPECTED_COUNT = 21
EVIDENCE_PATH = (
    Path(__file__).resolve().parents[2]
    / "docs/production/evidence/asian-village-integration/validation-report.json"
)


def main() -> None:
    world = unreal.EditorLevelLibrary.get_editor_world()
    current = "" if world is None else str(world.get_outermost().get_name()).split(".", 1)[0]
    if current != MAP_PATH:
        raise RuntimeError(f"expected current review map {MAP_PATH}, got {current}")
    labels = sorted(
        str(actor.get_actor_label())
        for actor in unreal.EditorLevelLibrary.get_all_level_actors()
        if MANAGED_TAG in {str(tag) for tag in actor.get_editor_property("tags")}
    )
    if len(labels) != EXPECTED_COUNT:
        raise RuntimeError(f"expected {EXPECTED_COUNT} review actors, got {len(labels)}")
    if len(set(labels)) != EXPECTED_COUNT or not all(label.startswith("AV_Review_") for label in labels):
        raise RuntimeError("review actor labels are missing or duplicated")
    report = {"ok": True, "map": current, "gallery_count": len(labels), "labels": labels}
    EVIDENCE_PATH.parent.mkdir(parents=True, exist_ok=True)
    EVIDENCE_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("[GAMEXXK] " + json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
