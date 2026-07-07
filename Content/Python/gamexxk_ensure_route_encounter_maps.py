from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
REPORT_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-route-encounter-maps-ensure.json"

ROUTE_ENCOUNTER_MAPS = [
    {
        "map": "/Game/GameXXK/Maps/L_RouteEvent",
        "label": "GameXXK_RouteEvent_Interactable",
        "screen_candidates": ["ROUTE_EVENT", "RouteEvent", "route_event"],
        "location": unreal.Vector(0.0, 0.0, 90.0),
    },
    {
        "map": "/Game/GameXXK/Maps/L_RouteCamp",
        "label": "GameXXK_RouteCamp_Interactable",
        "screen_candidates": ["ROUTE_CAMP", "RouteCamp", "route_camp"],
        "location": unreal.Vector(0.0, 0.0, 90.0),
    },
    {
        "map": "/Game/GameXXK/Maps/L_RouteMerchant",
        "label": "GameXXK_RouteMerchant_Interactable",
        "screen_candidates": ["ROUTE_MERCHANT", "RouteMerchant", "route_merchant"],
        "location": unreal.Vector(0.0, 0.0, 90.0),
    },
]

BATTLE_SCENE_MAP = {
    "map": "/Game/GameXXK/Maps/L_BattleScene",
    "label": "GameXXK_BattleScene_Presenter",
    "location": unreal.Vector(0.0, 0.0, 90.0),
    "camera_label": "GameXXK_BattleScene_Camera",
    "camera_tag": "GameXXK_BattleScene_Camera",
    "camera_location": unreal.Vector(-420.0, 0.0, 720.0),
    "camera_rotation": {"pitch": -60.0, "yaw": 0.0, "roll": 0.0},
    "camera_fov": 55.0,
}

ENEMY_TEXTURE_DIR = "/Game/GameXXK/Characters/Enemies/Textures"
ENEMY_SPRITE_DIR = "/Game/GameXXK/Characters/Enemies/Sprites"
ENEMY_FLIPBOOK_DIR = "/Game/GameXXK/Characters/Enemies/Flipbooks"
ENEMY_VISUAL_SPECS = [
    {
        "key": "default",
        "source": PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "enemy_money_mouse.png",
        "texture": "T_Enemy_Default",
        "sprite": "SPR_Enemy_Default",
        "flipbook": "FB_Enemy_Default",
    },
    {
        "key": "money_mouse",
        "source": PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "enemy_money_mouse.png",
        "texture": "T_Enemy_MoneyMouse",
        "sprite": "SPR_Enemy_MoneyMouse",
        "flipbook": "FB_Enemy_MoneyMouse",
    },
    {
        "key": "niu_huan",
        "source": PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "enemy_niu_huan.png",
        "texture": "T_Enemy_NiuHuan",
        "sprite": "SPR_Enemy_NiuHuan",
        "flipbook": "FB_Enemy_NiuHuan",
    },
    {
        "key": "black_bear",
        "source": PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "enemy_black_bear.png",
        "texture": "T_Enemy_BlackBear",
        "sprite": "SPR_Enemy_BlackBear",
        "flipbook": "FB_Enemy_BlackBear",
    },
    {
        "key": "tiger_boss",
        "source": PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "enemy_tiger_boss.png",
        "texture": "T_Boss_Tiger",
        "sprite": "SPR_Boss_Tiger",
        "flipbook": "FB_Boss_Tiger",
    },
]

ASSET_SCAN_PATHS = [
    "/Game/GameXXK/Maps",
    "/Game/GameXXK/Characters/Enemies",
    "/Game/GameXXK/Characters/Enemies/Textures",
    "/Game/1Game/Texture",
]


def _path(obj) -> str:
    return obj.get_path_name() if obj else ""


def _scan_project_assets() -> None:
    try:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        registry.scan_paths_synchronous(ASSET_SCAN_PATHS, True)
    except Exception:
        pass


