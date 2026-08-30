"""Read-only structural validation for the Qingshan prologue carriage preview."""

from __future__ import annotations

import json
import math

import unreal


TARGET_MAP = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
RIG_LABEL = "GameXXK_PrologueCarriageRig"
MANAGED_TAG = "GameXXKManaged.PrologueCarriageRig"
APPROVED_LOCATION = (16678.592, 5270.000, 1075.711)
DISPLAY_GROUND_OFFSET_Z = -72.0
TEXTURES = {
    "run_stop_2k": (
        "/Game/GameXXK/Cinematics/Prologue/Atlases/"
        "T_cinematic_carriage_run_stop_2k_atlas",
        2048,
    ),
    "idle_2k": (
        "/Game/GameXXK/Cinematics/Prologue/Atlases/"
        "T_cinematic_carriage_post_stop_idle_2k_atlas",
        2048,
    ),
    "run_stop_1k": (
        "/Game/GameXXK/Cinematics/Prologue/Atlases/"
        "T_cinematic_carriage_run_stop_1k_atlas",
        1024,
    ),
    "idle_1k": (
        "/Game/GameXXK/Cinematics/Prologue/Atlases/"
        "T_cinematic_carriage_post_stop_idle_1k_atlas",
        1024,
    ),
}


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


def _component(actor: object, name: str) -> object:
    matches = [
        component
        for component in actor.get_components_by_class(unreal.SceneComponent)
        if component.get_name() == name
    ]
    if len(matches) != 1:
        raise RuntimeError(f"expected one component named {name}, got {len(matches)}")
    return matches[0]


def _relative_location(component: object) -> tuple[float, float, float]:
    value = component.get_editor_property("relative_location")
    return float(value.x), float(value.y), float(value.z)


def _subtract(
    left: tuple[float, float, float],
    right: tuple[float, float, float],
) -> tuple[float, float, float]:
    return tuple(a - b for a, b in zip(left, right))


def _length(value: tuple[float, float, float]) -> float:
    return math.sqrt(sum(component * component for component in value))


def _dot(
    left: tuple[float, float, float],
    right: tuple[float, float, float],
) -> float:
    return sum(a * b for a, b in zip(left, right))


def _parse_dimensions(value: str) -> tuple[int, int]:
    parts = [part.strip() for part in str(value).lower().split("x")]
    if len(parts) != 2 or not all(part.isdigit() for part in parts):
        raise RuntimeError(f"invalid Texture2D Dimensions tag: {value!r}")
    return int(parts[0]), int(parts[1])


def _texture_size(texture: object) -> tuple[int, int]:
    subsystem = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    asset_path = str(texture.get_path_name()).split(".", 1)[0]
    if subsystem is not None:
        tags = {
            str(key): str(value)
            for key, value in subsystem.get_tag_values(asset_path).items()
        }
        if tags.get("Dimensions"):
            return _parse_dimensions(tags["Dimensions"])
    return int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())


def _resident_texture_size(texture: object) -> tuple[int, int]:
    return int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())


def validate() -> dict:
    dirty_before = _dirty_packages()
    if dirty_before:
        raise RuntimeError(
            "read-only validator requires a clean editor: " + ", ".join(dirty_before)
        )
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if level_subsystem is None or actor_subsystem is None:
        raise RuntimeError("required editor subsystems are unavailable")
    if not level_subsystem.load_level(TARGET_MAP):
        raise RuntimeError(f"could not load target map: {TARGET_MAP}")

    actors = list(actor_subsystem.get_all_level_actors())
    rigs = [
        actor
        for actor in actors
        if actor.get_actor_label() == RIG_LABEL and MANAGED_TAG in _tags(actor)
    ]
    if len(rigs) != 1:
        raise RuntimeError(f"expected one managed carriage Rig, got {len(rigs)}")
    player_starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
    if len(player_starts) != 1:
        raise RuntimeError(f"expected exactly one PlayerStart, got {len(player_starts)}")
    rig = rigs[0]
    player_start = player_starts[0]
    rig_location = rig.get_actor_location()
    player_location = player_start.get_actor_location()
    if (rig_location - player_location).length() > 0.1:
        raise RuntimeError("Rig is not anchored to PlayerStart")
    actual_anchor = (
        float(player_location.x),
        float(player_location.y),
        float(player_location.z),
    )
    if _length(_subtract(actual_anchor, APPROVED_LOCATION)) > 0.1:
        raise RuntimeError(
            f"PlayerStart/Rig anchor is not the approved PIE location: {actual_anchor}"
        )

    start = _relative_location(_component(rig, "CarriageStart"))
    stop = _relative_location(_component(rig, "CarriageStop"))
    exit_location = _relative_location(_component(rig, "CarriageExit"))
    hero = _relative_location(_component(rig, "HeroReveal"))
    display = _relative_location(_component(rig, "CarriageDisplay"))
    arrival = _subtract(stop, start)
    departure = _subtract(exit_location, stop)
    arrival_distance = _length(arrival)
    if abs(arrival_distance - 400.0) > 0.1:
        raise RuntimeError(f"arrival distance drifted: {arrival_distance}")
    if _dot(arrival, departure) <= 0.0:
        raise RuntimeError("departure does not continue in the arrival direction")
    if hero[0] >= stop[0]:
        raise RuntimeError("hero reveal marker is not in front of the carriage plane")
    if abs(display[2] - DISPLAY_GROUND_OFFSET_Z) > 0.1:
        raise RuntimeError(f"carriage display ground offset drifted: {display[2]}")

    texture_report: dict[str, dict[str, object]] = {}
    for key, (path, expected_size) in TEXTURES.items():
        texture = unreal.EditorAssetLibrary.load_asset(path)
        if texture is None or not isinstance(texture, unreal.Texture2D):
            raise RuntimeError(f"missing carriage texture: {path}")
        size = _texture_size(texture)
        if size != (expected_size, expected_size):
            raise RuntimeError(f"texture size mismatch for {path}: {size}")
        texture_report[key] = {
            "path": path,
            "size": list(size),
            "resident_size": list(_resident_texture_size(texture)),
        }

    dirty_after = _dirty_packages()
    if dirty_after != dirty_before:
        raise RuntimeError(f"read-only validation dirtied packages: {dirty_after}")
    report = {
        "ok": True,
        "target_map": TARGET_MAP,
        "rig_label": rig.get_actor_label(),
        "rig_class": str(rig.get_class().get_name()),
        "managed_rig_count": len(rigs),
        "player_start_count": len(player_starts),
        "arrival_distance": arrival_distance,
        "arrival_departure_dot": _dot(arrival, departure),
        "start_offset": list(start),
        "stop_offset": list(stop),
        "exit_offset": list(exit_location),
        "hero_reveal_offset": list(hero),
        "carriage_display_offset": list(display),
        "approved_anchor": list(APPROVED_LOCATION),
        "textures": texture_report,
        "dirty_after": dirty_after,
    }
    unreal.log(json.dumps(report, ensure_ascii=False, sort_keys=True))
    return report


if __name__ == "__main__":
    print(json.dumps(validate(), ensure_ascii=False, indent=2, sort_keys=True))
