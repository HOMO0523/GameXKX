"""Read-only snapshots and editor views for the Qingshan B0R whitebox."""

from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SCRIPTS_DIR = PROJECT_ROOT / "scripts"
if str(SCRIPTS_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPTS_DIR))

from gamexxk_qingshan_whitebox_config import load_config
from qingshan_whitebox_acceptance import canonical_layout_hash
from gamexxk_validate_qingshan_whitebox_b0r import _expected_proxy_transforms, _match_observed_transforms


WHITEBOX_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R"
SOURCE_MAP = "/Game/GameXXK/Maps/L_QingshanInn"
MANAGED_TAG = "QingshanB0RManaged"
PCG_GENERATED_COMPONENT_TAG = "PCG Generated Component"
CAMERA_IDS = (
    "CAM_QS_GATE_ARRIVAL",
    "CAM_QS_TOWN_CORE",
    "CAM_QS_MAIN_BRIDGE",
    "CAM_QS_SOUTH_DOCK",
)
PCG_LABELS = {
    "buildings": "QS_B0R_PCG_Buildings",
    "foliage": "QS_B0R_PCG_Foliage",
    "mountains": "QS_B0R_PCG_Mountains",
}


def _package_path(value) -> str:
    return str(value.get_outermost().get_path_name()).split(".", 1)[0] if value is not None else ""


def _current_map() -> str:
    return _package_path(unreal.EditorLevelLibrary.get_editor_world())


def _dirty_maps() -> list[str]:
    return sorted(_package_path(package) for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())


def _dirty_content() -> list[str]:
    return sorted(_package_path(package) for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())


def _ensure_whitebox() -> None:
    if _current_map() == WHITEBOX_MAP:
        return
    if _dirty_maps() or _dirty_content():
        raise RuntimeError("refusing whitebox map switch while packages are dirty")
    if not unreal.EditorLoadingAndSavingUtils.load_map(WHITEBOX_MAP):
        raise RuntimeError(f"could not load only whitebox map {WHITEBOX_MAP}")


def _tags(value) -> set[str]:
    try:
        raw = value.get_editor_property("component_tags")
    except Exception:
        raw = value.get_editor_property("tags")
    return {str(tag) for tag in (raw or [])}


def _vector(value) -> list[float]:
    result = [float(value.x), float(value.y), float(value.z)]
    if not all(math.isfinite(item) for item in result):
        raise ValueError("non-finite vector in whitebox snapshot")
    return result


def _rotation(value) -> list[float]:
    result = [float(value.roll), float(value.pitch), float(value.yaw)]
    if not all(math.isfinite(item) for item in result):
        raise ValueError("non-finite rotation in whitebox snapshot")
    return result


def _transform(value) -> dict:
    rotation = value.rotation.rotator() if hasattr(value, "rotation") else value.get_actor_rotation()
    location = value.translation if hasattr(value, "translation") else value.get_actor_location()
    scale = value.scale3d if hasattr(value, "scale3d") else value.get_actor_scale3d()
    return {"location_cm": _vector(location), "rotation_degrees": _rotation(rotation), "scale": _vector(scale)}


def _actors() -> list:
    return list(unreal.EditorLevelLibrary.get_all_level_actors())


def _label(actor) -> str:
    return str(actor.get_actor_label())


def _unique_actor(label: str):
    matches = [actor for actor in _actors() if _label(actor) == label]
    if len(matches) != 1:
        raise RuntimeError(f"expected one actor labelled {label}, found {len(matches)}")
    return matches[0]


def _pcg_transforms(label: str) -> list[dict]:
    actor = _unique_actor(label)
    result = []
    for component in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
        if PCG_GENERATED_COMPONENT_TAG not in _tags(component):
            continue
        for index in range(int(component.get_instance_count())):
            result.append(_transform(component.get_instance_transform(index, world_space=True)))
    return result


