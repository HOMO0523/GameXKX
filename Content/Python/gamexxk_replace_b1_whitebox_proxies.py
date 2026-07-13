"""Hide only the five B1 PCG proxy instances superseded by vendor buildings."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


B1_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1"
OUT = Path(__file__).resolve().parents[2] / "docs/production/evidence/asian-village-integration/b1-hidden-whitebox-proxies.json"
TARGETS = (
    ("CoreThatchedShop", "QS_B1_PCG_Building_courtyard_wing_orange", "ISM_SM_QS_B1_CourtyardWing_Orange_0", 1),
    ("CoreCanopyShop", "QS_B1_PCG_Building_tall_house_indigo", "ISM_SM_QS_B1_TallHouse_Indigo_0", 0),
    ("ApproachLowHouse", "QS_B1_PCG_Building_gable_shop_teal", "ISM_SM_QS_B1_GableShop_Teal_0", 1),
    ("BridgeHouse", "QS_B1_PCG_Building_tall_house_ochre", "ISM_SM_QS_B1_TallHouse_Ochre_0", 0),
    ("SouthTower", "QS_B1_PCG_Building_dock_shed_teal", "ISM_SM_QS_B1_DockShed_Teal_0", 0),
)


def _current_map() -> str:
    world = unreal.EditorLevelLibrary.get_editor_world()
    return "" if world is None else str(world.get_outermost().get_path_name()).split(".", 1)[0]


def _component(actor_label: str, component_name: str):
    actors = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if str(actor.get_actor_label()) == actor_label]
    if len(actors) != 1:
        raise RuntimeError(f"expected one actor {actor_label}, got {len(actors)}")
    components = [component for component in actors[0].get_components_by_class(unreal.InstancedStaticMeshComponent) if str(component.get_name()) == component_name]
    if len(components) != 1:
        raise RuntimeError(f"expected one component {component_name}, got {len(components)}")
    return components[0]


def _transform_payload(transform):
    rotation = transform.rotation
    scale = transform.scale3d
    translation = transform.translation
    return {
        "translation": [float(translation.x), float(translation.y), float(translation.z)],
        "rotation_quat": [float(rotation.x), float(rotation.y), float(rotation.z), float(rotation.w)],
        "scale": [float(scale.x), float(scale.y), float(scale.z)],
    }


def main() -> None:
    if _current_map() != B1_MAP:
        raise RuntimeError(f"must run in B1 map, got {_current_map()}")
    entries = []
    for purpose, actor_label, component_name, index in TARGETS:
        component = _component(actor_label, component_name)
        original = component.get_instance_transform(index, world_space=True)
        if original is None:
            raise RuntimeError(f"missing instance {actor_label}/{component_name}/{index}")
        payload = _transform_payload(original)
        original.scale3d = unreal.Vector(0.001, 0.001, 0.001)
        if not component.update_instance_transform(index, original, True, True, False):
            raise RuntimeError(f"failed to hide proxy {purpose}")
        check = component.get_instance_transform(index, world_space=True)
        if check is None or max(abs(float(value)) for value in (check.scale3d.x, check.scale3d.y, check.scale3d.z)) > 0.002:
            raise RuntimeError(f"proxy scale readback failed for {purpose}")
        entries.append({"purpose": purpose, "actor": actor_label, "component": component_name, "instance_index": index, "original_transform": payload})
    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError("failed to save B1 whitebox replacements")
    report = {"ok": True, "map": B1_MAP, "hidden_proxy_count": len(entries), "entries": entries}
    OUT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("[GAMEXXK] " + json.dumps({"ok": True, "hidden_proxy_count": len(entries)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
