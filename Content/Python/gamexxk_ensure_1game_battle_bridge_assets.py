from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
REPORT_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-1game-battle-bridge-assets.json"

ONEGAME_PLAYER_CONTROLLER = "/Game/1Game/Control/BP_PlayerController"
ONEGAME_GAME_MODE = "/Game/1Game/Control/BP_Gamemode"
GAMEXXK_BATTLE_DIR = "/Game/GameXXK/Battle"
GAMEXXK_BATTLE_PLAYER_CONTROLLER = "/Game/GameXXK/Battle/BP_1GameBattlePlayerController"
GAMEXXK_BATTLE_GAME_MODE = "/Game/GameXXK/Battle/BP_1GameBattleGameMode"

MAIN_MAP = "/Game/GameXXK/Maps/L_Main"
ROUTE_MAP = "/Game/GameXXK/Maps/L_RouteMap"
ONEGAME_NEW_WORLD = "/Game/1Game/Map/NewWorld"
BATTLE_MAP = "/Game/GameXXK/Maps/L_Battle_1Game"


def _write_report(report: dict) -> None:
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"ok": report.get("ok", False), "report": str(REPORT_PATH)}, ensure_ascii=False))


def _load_asset(asset_path: str):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset:
        raise RuntimeError(f"Could not load asset: {asset_path}")
    return asset


def _class_path(value) -> str:
    if value is None:
        return ""
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def _load_blueprint_class(asset_path: str):
    if hasattr(unreal.EditorAssetLibrary, "load_blueprint_class"):
        loaded_class = unreal.EditorAssetLibrary.load_blueprint_class(asset_path)
        if loaded_class:
            return loaded_class

    asset_name = asset_path.rsplit("/", 1)[-1]
    loaded_class = unreal.load_class(None, f"{asset_path}.{asset_name}_C")
    if not loaded_class:
        raise RuntimeError(f"Could not load generated class for blueprint: {asset_path}")
    return loaded_class


def _get_property(obj, *names):
    if obj is None:
        return None
    for name in names:
        try:
            return obj.get_editor_property(name)
        except Exception:
            pass
        try:
            value = getattr(obj, name)
            return value() if callable(value) else value
        except Exception:
            pass
    return None


def _set_property(obj, value, *names) -> str:
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return name
        except Exception:
            pass
        try:
            setattr(obj, name, value)
            return name
        except Exception:
            pass
    raise RuntimeError(f"Could not set any of properties: {names}")


def _ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def _ensure_duplicate(source_path: str, target_path: str) -> dict:
    created = False
    if not unreal.EditorAssetLibrary.does_asset_exist(target_path):
        target_dir = target_path.rsplit("/", 1)[0]
        _ensure_directory(target_dir)
        asset = unreal.EditorAssetLibrary.duplicate_asset(source_path, target_path)
        if not asset:
            raise RuntimeError(f"Could not duplicate {source_path} -> {target_path}")
        created = True
    else:
        asset = _load_asset(target_path)

    saved = bool(unreal.EditorAssetLibrary.save_asset(target_path, only_if_is_dirty=False))
    return {
        "source": source_path,
        "target": target_path,
        "created": created,
        "class": asset.get_class().get_path_name() if asset else "",
        "saved": saved,
    }


def _compile_blueprint(asset) -> None:
    if hasattr(unreal, "BlueprintEditorLibrary"):
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        return
    if hasattr(unreal, "KismetEditorUtilities"):
        unreal.KismetEditorUtilities.compile_blueprint(asset)
        return
    raise RuntimeError("No Blueprint compile API is available")


def _reparent_bridge_player_controller() -> dict:
    asset = _load_asset(GAMEXXK_BATTLE_PLAYER_CONTROLLER)
    report = {
        "asset": GAMEXXK_BATTLE_PLAYER_CONTROLLER,
        "reparented": False,
        "compiled": False,
        "saved": False,
    }

    if not hasattr(unreal, "BlueprintEditorLibrary"):
        raise RuntimeError("BlueprintEditorLibrary is required to modify BP_1GameBattlePlayerController")

    desired_parent = unreal.GameXXKMVPPlayerController.static_class()
    current_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset)
    report["parent_before"] = current_parent.get_path_name() if current_parent else ""
    report["desired_parent"] = desired_parent.get_path_name()

    if current_parent != desired_parent:
        unreal.BlueprintEditorLibrary.reparent_blueprint(asset, desired_parent)
        report["reparented"] = True

    _compile_blueprint(asset)
    report["compiled"] = True
    report["parent_after"] = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset).get_path_name()
    report["saved"] = bool(unreal.EditorAssetLibrary.save_asset(GAMEXXK_BATTLE_PLAYER_CONTROLLER, only_if_is_dirty=False))
    return report


