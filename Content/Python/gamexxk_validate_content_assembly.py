from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_ENGINE_INI = PROJECT_ROOT / "Config" / "DefaultEngine.ini"

REQUIRED_DIRECTORIES = [
    "/Game/GameXXK",
    "/Game/GameXXK/UI",
    "/Game/GameXXK/Maps",
    "/Game/GameXXK/Characters",
    "/Game/GameXXK/Characters/Hero",
    "/Game/GameXXK/Characters/Follower",
    "/Game/GameXXK/Characters/Merchant",
    "/Game/GameXXK/Data",
    "/Game/GameXXK/Sprites",
]

REQUIRED_ASSETS = [
    "/Game/GameXXK/Maps/L_Main",
    "/Game/GameXXK/Maps/L_QingshanInn",
]

EXPECTED_MAIN_MAP = "/Game/GameXXK/Maps/L_Main"
EXPECTED_GAME_MODE = "/Script/GameXXK.GameXXKMVPGameMode"


def _read_config_value(section: str, key: str) -> str:
    if not DEFAULT_ENGINE_INI.exists():
        return ""

    b_in_section = False
    for raw_line in DEFAULT_ENGINE_INI.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith(";"):
            continue
        if line.startswith("[") and line.endswith("]"):
            b_in_section = line == section
            continue
        if b_in_section and line.startswith(f"{key}="):
            return line.split("=", 1)[1].strip()
    return ""


def main() -> None:
    missing_directories = [
        directory
        for directory in REQUIRED_DIRECTORIES
        if not unreal.EditorAssetLibrary.does_directory_exist(directory)
    ]
    missing_assets = [
        asset
        for asset in REQUIRED_ASSETS
        if not unreal.EditorAssetLibrary.does_asset_exist(asset)
    ]
    invalid_assets = []
    for asset_path in REQUIRED_ASSETS:
        if asset_path in missing_assets:
            continue
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        asset_class = asset.get_class().get_name() if asset else ""
        if asset_class != "World":
            invalid_assets.append({
                "asset": asset_path,
                "expected_class": "World",
                "actual_class": asset_class,
            })

    game_default_map = _read_config_value("[/Script/EngineSettings.GameMapsSettings]", "GameDefaultMap")
    editor_startup_map = _read_config_value("[/Script/EngineSettings.GameMapsSettings]", "EditorStartupMap")
    global_default_game_mode = _read_config_value("[/Script/EngineSettings.GameMapsSettings]", "GlobalDefaultGameMode")
    config_errors = []
    if game_default_map != EXPECTED_MAIN_MAP:
        config_errors.append({
            "key": "GameDefaultMap",
            "expected": EXPECTED_MAIN_MAP,
            "actual": game_default_map,
        })
    if editor_startup_map != EXPECTED_MAIN_MAP:
        config_errors.append({
            "key": "EditorStartupMap",
            "expected": EXPECTED_MAIN_MAP,
            "actual": editor_startup_map,
        })
    if global_default_game_mode != EXPECTED_GAME_MODE:
        config_errors.append({
            "key": "GlobalDefaultGameMode",
            "expected": EXPECTED_GAME_MODE,
            "actual": global_default_game_mode,
        })

    report = {
        "ok": not missing_directories and not missing_assets and not invalid_assets and not config_errors,
        "missing_directories": missing_directories,
        "missing_assets": missing_assets,
        "invalid_assets": invalid_assets,
        "config_errors": config_errors,
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