def _snapshot() -> dict:
    config = load_config()
    managed = {}
    for actor in _actors():
        if MANAGED_TAG in _tags(actor):
            managed[_label(actor)] = {
                "transform": _transform(actor),
                "tags": sorted(_tags(actor)),
                "level": _package_path(actor.get_level()),
            }
    instances = {key: _pcg_transforms(label) for key, label in PCG_LABELS.items()}
    anchor = _unique_actor(config["coordinate_reference"]["actor_label"])
    expected_layout = _expected_proxy_transforms(config, anchor)
    observed_layout = {
        key: _match_observed_transforms(instances[key], expected_layout[key], key)
        for key in PCG_LABELS
    }
    expected_hash = canonical_layout_hash(expected_layout)["sha256"]
    observed_hash = canonical_layout_hash(observed_layout)["sha256"]
    if observed_hash != expected_hash:
        raise RuntimeError(f"observed layout hash differs from expected: observed={observed_hash}, expected={expected_hash}")
    return {
        "success": True,
        "action": "snapshot",
        "current_map": _current_map(),
        "source_map_clean": SOURCE_MAP not in _dirty_maps(),
        "managed_actors": managed,
        "pcg_instance_transforms": observed_layout,
        "counts": {key: len(value) for key, value in instances.items()},
        "layout_sha256": observed_hash,
        "expected_layout_sha256": expected_hash,
        "observed_layout_sha256": observed_hash,
        "cameras": {
            camera_id: {
                "transform": _transform(_unique_actor(camera_id)),
                "field_of_view": float(_unique_actor(camera_id).get_camera_component().get_editor_property("field_of_view")),
                "configured": config["cameras"][camera_id],
            }
            for camera_id in CAMERA_IDS
        },
    }


def _look_at_degrees(location, target) -> tuple[float, float, float]:
    values = tuple(float(value) for value in (*location, *target))
    if len(values) != 6 or not all(math.isfinite(value) for value in values):
        raise ValueError("look-at vectors must contain finite XYZ values")
    dx, dy, dz = (values[index + 3] - values[index] for index in range(3))
    horizontal = math.hypot(dx, dy)
    if horizontal == 0.0:
        yaw = 0.0
        pitch = 90.0 if dz >= 0.0 else -90.0
    else:
        yaw = math.degrees(math.atan2(dy, dx))
        pitch = math.degrees(math.atan2(dz, horizontal))
    return (pitch, yaw, 0.0)


def _north_local_location(anchor, local) -> list[float]:
    origin = _vector(anchor.get_actor_location())
    yaw = math.radians(float(anchor.get_actor_rotation().yaw))
    x, y, z = (float(value) for value in local)
    return [
        origin[0] + x * math.cos(yaw) - y * math.sin(yaw),
        origin[1] + x * math.sin(yaw) + y * math.cos(yaw),
        origin[2] + z,
    ]


def _angle_delta(first: float, second: float) -> float:
    return abs((float(first) - float(second) + 180.0) % 360.0 - 180.0)


def _transform_matches(actual, expected) -> bool:
    values = [float(value) for field in ("location_cm", "rotation_degrees", "scale") for value in (*actual[field], *expected[field])]
    if not all(math.isfinite(value) for value in values):
        return False
    return (
        all(abs(actual["location_cm"][index] - expected["location_cm"][index]) <= 1.0 for index in range(3))
        and all(_angle_delta(actual["rotation_degrees"][index], expected["rotation_degrees"][index]) <= 0.1 for index in range(3))
        and all(abs(actual["scale"][index] - expected["scale"][index]) <= 0.001 for index in range(3))
    )