def _configure_bridge_game_mode() -> dict:
    asset = _load_asset(GAMEXXK_BATTLE_GAME_MODE)
    game_mode_class = _load_blueprint_class(GAMEXXK_BATTLE_GAME_MODE)
    player_controller_class = _load_blueprint_class(GAMEXXK_BATTLE_PLAYER_CONTROLLER)
    game_mode_cdo = unreal.get_default_object(game_mode_class)
    before = _get_property(game_mode_cdo, "PlayerControllerClass", "player_controller_class")
    set_property_name = _set_property(
        game_mode_cdo,
        player_controller_class,
        "PlayerControllerClass",
        "player_controller_class",
    )

    _compile_blueprint(asset)
    return {
        "asset": GAMEXXK_BATTLE_GAME_MODE,
        "game_mode_class": _class_path(game_mode_class),
        "player_controller_before": _class_path(before),
        "player_controller_after": _class_path(_get_property(game_mode_cdo, "PlayerControllerClass", "player_controller_class")),
        "set_property": set_property_name,
        "compiled": True,
        "saved": bool(unreal.EditorAssetLibrary.save_asset(GAMEXXK_BATTLE_GAME_MODE, only_if_is_dirty=False)),
    }


def _get_editor_world():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if subsystem and hasattr(subsystem, "get_editor_world"):
        return subsystem.get_editor_world()
    if hasattr(unreal, "EditorLevelLibrary"):
        return unreal.EditorLevelLibrary.get_editor_world()
    return None


def _current_map_name() -> str:
    world = _get_editor_world()
    outermost = world.get_outermost() if world else None
    return outermost.get_name() if outermost else ""


def _load_map(map_path: str) -> None:
    current_map = _current_map_name()
    if current_map == map_path or current_map.endswith("/" + map_path.rsplit("/", 1)[-1]):
        return
    loaded = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if not loaded:
        raise RuntimeError(f"Could not load map: {map_path}")


def _set_map_game_mode(map_path: str, game_mode_class) -> dict:
    _load_map(map_path)
    world = _get_editor_world()
    if not world:
        raise RuntimeError(f"Could not get editor world for {map_path}")

    world_settings = world.get_world_settings()
    world_settings.set_editor_property("default_game_mode", game_mode_class)
    saved = bool(unreal.EditorLoadingAndSavingUtils.save_current_level())
    return {
        "map": map_path,
        "game_mode": game_mode_class.get_path_name() if game_mode_class else "",
        "saved": saved,
    }


def _phase_from_argv() -> str:
    args = list(sys.argv[1:])
    if "--phase" in args:
        index = args.index("--phase")
        if index + 1 < len(args):
            return args[index + 1]
    return "copy"


def _copy_assets(report: dict) -> None:
    report["copied_assets"].append(_ensure_duplicate(ONEGAME_PLAYER_CONTROLLER, GAMEXXK_BATTLE_PLAYER_CONTROLLER))
    report["blueprint"] = _reparent_bridge_player_controller()
    report["copied_assets"].append(_ensure_duplicate(ONEGAME_GAME_MODE, GAMEXXK_BATTLE_GAME_MODE))
    report["game_mode_blueprint"] = _configure_bridge_game_mode()
    report["copied_assets"].append(_ensure_duplicate(MAIN_MAP, ROUTE_MAP))
    report["copied_assets"].append(_ensure_duplicate(ONEGAME_NEW_WORLD, BATTLE_MAP))
    report["save_dirty_packages"] = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    report["ok"] = all(
        item.get("target") and unreal.EditorAssetLibrary.does_asset_exist(item["target"]) and item.get("saved")
        for item in report["copied_assets"]
    ) and bool(report["blueprint"].get("saved")) and bool(report["game_mode_blueprint"].get("saved"))


def _configure_map(report: dict, map_path: str, game_mode_class) -> None:
    report["maps"].append(_set_map_game_mode(map_path, game_mode_class))
    report["save_dirty_packages"] = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    report["ok"] = all(item.get("saved") for item in report["maps"])


def main() -> None:
    phase = _phase_from_argv()
    report: dict = {
        "ok": False,
        "phase": phase,
        "copied_assets": [],
        "blueprint": {},
        "game_mode_blueprint": {},
        "maps": [],
        "original_assets": {
            "player_controller": ONEGAME_PLAYER_CONTROLLER,
            "game_mode": ONEGAME_GAME_MODE,
            "new_world": ONEGAME_NEW_WORLD,
        },
    }

    try:
        if phase == "copy":
            _copy_assets(report)
        elif phase == "configure-route":
            _configure_map(report, ROUTE_MAP, unreal.GameXXKFlowMapGameMode.static_class())
        elif phase == "configure-battle":
            _configure_map(report, BATTLE_MAP, _load_blueprint_class(GAMEXXK_BATTLE_GAME_MODE))
        elif phase == "configure-battle-flow":
            _configure_map(report, BATTLE_MAP, unreal.GameXXKFlowMapGameMode.static_class())
        else:
            raise RuntimeError(f"Unknown phase: {phase}")
    except Exception as exc:
        report["error"] = str(exc)

    _write_report(report)
    if not report["ok"]:
        raise RuntimeError(report.get("error") or "1Game battle bridge asset setup failed")


if __name__ == "__main__":
    main()
