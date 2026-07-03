from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
REPORT_PATH = PROJECT_ROOT / "Saved" / "AssetAnalysis" / "1game_battle_ui_bridge_probe.json"

BATTLE_MAP = "/Game/GameXXK/Maps/L_Battle_1Game"
ORIGINAL_ONEGAME_MAP = "/Game/1Game/Map/NewWorld"
COPIED_PLAYER_CONTROLLER = "/Game/GameXXK/Battle/BP_1GameBattlePlayerController"
ORIGINAL_PLAYER_CONTROLLER = "/Game/1Game/Control/BP_PlayerController"
ORIGINAL_GAME_MODE = "/Game/1Game/Control/BP_Gamemode"


def _class_path(value) -> str:
    if value is None:
        return ""
    try:
        return value.get_path_name()
    except Exception:
        pass
    try:
        return value.get_class().get_path_name()
    except Exception:
        return str(value)


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


def _load_asset(path: str):
    return unreal.EditorAssetLibrary.load_asset(path)


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


def _load_map(path: str) -> None:
    current = _current_map_name()
    if current == path or current.endswith("/" + path.rsplit("/", 1)[-1]):
        return
    if not unreal.EditorLoadingAndSavingUtils.load_map(path):
        raise RuntimeError(f"Could not load map: {path}")


def _world_settings_report(path: str) -> dict:
    _load_map(path)
    world = _get_editor_world()
    if world is None:
        raise RuntimeError(f"No editor world after loading {path}")
    world_settings = world.get_world_settings()
    game_mode = world_settings.get_editor_property("default_game_mode")
    cdo = unreal.get_default_object(game_mode) if game_mode else None
    player_controller = _get_property(cdo, "PlayerControllerClass", "player_controller_class")
    hud = _get_property(cdo, "HUDClass", "hud_class")
    pawn = _get_property(cdo, "DefaultPawnClass", "default_pawn_class")
    return {
        "map": path,
        "current_map": _current_map_name(),
        "world_settings_game_mode": _class_path(game_mode),
        "game_mode_default_player_controller": _class_path(player_controller),
        "game_mode_default_hud": _class_path(hud),
        "game_mode_default_pawn": _class_path(pawn),
    }


def _blueprint_parent(path: str) -> dict:
    asset = _load_asset(path)
    report = {"asset": path, "exists": bool(asset), "asset_class": _class_path(asset)}
    if asset and hasattr(unreal, "BlueprintEditorLibrary"):
        parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset)
        report["parent_class"] = _class_path(parent)
    return report


def main() -> None:
    report = {
        "ok": True,
        "battle_map": _world_settings_report(BATTLE_MAP),
        "original_onegame_map": _world_settings_report(ORIGINAL_ONEGAME_MAP),
        "copied_player_controller": _blueprint_parent(COPIED_PLAYER_CONTROLLER),
        "original_player_controller": _blueprint_parent(ORIGINAL_PLAYER_CONTROLLER),
        "original_game_mode": _blueprint_parent(ORIGINAL_GAME_MODE),
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"ok": True, "report": str(REPORT_PATH), "summary": report}, ensure_ascii=False))


if __name__ == "__main__":
    main()
