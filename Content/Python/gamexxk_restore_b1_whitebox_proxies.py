"""Restore the exact B1 PCG proxy instances hidden by the vendor replacement test."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


B1_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1"
SOURCE = Path(__file__).resolve().parents[2] / "docs/production/evidence/asian-village-integration/b1-hidden-whitebox-proxies.json"


def main() -> None:
    world = unreal.EditorLevelLibrary.get_editor_world()
    current = "" if world is None else str(world.get_outermost().get_path_name()).split(".", 1)[0]
    if current != B1_MAP:
        raise RuntimeError(f"must run in B1 map, got {current}")
    data = json.loads(SOURCE.read_text(encoding="utf-8"))
    restored = 0
    for entry in data["entries"]:
        actors = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if str(actor.get_actor_label()) == entry["actor"]]
        if len(actors) != 1:
            raise RuntimeError(f"actor lookup failed: {entry['actor']}")
        components = [component for component in actors[0].get_components_by_class(unreal.InstancedStaticMeshComponent) if str(component.get_name()) == entry["component"]]
        if len(components) != 1:
            raise RuntimeError(f"component lookup failed: {entry['component']}")
        payload = entry["original_transform"]
        transform = unreal.Transform()
        transform.translation = unreal.Vector(*payload["translation"])
        transform.rotation = unreal.Quat(*payload["rotation_quat"])
        transform.scale3d = unreal.Vector(*payload["scale"])
        if not components[0].update_instance_transform(entry["instance_index"], transform, True, True, False):
            raise RuntimeError(f"failed to restore {entry['purpose']}")
        restored += 1
    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError("failed to save restored B1 proxies")
    print(f"[GAMEXXK] restored_proxy_count={restored}")


if __name__ == "__main__":
    main()
