from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
REPORT_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-main-menu-validate.json"

MAIN_MAP = "/Game/GameXXK/Maps/L_Main"
TEXTURE_PATHS = [
    "/Game/GameXXK/UI/MainMenu/Textures/T_MainMenuCover",
    "/Game/GameXXK/UI/MainMenu/Textures/T_InkButtonBase",
]


def _load_main_map() -> None:
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAIN_MAP):
        raise RuntimeError(f"Could not load main menu map: {MAIN_MAP}")


def _get_editor_world():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if subsystem and hasattr(subsystem, "get_editor_world"):
        return subsystem.get_editor_world()
    if hasattr(unreal, "EditorLevelLibrary"):
        return unreal.EditorLevelLibrary.get_editor_world()
    return None


def main() -> dict:
    missing_textures = [
        path for path in TEXTURE_PATHS
        if not unreal.EditorAssetLibrary.load_asset(path)
    ]

    _load_main_map()
    world = _get_editor_world()
    if not world:
        raise RuntimeError("Could not resolve editor world for L_Main")
    world_settings = world.get_world_settings()
    game_mode = world_settings.get_editor_property("default_game_mode") if world_settings else None
    game_mode_path = game_mode.get_path_name() if game_mode else ""

    errors = []
    if missing_textures:
        errors.append(f"missing textures: {', '.join(missing_textures)}")
    if "GameXXKFlowMapGameMode" not in game_mode_path:
        errors.append(f"L_Main default game mode is not GameXXKFlowMapGameMode: {game_mode_path}")

    report = {
        "status": "ok" if not errors else "error",
        "errors": errors,
        "textures": TEXTURE_PATHS,
        "main_map": MAIN_MAP,
        "main_map_default_game_mode": game_mode_path,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    if errors:
        raise RuntimeError("; ".join(errors))
    return report


if __name__ == "__main__":
    main()