def _get_editor_world():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if subsystem and hasattr(subsystem, "get_editor_world"):
        return subsystem.get_editor_world()
    if hasattr(unreal, "EditorLevelLibrary"):
        return unreal.EditorLevelLibrary.get_editor_world()
    return None


def _load_map(map_path: str) -> None:
    if not unreal.EditorLoadingAndSavingUtils.load_map(map_path):
        raise RuntimeError(f"Could not load map: {map_path}")


def _new_level(map_path: str) -> bool:
    if hasattr(unreal.EditorLevelLibrary, "new_level"):
        return bool(unreal.EditorLevelLibrary.new_level(map_path))
    if hasattr(unreal.EditorLoadingAndSavingUtils, "new_blank_map"):
        world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        if not world:
            return False
        return bool(unreal.EditorLoadingAndSavingUtils.save_map(world, map_path))
    return False


def _all_level_actors() -> list:
    world = _get_editor_world()
    if not world:
        return []
    if hasattr(unreal, "EditorLevelLibrary"):
        return list(unreal.EditorLevelLibrary.get_all_level_actors())
    return list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor))


def _actor_by_label(label: str):
    for actor in _all_level_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


def _actor_exists(label: str) -> bool:
    return _actor_by_label(label) is not None


def _ensure_directory(path: str) -> bool:
    if unreal.EditorAssetLibrary.does_directory_exist(path):
        return False
    return bool(unreal.EditorAssetLibrary.make_directory(path))


def _create_or_load_asset(asset_name: str, package_path: str, asset_class, factory):
    asset_path = f"{package_path}/{asset_name}"
    existing = unreal.EditorAssetLibrary.load_asset(asset_path)
    if existing:
        return existing, False
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(asset_name, package_path, asset_class, factory)
    if not asset:
        raise RuntimeError(f"Could not create asset {asset_path}")
    return asset, True


def _set_prop(obj, prop_name: str, value) -> None:
    try:
        obj.set_editor_property(prop_name, value)
    except Exception as exc:
        raise RuntimeError(f"Could not set {prop_name} on {obj}: {exc}") from exc


def _texture_size(texture) -> tuple[float, float]:
    for x_name, y_name in (("blueprint_get_size_x", "blueprint_get_size_y"), ("get_size_x", "get_size_y")):
        try:
            return float(getattr(texture, x_name)()), float(getattr(texture, y_name)())
        except Exception:
            pass
    return 128.0, 128.0


def _configure_texture(texture) -> None:
    texture_settings = {
        "mip_gen_settings": ("TextureMipGenSettings", "TMGS_NO_MIPMAPS"),
        "filter": ("TextureFilter", "TF_NEAREST"),
    }
    for prop, enum_info in texture_settings.items():
        enum_name, value_name = enum_info
        enum_type = getattr(unreal, enum_name, None)
        if enum_type is not None and hasattr(enum_type, value_name):
            texture.set_editor_property(prop, getattr(enum_type, value_name))


def _import_texture(source_file: Path, destination_name: str) -> object:
    if not source_file.exists():
        raise RuntimeError(f"Missing enemy texture source: {source_file}")
    _ensure_directory(ENEMY_TEXTURE_DIR)
    task = unreal.AssetImportTask()
    task.filename = str(source_file)
    task.destination_path = ENEMY_TEXTURE_DIR
    task.destination_name = destination_name
    task.automated = True
    task.save = False
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture_path = f"{ENEMY_TEXTURE_DIR}/{destination_name}"
    texture = unreal.EditorAssetLibrary.load_asset(texture_path)
    if not texture:
        raise RuntimeError(f"Could not load imported enemy texture: {texture_path}")
    _configure_texture(texture)
    return texture


def _make_rotator(rotation: dict) -> unreal.Rotator:
    rotator = unreal.Rotator()
    rotator.pitch = float(rotation.get("pitch", 0.0))
    rotator.yaw = float(rotation.get("yaw", 0.0))
    rotator.roll = float(rotation.get("roll", 0.0))
    return rotator


