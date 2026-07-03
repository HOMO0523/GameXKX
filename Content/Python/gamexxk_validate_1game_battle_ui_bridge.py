from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
REPORT_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-1game-battle-ui-bridge-validation.json"

BATTLE_MAP = "/Game/GameXXK/Maps/L_Battle_1Game"
COPIED_GAME_MODE_CLASS = "/Game/GameXXK/Battle/BP_1GameBattleGameMode.BP_1GameBattleGameMode_C"
COPIED_PLAYER_CONTROLLER_CLASS = "/Game/GameXXK/Battle/BP_1GameBattlePlayerController.BP_1GameBattlePlayerController_C"


def _class_path(value) -> str:
    if value is None:
        return ""
    try:
        return value.get_path_name()
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


def main() -> None:
    _load_map(BATTLE_MAP)
    world = _get_editor_world()
    world_settings = world.get_world_settings() if world else None
    game_mode = _get_property(world_settings, "default_game_mode") if world_settings else None
    game_mode_cdo = unreal.get_default_object(game_mode) if game_mode else None
    player_controller = _get_property(game_mode_cdo, "PlayerControllerClass", "player_controller_class")
    actual_game_mode = _class_path(game_mode)
    actual_player_controller = _class_path(player_controller)

    result = {
        "ok": actual_game_mode == COPIED_GAME_MODE_CLASS and actual_player_controller == COPIED_PLAYER_CONTROLLER_CLASS,
        "map": BATTLE_MAP,
        "game_mode": actual_game_mode,
        "expected_game_mode": COPIED_GAME_MODE_CLASS,
        "player_controller": actual_player_controller,
        "expected_player_controller": COPIED_PLAYER_CONTROLLER_CLASS,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))
    if not result["ok"]:
        raise RuntimeError("L_Battle_1Game is not using the copied 1Game battle UI bridge")


if __name__ == "__main__":
    main()
