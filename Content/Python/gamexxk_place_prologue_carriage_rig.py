"""Place exactly one managed prologue carriage Rig in the playable Qingshan map."""

from __future__ import annotations

import argparse
import json
import math

import unreal


TARGET_MAP = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
RIG_CLASS_PATH = "/Script/GameXXK.GameXXKPrologueCarriageRig"
RIG_LABEL = "GameXXK_PrologueCarriageRig"
MANAGED_TAG = "GameXXKManaged.PrologueCarriageRig"
APPROVED_LOCATION = unreal.Vector(16678.592, 5270.000, 1075.711)
APPROVED_ROTATION = unreal.Rotator(0.0, 0.0, 0.0)


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


def _transform(actor: object) -> dict[str, list[float]]:
    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    return {
        "location": [float(location.x), float(location.y), float(location.z)],
        "rotation": [float(rotation.pitch), float(rotation.yaw), float(rotation.roll)],
        "scale": [float(scale.x), float(scale.y), float(scale.z)],
    }


def _tags(actor: object) -> set[str]:
    return {str(value) for value in actor.get_editor_property("tags")}


def _current_map_package() -> str:
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        return ""
    return str(world.get_outermost().get_name())


def place(calibrate_approved_anchor: bool = False) -> dict:
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
    player_starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
    if len(player_starts) != 1:
        raise RuntimeError(f"expected exactly one PlayerStart, got {len(player_starts)}")
    player_start = player_starts[0]
    player_start_before = _transform(player_start)

    if calibrate_approved_anchor:
        player_start.set_actor_location(APPROVED_LOCATION, False, False)
        player_start.set_actor_rotation(APPROVED_ROTATION, False)

    candidates = [
        actor
        for actor in actors
        if actor.get_actor_label() == RIG_LABEL or MANAGED_TAG in _tags(actor)
    ]
    if len(candidates) > 1:
        raise RuntimeError(f"expected at most one managed Rig, got {len(candidates)}")
    created = not candidates
    if candidates:
        rig = candidates[0]
        if rig.get_actor_label() != RIG_LABEL or MANAGED_TAG not in _tags(rig):
            raise RuntimeError("refusing to update unowned prologue carriage actor")
    else:
        rig_class = unreal.load_class(None, RIG_CLASS_PATH)
        if rig_class is None:
            raise RuntimeError(f"could not load native Rig class: {RIG_CLASS_PATH}")
        rig = actor_subsystem.spawn_actor_from_class(
            rig_class,
            player_start.get_actor_location(),
            player_start.get_actor_rotation(),
            False,
        )
        if rig is None:
            raise RuntimeError("could not spawn prologue carriage Rig")
        rig.set_actor_label(RIG_LABEL, True)
        rig.set_editor_property("tags", [unreal.Name(MANAGED_TAG)])

    rig.set_actor_location(player_start.get_actor_location(), False, False)
    rig.set_actor_rotation(player_start.get_actor_rotation(), False)
    rig.set_actor_scale3d(player_start.get_actor_scale3d())

    player_start_after = _transform(player_start)
    if not calibrate_approved_anchor and player_start_after != player_start_before:
        raise RuntimeError("PlayerStart transform changed during Rig placement")
    if calibrate_approved_anchor:
        expected_location = [
            float(APPROVED_LOCATION.x),
            float(APPROVED_LOCATION.y),
            float(APPROVED_LOCATION.z),
        ]
        expected_rotation = [
            float(APPROVED_ROTATION.pitch),
            float(APPROVED_ROTATION.yaw),
            float(APPROVED_ROTATION.roll),
        ]
        rig_after = _transform(rig)
        if (
            math.dist(player_start_after["location"], expected_location) > 0.1
            or player_start_after["rotation"] != expected_rotation
            or math.dist(rig_after["location"], expected_location) > 0.1
            or rig_after["rotation"] != expected_rotation
        ):
            raise RuntimeError("approved PlayerStart/Rig calibration did not commit exactly")
    if not level_subsystem.save_current_level():
        raise RuntimeError("could not save the target Qingshan level")
    dirty_after = _dirty_packages()
    if dirty_after:
        raise RuntimeError("packages remain dirty after save: " + ", ".join(dirty_after))

    report = {
        "ok": True,
        "target_map": TARGET_MAP,
        "calibration_mode": bool(calibrate_approved_anchor),
        "created": created,
        "rig_label": rig.get_actor_label(),
        "rig_tags": sorted(_tags(rig)),
        "rig_class": str(rig.get_class().get_name()),
        "rig_transform": _transform(rig),
        "player_start_transform_before": player_start_before,
        "player_start_transform_after": player_start_after,
        "managed_rig_count": 1,
        "saved_packages": [TARGET_MAP],
        "dirty_after": dirty_after,
    }
    unreal.log(json.dumps(report, ensure_ascii=False, sort_keys=True))
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--calibrate-approved-anchor", action="store_true")
    args = parser.parse_args()
    print(
        json.dumps(
            place(calibrate_approved_anchor=args.calibrate_approved_anchor),
            ensure_ascii=False,
            indent=2,
            sort_keys=True,
        )
    )
