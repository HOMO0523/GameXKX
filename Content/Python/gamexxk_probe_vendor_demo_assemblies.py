"""Read-only UE 5.4 commandlet probe for Eastern Village demo building assemblies."""

from __future__ import annotations

import json
import os
from pathlib import Path

import unreal


MAP_PATH = "/Game/Asian_Village/maps/Asian_Village_Demo"
OUTPUT = Path(os.environ["GAMEXXK_VENDOR_ASSEMBLY_OUTPUT"])


def _mesh_path(actor) -> str:
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if component is None:
        return ""
    mesh = component.get_editor_property("static_mesh")
    return "" if mesh is None else str(mesh.get_path_name()).split(".", 1)[0]


def _record(actor) -> dict:
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    return {
        "label": str(actor.get_actor_label()),
        "mesh": _mesh_path(actor),
        "location_cm": [round(float(location.x), 2), round(float(location.y), 2), round(float(location.z), 2)],
        "rotation": [round(float(rotation.roll), 2), round(float(rotation.pitch), 2), round(float(rotation.yaw), 2)],
        "scale": [round(float(scale.x), 3), round(float(scale.y), 3), round(float(scale.z), 3)],
    }


def main() -> None:
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError(f"could not load vendor demo {MAP_PATH}")
    records = [
        _record(actor)
        for actor in unreal.EditorLevelLibrary.get_all_level_actors()
        if isinstance(actor, unreal.StaticMeshActor)
        and "/Game/Asian_Village/meshes/building/" in _mesh_path(actor)
    ]
    records.sort(key=lambda entry: (entry["location_cm"][0], entry["location_cm"][1], entry["mesh"]))
    if not records:
        raise RuntimeError("vendor demo contained no building mesh actors")
    report = {"ok": True, "map": MAP_PATH, "building_actor_count": len(records), "actors": records}
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("[GAMEXXK] " + json.dumps({"ok": True, "building_actor_count": len(records)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