def _set_flipbook_keyframes(flipbook, sprites: list[object], frames_per_second: float) -> None:
    keyframes = []
    for sprite in sprites:
        keyframe = unreal.PaperFlipbookKeyFrame()
        keyframe.set_editor_property("sprite", sprite)
        keyframe.set_editor_property("frame_run", 1)
        keyframes.append(keyframe)
    flipbook.set_editor_property("frames_per_second", frames_per_second)
    flipbook.set_editor_property("key_frames", keyframes)
    invalidate = getattr(flipbook, "invalidate_cached_data", None)
    if invalidate:
        invalidate()


def _ensure_enemy_visual_assets() -> dict:
    _ensure_directory(ENEMY_TEXTURE_DIR)
    _ensure_directory(ENEMY_SPRITE_DIR)
    _ensure_directory(ENEMY_FLIPBOOK_DIR)

    assets = {}
    for spec in ENEMY_VISUAL_SPECS:
        texture = _import_texture(spec["source"], spec["texture"])
        texture_width, texture_height = _texture_size(texture)

        sprite, sprite_created = _create_or_load_asset(spec["sprite"], ENEMY_SPRITE_DIR, unreal.PaperSprite, unreal.PaperSpriteFactory())
        _set_prop(sprite, "source_texture", texture)
        _set_prop(sprite, "source_uv", unreal.Vector2D(0.0, 0.0))
        _set_prop(sprite, "source_dimension", unreal.Vector2D(texture_width, texture_height))
        _set_prop(sprite, "source_texture_dimension", unreal.Vector2D(texture_width, texture_height))
        _set_prop(sprite, "pixels_per_unreal_unit", 1.0)
        _set_prop(sprite, "pivot_mode", unreal.SpritePivotMode.BOTTOM_CENTER)
        _set_prop(sprite, "custom_pivot_point", unreal.Vector2D(texture_width * 0.5, texture_height))

        flipbook, flipbook_created = _create_or_load_asset(spec["flipbook"], ENEMY_FLIPBOOK_DIR, unreal.PaperFlipbook, unreal.PaperFlipbookFactory())
        _set_flipbook_keyframes(flipbook, [sprite], 1.0)
        assets[spec["key"]] = {
            "texture": f"{ENEMY_TEXTURE_DIR}/{spec['texture']}",
            "sprite": f"{ENEMY_SPRITE_DIR}/{spec['sprite']}",
            "sprite_created": sprite_created,
            "flipbook": f"{ENEMY_FLIPBOOK_DIR}/{spec['flipbook']}",
            "flipbook_created": flipbook_created,
            "texture_size": {"x": texture_width, "y": texture_height},
        }

    unreal.EditorAssetLibrary.save_directory(ENEMY_TEXTURE_DIR)
    unreal.EditorAssetLibrary.save_directory(ENEMY_SPRITE_DIR)
    unreal.EditorAssetLibrary.save_directory(ENEMY_FLIPBOOK_DIR)
    return assets


def _enum_value(enum_type, candidates: list[str]):
    for candidate in candidates:
        if hasattr(enum_type, candidate):
            return getattr(enum_type, candidate)
    available = [name for name in dir(enum_type) if not name.startswith("_")]
    raise RuntimeError(f"Could not resolve EGameXXKScreen enum from {candidates}; available={available}")


def _spawn_scene_markers(map_path: str) -> dict:
    spawned = []
    floor_mesh = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane")
    if floor_mesh and not _actor_exists("GameXXK_Encounter_Floor"):
        floor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, 0.0))
        if floor:
            floor.set_actor_label("GameXXK_Encounter_Floor")
            floor.set_actor_scale3d(unreal.Vector(24.0, 14.0, 1.0))
            floor_component = floor.get_editor_property("static_mesh_component")
            if floor_component:
                floor_component.set_static_mesh(floor_mesh)
            spawned.append(floor.get_actor_label())

    if not _actor_exists("GameXXK_Encounter_Light"):
        light = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(-300.0, -300.0, 500.0))
        if light:
            light.set_actor_label("GameXXK_Encounter_Light")
            light.set_actor_rotation(unreal.Rotator(-45.0, -35.0, 0.0), False)
            spawned.append(light.get_actor_label())

    if not _actor_exists("GameXXK_Encounter_PlayerStart"):
        player_start = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(220.0, 0.0, 80.0))
        if player_start:
            player_start.set_actor_label("GameXXK_Encounter_PlayerStart")
            spawned.append(player_start.get_actor_label())

    return {
        "map": map_path,
        "spawned": spawned,
    }


