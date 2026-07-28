"""Report battle-animation pilot asset visibility and live PIE unit state."""

from __future__ import annotations

import json
import unreal


ASSET_DIR = "/Game/GameXXK/Characters/BattleAnimationPilot/Hero/Idle/Flipbooks"
PACKAGE_PATH = f"{ASSET_DIR}/FB_Pilot_Hero_Idle"
OBJECT_PATH = f"{PACKAGE_PATH}.FB_Pilot_Hero_Idle"
PRODUCTION_HERO_PATH = "/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Idle_West"


def _path(value: object) -> str:
    return str(value.get_path_name()) if value else ""


def probe() -> dict:
    editor_loaded = unreal.EditorAssetLibrary.load_asset(PACKAGE_PATH)
    package_loaded = unreal.load_asset(PACKAGE_PATH)
    object_loaded = unreal.load_asset(OBJECT_PATH)
    production_hero = unreal.load_asset(PRODUCTION_HERO_PATH)
    production_hero_sprite = None
    if production_hero:
        keyframes = list(production_hero.get_editor_property("key_frames"))
        if keyframes:
            production_hero_sprite = keyframes[0].get_editor_property("sprite")
    production_sprite_summary = {}
    if production_hero_sprite:
        source_dimension = production_hero_sprite.get_editor_property("source_dimension")
        production_sprite_summary = {
            "path": _path(production_hero_sprite),
            "source_dimension": [float(source_dimension.x), float(source_dimension.y)],
            "pixels_per_unreal_unit": float(production_hero_sprite.get_editor_property("pixels_per_unreal_unit")),
        }
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = subsystem.get_game_world()
    units = []
    if world:
        for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
            if "BattleSceneUnitActor" not in actor.get_class().get_name():
                continue
            visual = actor.get_battle_visual_component()
            active_flipbook = visual.get_flipbook() if visual else None
            units.append(
                {
                    "unit_id": str(actor.get_unit_id()),
                    "is_enemy": bool(actor.is_enemy_unit()),
                    "actor": actor.get_path_name(),
                    "flipbook": _path(active_flipbook),
                    "looping": bool(visual.is_looping()) if visual else False,
                    "playing": bool(visual.is_playing()) if visual else False,
                    "playback_position": float(visual.get_playback_position()) if visual else -1.0,
                }
            )
    return {
        "ok": True,
        "package_path": PACKAGE_PATH,
        "object_path": OBJECT_PATH,
        "does_asset_exist": bool(unreal.EditorAssetLibrary.does_asset_exist(PACKAGE_PATH)),
        "editor_load_asset": _path(editor_loaded),
        "unreal_load_package": _path(package_loaded),
        "unreal_load_object": _path(object_loaded),
        "production_hero_sprite": production_sprite_summary,
        "list_assets": list(unreal.EditorAssetLibrary.list_assets(ASSET_DIR, recursive=True, include_folder=False)),
        "pie_world": _path(world),
        "units": units,
    }


if __name__ == "__main__":
    print(json.dumps(probe(), ensure_ascii=False))
