"""Read-only structural validation for the tutorial 0-1 map."""

from __future__ import annotations

import json

import unreal


MAP_PATH = "/Game/GameXXK/Maps/Tutorial/L_Tutorial_0_1"
EXPECTED_GAME_MODE = "/Script/GameXXK.GameXXKTutorial01GameMode"


def _path(value: object) -> str:
    method = getattr(value, "get_path_name", None)
    return str(method()) if callable(method) else ""


def validate() -> dict:
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError(f"tutorial map is missing: {MAP_PATH}")
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError(f"tutorial map failed to load: {MAP_PATH}")
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    world = editor.get_editor_world() if editor else None
    if world is None:
        raise RuntimeError("tutorial map has no editor world")
    actors = list(actors_api.get_all_level_actors()) if actors_api else []
    game_mode = world.get_world_settings().get_editor_property(
        "default_game_mode"
    )
    dirty = [
        str(package.get_name())
        for package in (
            list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
            + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
        )
    ]
    report = {
        "ok": _path(game_mode) == EXPECTED_GAME_MODE
        and not actors
        and not dirty,
        "map": MAP_PATH,
        "gameMode": _path(game_mode),
        "actorCount": len(actors),
        "actors": [
            {
                "label": str(actor.get_actor_label()),
                "class": _path(actor.get_class()),
            }
            for actor in actors
        ],
        "dirty": sorted(dirty),
    }
    if not report["ok"]:
        raise RuntimeError(json.dumps(report, ensure_ascii=False))
    return report


if __name__ == "__main__":
    print(json.dumps(validate(), ensure_ascii=False, indent=2))