def _ensure_interactable(spec: dict) -> dict:
    actor_class = getattr(unreal, "GameXXKRouteEncounterSceneActor", None)
    if not actor_class:
        raise RuntimeError("GameXXKRouteEncounterSceneActor is not available; compile C++ before running map ensure")

    label = spec["label"]
    actor = _actor_by_label(label)
    spawned = False
    if not actor:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            actor_class.static_class(),
            spec["location"],
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        if not actor:
            raise RuntimeError(f"Could not spawn {label}")
        actor.set_actor_label(label)
        spawned = True

    screen = _enum_value(unreal.GameXXKScreen, spec["screen_candidates"])
    actor.set_editor_property("encounter_screen", screen)
    actor.set_actor_location(spec["location"], False, False)
    return {
        "label": label,
        "spawned": spawned,
        "class": _path(actor.get_class()),
        "encounter_screen": str(actor.get_editor_property("encounter_screen")),
        "location": {
            "x": actor.get_actor_location().x,
            "y": actor.get_actor_location().y,
            "z": actor.get_actor_location().z,
        },
    }


def _ensure_battle_presenter(spec: dict) -> dict:
    actor_class = getattr(unreal, "GameXXKBattleScenePresenter", None)
    if not actor_class:
        raise RuntimeError("GameXXKBattleScenePresenter is not available; compile C++ before running map ensure")

    label = spec["label"]
    actor = _actor_by_label(label)
    spawned = False
    if not actor:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            actor_class.static_class(),
            spec["location"],
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        if not actor:
            raise RuntimeError(f"Could not spawn {label}")
        actor.set_actor_label(label)
        spawned = True
    actor.set_actor_location(spec["location"], False, False)
    return {
        "label": label,
        "spawned": spawned,
        "class": _path(actor.get_class()),
        "location": {
            "x": actor.get_actor_location().x,
            "y": actor.get_actor_location().y,
            "z": actor.get_actor_location().z,
        },
    }


def _ensure_actor_tag(actor, tag: str) -> None:
    tags = []
    try:
        tags = list(actor.get_editor_property("tags"))
    except Exception:
        tags = []
    tag_name = unreal.Name(tag)
    if tag_name not in tags:
        tags.append(tag_name)
        actor.set_editor_property("tags", tags)


def _camera_component(actor):
    try:
        return actor.get_camera_component()
    except Exception:
        pass
    try:
        return actor.get_editor_property("camera_component")
    except Exception:
        return None


def _ensure_battle_camera(spec: dict) -> dict:
    label = spec["camera_label"]
    actor = _actor_by_label(label)
    spawned = False
    if not actor:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.CameraActor,
            spec["camera_location"],
            _make_rotator(spec["camera_rotation"]),
        )
        if not actor:
            raise RuntimeError(f"Could not spawn {label}")
        actor.set_actor_label(label)
        spawned = True

    actor.set_actor_location(spec["camera_location"], False, False)
    actor.set_actor_rotation(_make_rotator(spec["camera_rotation"]), False)
    _ensure_actor_tag(actor, spec["camera_tag"])

    camera_component = _camera_component(actor)
    if camera_component:
        camera_component.set_editor_property("projection_mode", unreal.CameraProjectionMode.PERSPECTIVE)
        camera_component.set_editor_property("field_of_view", float(spec["camera_fov"]))

    rotation = actor.get_actor_rotation()
    return {
        "label": label,
        "spawned": spawned,
        "class": _path(actor.get_class()),
        "tag": spec["camera_tag"],
        "location": {
            "x": actor.get_actor_location().x,
            "y": actor.get_actor_location().y,
            "z": actor.get_actor_location().z,
        },
        "rotation": {
            "pitch": rotation.pitch,
            "yaw": rotation.yaw,
            "roll": rotation.roll,
        },
        "fov": float(spec["camera_fov"]),
    }


