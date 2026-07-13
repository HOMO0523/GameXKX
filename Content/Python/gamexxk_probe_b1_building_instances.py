"""Read-only inventory of B1 instanced building components near replacement plots."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


B1_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1"
PLOTS = {
    "CoreThatchedShop": (-13600.0, -200.0),
    "CoreCanopyShop": (-10200.0, 600.0),
    "ApproachLowHouse": (-8200.0, -4700.0),
    "BridgeHouse": (-18500.0, -1500.0),
    "SouthTower": (-17400.0, -10800.0),
}
OUT = Path(__file__).resolve().parents[2] / "docs/production/evidence/asian-village-integration/b1-pcg-building-instances.json"


def main() -> None:
    world = unreal.EditorLevelLibrary.get_editor_world()
    current = "" if world is None else str(world.get_outermost().get_path_name()).split(".", 1)[0]
    if current != B1_MAP:
        raise RuntimeError(f"expected current B1 map, got {current}")
    anchors = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if str(actor.get_actor_label()) == "QingshanInn_TownExit"]
    if len(anchors) != 1:
        raise RuntimeError(f"expected one North Gate F anchor, got {len(anchors)}")
    anchor_transform = anchors[0].get_actor_transform()
    world_plots = {
        name: unreal.MathLibrary.transform_location(anchor_transform, unreal.Vector(x, y, 0.0))
        for name, (x, y) in PLOTS.items()
    }
    matches = {name: [] for name in PLOTS}
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        for component in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            mesh = component.get_editor_property("static_mesh")
            if mesh is None:
                continue
            mesh_path = str(mesh.get_path_name()).split(".", 1)[0]
            for index in range(int(component.get_instance_count())):
                transform = component.get_instance_transform(index, world_space=True)
                if transform is None:
                    continue
                point = transform.translation
                for name, target in world_plots.items():
                    distance = ((float(point.x) - float(target.x)) ** 2 + (float(point.y) - float(target.y)) ** 2) ** 0.5
                    if distance <= 1300.0:
                        matches[name].append({
                            "distance_cm": round(distance, 2),
                            "actor": str(actor.get_actor_label()),
                            "component": str(component.get_name()),
                            "mesh": mesh_path,
                            "instance_index": index,
                            "location_cm": [round(float(point.x), 2), round(float(point.y), 2), round(float(point.z), 2)],
                        })
    for values in matches.values():
        values.sort(key=lambda value: value["distance_cm"])
    report = {"ok": True, "map": current, "matches": matches}
    OUT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("[GAMEXXK] " + json.dumps({"ok": True, "counts": {name: len(items) for name, items in matches.items()}}, ensure_ascii=False))


if __name__ == "__main__":
    main()