def _camera_view(camera_id: str) -> dict:
    if camera_id not in CAMERA_IDS:
        raise ValueError(f"unknown camera id: {camera_id}")
    config = load_config()
    actor = _unique_actor(camera_id)
    if MANAGED_TAG not in _tags(actor):
        raise RuntimeError(f"camera is not tag-owned: {camera_id}")
    location = _vector(actor.get_actor_location())
    rotation = actor.get_actor_rotation()
    rotation_payload = _rotation(rotation)
    fov = float(actor.get_camera_component().get_editor_property("field_of_view"))
    spec = config["cameras"][camera_id]
    anchor = _unique_actor(config["coordinate_reference"]["actor_label"])
    expected_location = _north_local_location(anchor, spec["location_cm"])
    expected_target = _north_local_location(anchor, spec["target_cm"])
    pitch, yaw, roll = _look_at_degrees(expected_location, expected_target)
    expected = {
        "location_cm": expected_location,
        "rotation_degrees": [roll, pitch, yaw],
        "scale": [1.0, 1.0, 1.0],
    }
    if not _transform_matches(_transform(actor), expected):
        raise RuntimeError(f"camera transform differs from config: {camera_id}")
    if abs(fov - float(spec["fov_degrees"])) > 0.01:
        raise RuntimeError(f"camera FOV differs from config: {camera_id}")
    if not all(math.isfinite(value) for value in (*location, *rotation_payload, *expected_location, *expected_target)):
        raise RuntimeError(f"camera transform is non-finite: {camera_id}")
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(
        unreal.Vector(x=location[0], y=location[1], z=location[2]),
        unreal.Rotator(pitch=float(rotation.pitch), yaw=float(rotation.yaw), roll=float(rotation.roll)),
    )
    return {
        "success": True, "action": "view", "camera_id": camera_id,
        "location_cm": location, "target_cm": expected_target,
        "rotation_degrees": rotation_payload, "field_of_view": fov,
    }


def _topdown_location(anchor, bounds) -> list[float]:
    rotation = anchor.get_actor_rotation()
    scale = _vector(anchor.get_actor_scale3d())
    values = [float(rotation.pitch), float(rotation.yaw), float(rotation.roll), *scale, *(float(value) for value in bounds)]
    if len(bounds) != 4 or not all(math.isfinite(value) for value in values):
        raise RuntimeError("north anchor and world bounds must be finite")
    if abs(float(rotation.pitch)) > 0.001 or abs(float(rotation.roll)) > 0.001 or any(abs(value - 1.0) > 0.001 for value in scale):
        raise RuntimeError("north anchor must be upright with unit scale")
    origin = _vector(anchor.get_actor_location())
    local_x = (float(bounds[0]) + float(bounds[1])) / 2.0
    local_y = (float(bounds[2]) + float(bounds[3])) / 2.0
    yaw = math.radians(float(rotation.yaw))
    return [
        origin[0] + local_x * math.cos(yaw) - local_y * math.sin(yaw),
        origin[1] + local_x * math.sin(yaw) + local_y * math.cos(yaw),
        origin[2] + 42000.0,
    ]


def _topdown() -> dict:
    config = load_config()
    anchor = _unique_actor(config["coordinate_reference"]["actor_label"])
    center = _topdown_location(anchor, config["world_bounds_cm"])
    rotation = unreal.Rotator(pitch=-90.0, yaw=0.0, roll=0.0)
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(
        unreal.Vector(x=center[0], y=center[1], z=center[2]), rotation
    )
    return {"success": True, "action": "topdown", "location_cm": center, "rotation_degrees": [0.0, -90.0, 0.0]}


def run(action: str, camera_id: str = "") -> dict:
    _ensure_whitebox()
    if SOURCE_MAP in _dirty_maps():
        raise RuntimeError("source map is dirty")
    if action == "snapshot":
        return _snapshot()
    if action == "view":
        return _camera_view(camera_id)
    if action == "topdown":
        return _topdown()
    raise ValueError(f"unknown action: {action}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--action", required=True, choices=("snapshot", "view", "topdown"))
    parser.add_argument("--camera-id", default="", choices=("", *CAMERA_IDS))
    args = parser.parse_args()
    try:
        result = run(args.action, args.camera_id)
    except Exception as error:
        result = {"success": False, "action": args.action, "error": str(error), "exception_type": type(error).__name__}
    try:
        output = json.dumps(result, sort_keys=True, separators=(",", ":"), allow_nan=False)
    except (TypeError, ValueError) as error:
        output = json.dumps({"success": False, "action": args.action, "error": f"JSON serialization failed: {error}"}, sort_keys=True, separators=(",", ":"), allow_nan=False)
    print(output)


if __name__ == "__main__":
    main()