def _ensure_map(spec: dict) -> dict:
    map_path = spec["map"]
    created = False
    if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
        created = _new_level(map_path)
        if not created:
            raise RuntimeError(f"Could not create map: {map_path}")
    else:
        _load_map(map_path)

    world = _get_editor_world()
    if not world:
        raise RuntimeError(f"Missing editor world for {map_path}")

    world_settings = world.get_world_settings()
    game_mode_class = unreal.GameXXKFlowMapGameMode.static_class()
    world_settings.set_editor_property("default_game_mode", game_mode_class)
    markers = _spawn_scene_markers(map_path)
    interactable = _ensure_interactable(spec)
    saved = bool(unreal.EditorLoadingAndSavingUtils.save_current_level())
    return {
        "map": map_path,
        "created": created,
        "game_mode": _path(game_mode_class),
        "markers": markers,
        "interactable": interactable,
        "saved": saved,
        "exists": bool(unreal.EditorAssetLibrary.does_asset_exist(map_path)),
    }


def _ensure_battle_scene_map(spec: dict) -> dict:
    map_path = spec["map"]
    created = False
    if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
        created = _new_level(map_path)
        if not created:
            raise RuntimeError(f"Could not create map: {map_path}")
    else:
        _load_map(map_path)

    world = _get_editor_world()
    if not world:
        raise RuntimeError(f"Missing editor world for {map_path}")

    world_settings = world.get_world_settings()
    game_mode_class = unreal.GameXXKFlowMapGameMode.static_class()
    world_settings.set_editor_property("default_game_mode", game_mode_class)
    markers = _spawn_scene_markers(map_path)
    presenter = _ensure_battle_presenter(spec)
    camera = _ensure_battle_camera(spec)
    saved = bool(unreal.EditorLoadingAndSavingUtils.save_current_level())
    return {
        "map": map_path,
        "created": created,
        "game_mode": _path(game_mode_class),
        "markers": markers,
        "presenter": presenter,
        "camera": camera,
        "saved": saved,
        "exists": bool(unreal.EditorAssetLibrary.does_asset_exist(map_path)),
    }


def main() -> dict:
    _scan_project_assets()
    report = {
        "ok": False,
        "maps": [],
        "battle_scene": {},
        "enemy_visual_assets": {},
    }
    try:
        report["enemy_visual_assets"] = _ensure_enemy_visual_assets()
        for spec in ROUTE_ENCOUNTER_MAPS:
            report["maps"].append(_ensure_map(spec))
        report["battle_scene"] = _ensure_battle_scene_map(BATTLE_SCENE_MAP)
        report["save_dirty_packages"] = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        report["ok"] = (
            all(item.get("exists") and item.get("saved") and item.get("interactable") for item in report["maps"])
            and bool(report["battle_scene"].get("exists"))
            and bool(report["battle_scene"].get("saved"))
            and bool(report["battle_scene"].get("presenter"))
            and bool(report["battle_scene"].get("camera"))
            and all(bool(report["enemy_visual_assets"].get(spec["key"], {}).get("flipbook")) for spec in ENEMY_VISUAL_SPECS)
        )
    except Exception as exc:
        report["error"] = str(exc)

    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[GameXXK][EnsureRouteEncounterMaps] " + json.dumps(report, ensure_ascii=False))
    if not report["ok"]:
        raise RuntimeError(report.get("error") or "route encounter map ensure failed")
    return report


if __name__ == "__main__":
    print(json.dumps(main(), ensure_ascii=False))
