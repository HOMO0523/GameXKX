from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
REPORT_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-route-encounter-maps-validation.json"

ROUTE_ENCOUNTER_MAPS = [
    {
        "map": "/Game/GameXXK/Maps/L_RouteEvent",
        "label": "GameXXK_RouteEvent_Interactable",
        "screen_key": "routeevent",
    },
    {
        "map": "/Game/GameXXK/Maps/L_RouteCamp",
        "label": "GameXXK_RouteCamp_Interactable",
        "screen_key": "routecamp",
    },
    {
        "map": "/Game/GameXXK/Maps/L_RouteMerchant",
        "label": "GameXXK_RouteMerchant_Interactable",
        "screen_key": "routemerchant",
    },
]

BATTLE_SCENE_MAP = {
    "map": "/Game/GameXXK/Maps/L_BattleScene",
    "label": "GameXXK_BattleScene_Presenter",
    "camera_label": "GameXXK_BattleScene_Camera",
}

ENEMY_FLIPBOOK_PATHS = {
    "default": "/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_Default",
    "money_mouse": "/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_MoneyMouse",
    "niu_huan": "/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_NiuHuan",
    "black_bear": "/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_BlackBear",
    "tiger_boss": "/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Boss_Tiger",
}

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


def _all_level_actors() -> list:
    world = _get_editor_world()
    if not world:
        return []
    if hasattr(unreal, "EditorLevelLibrary"):
        return list(unreal.EditorLevelLibrary.get_all_level_actors())
    return list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor))


def _normalise(value: str) -> str:
    return value.lower().replace("_", "").replace("::", ".")


def _find_interactable(label: str):
    for actor in _all_level_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


def _float_close(value: float, expected: float, tolerance: float = 0.5) -> bool:
    return abs(float(value) - float(expected)) <= tolerance


def _effective_camera_pitch(rotation: dict) -> float:
    pitch = float(rotation.get("pitch", 0.0))
    roll = float(rotation.get("roll", 0.0))
    return pitch if abs(pitch) > 1.0 else roll


def _camera_component(actor):
    try:
        return actor.get_camera_component()
    except Exception:
        pass
    try:
        return actor.get_editor_property("camera_component")
    except Exception:
        return None


def _validate_map(spec: dict) -> dict:
    map_path = spec["map"]
    result = {
        "map": map_path,
        "exists": bool(unreal.EditorAssetLibrary.does_asset_exist(map_path)),
        "loaded": False,
        "game_mode": "",
        "interactable": {},
        "ok": False,
    }
    if not result["exists"]:
        result["error"] = "missing map asset"
        return result

    if not unreal.EditorLoadingAndSavingUtils.load_map(map_path):
        result["error"] = "load_map failed"
        return result

    result["loaded"] = True
    world = _get_editor_world()
    if not world:
        result["error"] = "missing editor world"
        return result

    settings = world.get_world_settings()
    game_mode = settings.get_editor_property("default_game_mode")
    result["game_mode"] = _path(game_mode)
    game_mode_ok = "GameXXKFlowMapGameMode" in result["game_mode"]
    if not game_mode_ok:
        result["error"] = "map does not use GameXXKFlowMapGameMode"

    actor = _find_interactable(spec["label"])
    if actor:
        screen_text = str(actor.get_editor_property("encounter_screen"))
        result["interactable"] = {
            "label": actor.get_actor_label(),
            "class": _path(actor.get_class()),
            "encounter_screen": screen_text,
        }
    interactable_ok = bool(actor) and "GameXXKRouteEncounterSceneActor" in result["interactable"].get("class", "")
    interactable_ok = interactable_ok and spec["screen_key"] in _normalise(result["interactable"].get("encounter_screen", ""))
    if game_mode_ok and not interactable_ok:
        result["error"] = "missing or misconfigured GameXXKRouteEncounterSceneActor"

    result["ok"] = game_mode_ok and interactable_ok
    return result


