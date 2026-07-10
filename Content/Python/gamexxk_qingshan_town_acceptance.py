"""Small UE-side probes for Qingshan prototype acceptance."""

from __future__ import annotations

import argparse
import json
import math

import unreal


PROTOTYPE_MAP = "/Game/GameXXK/Maps/Prototype/L_QingshanTown_PCG_Prototype"
SOURCE_MAP = "/Game/GameXXK/Maps/L_QingshanInn"
PCG_ACTOR_LABEL = "QingshanTown_PCG_Buildings"
PCG_GENERATED_COMPONENT_TAG = "PCG Generated Component"
TEXTURE_PATH = "/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA/T_Qingshan_Shop_A_Albedo"
PROP_TAGS = ("TownPCG_Prop", "TownPCG_PropMarker")
VIEWS = {
    "north-gate": ((-5000.0, 2500.0, 3400.0), (0.0, -3200.0, 200.0)),
    "market": ((4600.0, -4300.0, 3000.0), (0.0, -6400.0, 250.0)),
    "bridge": ((4700.0, -8500.0, 3100.0), (0.0, -10620.0, 170.0)),
    "south-wharf": ((-4800.0, -14500.0, 3400.0), (0.0, -11200.0, 150.0)),
}


def _package_path(value) -> str:
    return str(value.get_outermost().get_path_name()) if value is not None else ""


def _current_map() -> str:
    return _package_path(unreal.EditorLevelLibrary.get_editor_world())


def _dirty_maps() -> list[str]:
    return sorted(str(package.get_path_name()).split(".", 1)[0] for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())


def _dirty_content() -> list[str]:
    return sorted(str(package.get_path_name()).split(".", 1)[0] for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())


def _ensure_prototype() -> None:
    if _current_map() == PROTOTYPE_MAP:
        return
    if _dirty_maps() or _dirty_content():
        raise RuntimeError("refusing prototype map switch while packages are dirty")
    if not unreal.EditorLoadingAndSavingUtils.load_map(PROTOTYPE_MAP):
        raise RuntimeError("prototype map could not be loaded")


def _actors(label: str) -> list:
    return [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if actor.get_actor_label() == label]


def _tags(value) -> list[str]:
    try:
        raw = value.get_editor_property("component_tags")
    except Exception:
        raw = value.get_editor_property("tags")
    return [str(tag) for tag in (raw or [])]


def _vector(value) -> list[float]:
    return [float(value.x), float(value.y), float(value.z)]


def _transforms() -> list[dict]:
    matches = _actors(PCG_ACTOR_LABEL)
    if len(matches) != 1:
        raise RuntimeError(f"expected one PCG actor, found {len(matches)}")
    components = [
        component
        for component in matches[0].get_components_by_class(unreal.InstancedStaticMeshComponent)
        if PCG_GENERATED_COMPONENT_TAG in _tags(component)
    ]
    if len(components) != 1:
        raise RuntimeError(f"expected one generated ISM component, found {len(components)}")
    result = []
    component = components[0]
    for index in range(component.get_instance_count()):
        transform = component.get_instance_transform(index, world_space=True)
        rotation = transform.rotation.rotator()
        result.append({
            "location_cm": _vector(transform.translation),
            "rotation_degrees": [float(rotation.pitch), float(rotation.yaw), float(rotation.roll)],
            "scale": _vector(transform.scale3d),
        })
    return result


def _look_at(location, target) -> unreal.Rotator:
    delta = tuple(target[index] - location[index] for index in range(3))
    yaw = math.degrees(math.atan2(delta[1], delta[0]))
    pitch = math.degrees(math.atan2(delta[2], math.hypot(delta[0], delta[1])))
    return unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0)


def run(action: str, view: str = "") -> dict:
    _ensure_prototype()
    if SOURCE_MAP in _dirty_maps():
        raise RuntimeError("source map is dirty")
    if action == "snapshot":
        return {"success": True, "action": action, "current_map": _current_map(), "transforms": _transforms(), "dirty_maps": _dirty_maps(), "dirty_content": _dirty_content()}
    if action == "clear":
        payload = json.loads(unreal.GameXXKTownPCGAutomationLibrary.clear_town_pcg(PCG_ACTOR_LABEL))
        return {"success": bool(payload.get("success")), "action": action, "current_map": _current_map(), "clear": payload, "dirty_maps": _dirty_maps()}
    if action == "metadata":
        texture = unreal.load_asset(TEXTURE_PATH)
        if texture is None:
            raise RuntimeError("building albedo texture is missing")
        actors = unreal.EditorLevelLibrary.get_all_level_actors()
        prop_count = sum(any(tag in _tags(actor) for tag in PROP_TAGS) for actor in actors)
        return {"success": True, "action": action, "texture_path": TEXTURE_PATH, "texture_size": [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())], "prop_marker_count": prop_count}
    if action == "view":
        if view not in VIEWS:
            raise ValueError(f"unknown view: {view}")
        location, target = VIEWS[view]
        rotation = _look_at(location, target)
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(*location), rotation)
        return {"success": True, "action": action, "view": view, "location_cm": list(location), "target_cm": list(target), "rotation_degrees": [float(rotation.pitch), float(rotation.yaw), float(rotation.roll)]}
    raise ValueError(f"unknown action: {action}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--action", choices=("snapshot", "clear", "metadata", "view"), required=True)
    parser.add_argument("--view", default="")
    args = parser.parse_args()
    try:
        result = run(args.action, args.view)
    except Exception as error:
        result = {"success": False, "action": args.action, "error": str(error), "exception_type": type(error).__name__}
    print(json.dumps(result, sort_keys=True, separators=(",", ":"), allow_nan=False))


if __name__ == "__main__":
    main()
