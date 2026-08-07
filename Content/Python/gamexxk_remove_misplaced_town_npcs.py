"""Remove only the accidentally installed managed NPC actors from L_QingshanInn."""

from __future__ import annotations

import json

import unreal


MAP = "/Game/GameXXK/Maps/L_QingshanInn"
MANAGED_PREFIX = "QingshanTown_Npc_"


def main() -> dict[str, object]:
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not level_subsystem or not actor_subsystem:
        raise RuntimeError("required editor subsystems are unavailable")
    if not level_subsystem.load_level(MAP):
        raise RuntimeError(f"could not load cleanup map: {MAP}")

    removed: list[str] = []
    for actor in list(actor_subsystem.get_all_level_actors()):
        label = actor.get_actor_label()
        if label.startswith(MANAGED_PREFIX):
            if not actor_subsystem.destroy_actor(actor):
                raise RuntimeError(f"could not remove misplaced actor: {label}")
            removed.append(label)

    if len(removed) != 6:
        raise RuntimeError(f"refusing unexpected cleanup count: {len(removed)} labels={removed}")
    if not level_subsystem.save_current_level():
        raise RuntimeError("could not save cleaned L_QingshanInn")

    remaining = sorted(
        actor.get_actor_label()
        for actor in actor_subsystem.get_all_level_actors()
        if actor.get_actor_label().startswith(MANAGED_PREFIX)
    )
    result = {
        "ok": not remaining,
        "map": MAP,
        "removed": sorted(removed),
        "remaining_managed": remaining,
        "saved": True,
    }
    if not result["ok"]:
        raise RuntimeError("managed NPC cleanup verification failed")
    print(json.dumps(result, ensure_ascii=False))
    return result


if __name__ == "__main__":
    main()