def _validate_battle_scene(spec: dict) -> dict:
    map_path = spec["map"]
    result = {
        "map": map_path,
        "exists": bool(unreal.EditorAssetLibrary.does_asset_exist(map_path)),
        "loaded": False,
        "game_mode": "",
        "presenter": {},
        "camera": {},
        "ok": False,
    }
    if not result["exists"]:
        result["error"] = "missing map asset"
        return result
    if not unreal.EditorLoadingAndSavingUtils.load_map(map_path):
        result["error"] = "load_map failed"
        return result
    result["loaded"] = True
    world = _get_editor_world()
    if not world:
        result["error"] = "missing editor world"
        return result

    settings = world.get_world_settings()
    game_mode = settings.get_editor_property("default_game_mode")
    result["game_mode"] = _path(game_mode)
    game_mode_ok = "GameXXKFlowMapGameMode" in result["game_mode"]
    if not game_mode_ok:
        result["error"] = "battle scene does not use GameXXKFlowMapGameMode"

    presenter = _find_interactable(spec["label"])
    if presenter:
        result["presenter"] = {
            "label": presenter.get_actor_label(),
            "class": _path(presenter.get_class()),
        }
    presenter_ok = bool(presenter) and "GameXXKBattleScenePresenter" in result["presenter"].get("class", "")
    if game_mode_ok and not presenter_ok:
        result["error"] = "missing GameXXKBattleScenePresenter"

    camera = _find_interactable(spec["camera_label"])
    if camera:
        camera_component = None
        camera_component = _camera_component(camera)
        rotation = camera.get_actor_rotation()
        result["camera"] = {
            "label": camera.get_actor_label(),
            "class": _path(camera.get_class()),
            "location": {
                "x": camera.get_actor_location().x,
                "y": camera.get_actor_location().y,
                "z": camera.get_actor_location().z,
            },
            "rotation": {
                "pitch": rotation.pitch,
                "yaw": rotation.yaw,
                "roll": rotation.roll,
            },
            "projection_mode": str(camera_component.get_editor_property("projection_mode")) if camera_component else "",
        }
        result["camera"]["effective_pitch"] = _effective_camera_pitch(result["camera"]["rotation"])
    camera_ok = (
        bool(camera)
        and "CameraActor" in result["camera"].get("class", "")
        and _float_close(result["camera"].get("effective_pitch", 0.0), -60.0)
        and "PERSPECTIVE" in result["camera"].get("projection_mode", "").upper()
    )
    if game_mode_ok and presenter_ok and not camera_ok:
        result["error"] = "missing or misconfigured GameXXK battle scene camera"

    result["ok"] = game_mode_ok and presenter_ok and camera_ok
    return result


def _validate_enemy_visual_assets() -> dict:
    assets = {}
    for key, path in ENEMY_FLIPBOOK_PATHS.items():
        asset = unreal.EditorAssetLibrary.load_asset(path)
        assets[key] = {
            "path": path,
            "exists": bool(asset),
            "class": _path(asset.get_class()) if asset else "",
        }
    return {
        "ok": all(item["exists"] and "PaperFlipbook" in item["class"] for item in assets.values()),
        "assets": assets,
    }


def main() -> dict:
    _scan_project_assets()
    report = {
        "ok": False,
        "maps": [_validate_map(spec) for spec in ROUTE_ENCOUNTER_MAPS],
        "battle_scene": _validate_battle_scene(BATTLE_SCENE_MAP),
        "enemy_visual_assets": _validate_enemy_visual_assets(),
    }
    report["ok"] = (
        all(item.get("ok") for item in report["maps"])
        and bool(report["battle_scene"].get("ok"))
        and bool(report["enemy_visual_assets"].get("ok"))
    )
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("[GameXXK][RouteEncounterMaps] " + json.dumps(report, ensure_ascii=False))
    if not report["ok"]:
        raise RuntimeError("route encounter map validation failed")
    return report


if __name__ == "__main__":
    print(json.dumps(main(), ensure_ascii=False))
