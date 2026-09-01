"""Place exactly one managed post-carriage controller beside the accepted Rig."""

from __future__ import annotations

import json
import math

import unreal


TARGET_MAP = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
AFTERMATH_CLASS_PATH = "/Script/GameXXK.GameXXKPrologueAftermathController"
AFTERMATH_LABEL = "GameXXK_PrologueAftermath"
AFTERMATH_TAG = "GameXXKManaged.PrologueAftermath"
CARRIAGE_LABEL = "GameXXK_PrologueCarriageRig"
CARRIAGE_TAG = "GameXXKManaged.PrologueCarriageRig"
YUEBAI_REVEAL_OFFSET = (250.623, 666.139, 0.0)
YUEBAI_REVEAL_WORLD = (16929.215, 5936.139, 1075.711)


def _package_name(package: object) -> str:
    getter = getattr(package, "get_name", None)
    return str(getter() if callable(getter) else package)


def _dirty_packages() -> list[str]:
    result = {
        _package_name(package)
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    }
    result.update(
        _package_name(package)
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    )
    return sorted(result)


def _tags(actor: object) -> set[str]:
    return {str(value) for value in actor.get_editor_property("tags")}


def _vector(value: object) -> list[float]:
    return [float(value.x), float(value.y), float(value.z)]


def _transform(actor: object) -> dict[str, list[float]]:
    rotation = actor.get_actor_rotation()
    return {
        "location": _vector(actor.get_actor_location()),
        "rotation": [float(rotation.pitch), float(rotation.yaw), float(rotation.roll)],
        "scale": _vector(actor.get_actor_scale3d()),
    }


def _scene_component(actor: object, name: str) -> object:
    matches = [
        component
        for component in actor.get_components_by_class(unreal.SceneComponent)
        if str(component.get_name()) == name
    ]
    if len(matches) != 1:
        raise RuntimeError(f"expected one component named {name}, got {len(matches)}")
    return matches[0]


def _distance(left: list[float], right: tuple[float, float, float]) -> float:
    return math.dist(left, list(right))


def _current_map_package() -> str:
    world = unreal.EditorLevelLibrary.get_editor_world()
    return str(world.get_outermost().get_name()) if world else ""


def place() -> dict[str, object]:
    dirty_before = _dirty_packages()
    if dirty_before:
        raise RuntimeError(
            "refusing to load Qingshan with dirty packages; save through MCP first: "
            + ", ".join(dirty_before)
        )

    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if level_subsystem is None or actor_subsystem is None:
        raise RuntimeError("required editor subsystems are unavailable")
    if not level_subsystem.load_level(TARGET_MAP):
        raise RuntimeError(f"could not load target map: {TARGET_MAP}")
    if _current_map_package() != TARGET_MAP:
        raise RuntimeError(
            f"loaded map mismatch: expected {TARGET_MAP}, got {_current_map_package()}"
        )

    actors = list(actor_subsystem.get_all_level_actors())
    carriage_candidates = [
        actor
        for actor in actors
        if actor.get_actor_label() == CARRIAGE_LABEL and CARRIAGE_TAG in _tags(actor)
    ]
    if len(carriage_candidates) != 1:
        raise RuntimeError(
            f"expected exactly one accepted carriage Rig, got {len(carriage_candidates)}"
        )
    carriage = carriage_candidates[0]

    candidates = [
        actor
        for actor in actors
        if actor.get_actor_label() == AFTERMATH_LABEL or AFTERMATH_TAG in _tags(actor)
    ]
    if len(candidates) > 1:
        raise RuntimeError(
            f"expected at most one managed aftermath controller, got {len(candidates)}"
        )
    created = not candidates
    if candidates:
        aftermath = candidates[0]
        if (
            aftermath.get_actor_label() != AFTERMATH_LABEL
            or AFTERMATH_TAG not in _tags(aftermath)
        ):
            raise RuntimeError("refusing to update an unowned aftermath actor")
    else:
        aftermath_class = unreal.load_class(None, AFTERMATH_CLASS_PATH)
        if aftermath_class is None:
            raise RuntimeError(
                f"could not load native aftermath class: {AFTERMATH_CLASS_PATH}"
            )
        aftermath = actor_subsystem.spawn_actor_from_class(
            aftermath_class,
            carriage.get_actor_location(),
            carriage.get_actor_rotation(),
            False,
        )
        if aftermath is None:
            raise RuntimeError("could not spawn prologue aftermath controller")
        aftermath.set_actor_label(AFTERMATH_LABEL, True)
        aftermath.set_editor_property("tags", [unreal.Name(AFTERMATH_TAG)])

    aftermath.set_actor_location(carriage.get_actor_location(), False, False)
    aftermath.set_actor_rotation(carriage.get_actor_rotation(), False)
    aftermath.set_actor_scale3d(carriage.get_actor_scale3d())

    reveal = _scene_component(aftermath, "YueBaiReveal")
    relative_location = _vector(reveal.get_editor_property("relative_location"))
    world_location = _vector(reveal.get_world_location())
    if _distance(relative_location, YUEBAI_REVEAL_OFFSET) > 0.1:
        raise RuntimeError(f"YueBai reveal relative offset drifted: {relative_location}")
    if _distance(world_location, YUEBAI_REVEAL_WORLD) > 0.1:
        raise RuntimeError(f"YueBai reveal world location drifted: {world_location}")

    if not level_subsystem.save_current_level():
        raise RuntimeError("could not save the target Qingshan level")
    dirty_after = _dirty_packages()
    if dirty_after:
        raise RuntimeError("packages remain dirty after save: " + ", ".join(dirty_after))

    report = {
        "ok": True,
        "target_map": TARGET_MAP,
        "created": created,
        "aftermath_label": aftermath.get_actor_label(),
        "aftermath_tag": AFTERMATH_TAG,
        "aftermath_class": str(aftermath.get_class().get_name()),
        "aftermath_transform": _transform(aftermath),
        "carriage_transform": _transform(carriage),
        "managed_aftermath_count": 1,
        "yuebai_reveal_relative": relative_location,
        "yuebai_reveal_world": world_location,
        "saved_packages": [TARGET_MAP],
        "dirty_after": dirty_after,
    }
    unreal.log(json.dumps(report, ensure_ascii=False, sort_keys=True))
    return report


if __name__ == "__main__":
    print(json.dumps(place(), ensure_ascii=False, indent=2, sort_keys=True))
