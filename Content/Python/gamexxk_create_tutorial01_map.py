"""Create/update only the isolated pure-2D tutorial 0-1 map."""

from __future__ import annotations

import json

import unreal


MAP_PATH = "/Game/GameXXK/Maps/Tutorial/L_Tutorial_0_1"
MAP_DIRECTORY = "/Game/GameXXK/Maps/Tutorial"
EXPECTED_GAME_MODE = "/Script/GameXXK.GameXXKTutorial01GameMode"


def _package_name(package: object) -> str:
    method = getattr(package, "get_name", None)
    return str(method()) if callable(method) else str(package)


def _dirty_packages() -> list[str]:
    packages = list(
        unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    )
    packages.extend(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
    return sorted({_package_name(package) for package in packages})


def _editor_world():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return subsystem.get_editor_world() if subsystem else None


def _class_path(value: object) -> str:
    method = getattr(value, "get_path_name", None)
    return str(method()) if callable(method) else ""


def create_or_update() -> dict:
    dirty_before = _dirty_packages()
    if dirty_before:
        raise RuntimeError(
            "refusing tutorial-map creation with dirty packages: "
            + ", ".join(dirty_before)
        )

    if not unreal.EditorAssetLibrary.does_directory_exist(MAP_DIRECTORY):
        if not unreal.EditorAssetLibrary.make_directory(MAP_DIRECTORY):
            raise RuntimeError(f"could not create map directory: {MAP_DIRECTORY}")

    created = False
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
            raise RuntimeError(f"could not load tutorial map: {MAP_PATH}")
    else:
        if not unreal.EditorLevelLibrary.new_level(MAP_PATH):
            raise RuntimeError(f"could not create tutorial map: {MAP_PATH}")
        created = True

    world = _editor_world()
    if world is None:
        raise RuntimeError("tutorial map has no editor world")
    world_settings = world.get_world_settings()
    game_mode_class = getattr(unreal, "GameXXKTutorial01GameMode", None)
    if world_settings is None or game_mode_class is None:
        raise RuntimeError("tutorial GameMode class is unavailable")
    world_settings.set_editor_property(
        "default_game_mode", game_mode_class.static_class()
    )

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = list(actor_subsystem.get_all_level_actors()) if actor_subsystem else []
    if actors:
        raise RuntimeError(
            "tutorial map must remain actor-free; found: "
            + ", ".join(str(actor.get_actor_label()) for actor in actors)
        )
    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError(f"could not save tutorial map: {MAP_PATH}")

    dirty_after = _dirty_packages()
    if dirty_after:
        raise RuntimeError(
            "tutorial map remained dirty after save: " + ", ".join(dirty_after)
        )
    saved_game_mode = world_settings.get_editor_property("default_game_mode")
    result = {
        "ok": True,
        "map": MAP_PATH,
        "created": created,
        "exists": bool(unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH)),
        "gameMode": _class_path(saved_game_mode),
        "actorCount": len(actors),
        "dirtyAfter": dirty_after,
    }
    if result["gameMode"] != EXPECTED_GAME_MODE:
        raise RuntimeError(f"tutorial GameMode drifted: {result['gameMode']}")
    return result


if __name__ == "__main__":
    print(json.dumps(create_or_update(), ensure_ascii=False, indent=2))
