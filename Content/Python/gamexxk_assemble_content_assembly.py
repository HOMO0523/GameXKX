from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_ENGINE_INI = PROJECT_ROOT / "Config" / "DefaultEngine.ini"

CONTENT_DIRECTORIES = [
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

MAP_ASSETS = [
    "/Game/GameXXK/Maps/L_Main",
    "/Game/GameXXK/Maps/L_QingshanInn",
]

GAME_MAPS_SECTION = "[/Script/EngineSettings.GameMapsSettings]"
MAIN_MAP = "/Game/GameXXK/Maps/L_Main"


def _ensure_directories() -> list[str]:
    created = []
    for directory in CONTENT_DIRECTORIES:
        if not unreal.EditorAssetLibrary.does_directory_exist(directory):
            unreal.EditorAssetLibrary.make_directory(directory)
            created.append(directory)
    return created


def _ensure_maps() -> list[str]:
    created = []
    for map_asset in MAP_ASSETS:
        if unreal.EditorAssetLibrary.does_asset_exist(map_asset):
            continue
        if not unreal.EditorLevelLibrary.new_level(map_asset):
            raise RuntimeError(f"EditorLevelLibrary.new_level failed for {map_asset}")
        created.append(map_asset)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    return created


def _set_or_insert_section_values(lines: list[str], section: str, values: dict[str, str]) -> list[str]:
    result = list(lines)
    try:
        section_index = next(index for index, line in enumerate(result) if line.strip() == section)
    except StopIteration:
        if result and result[-1].strip():
            result.append("")
        result.append(section)
        for key, value in values.items():
            result.append(f"{key}={value}")
        return result

    next_section_index = len(result)
    for index in range(section_index + 1, len(result)):
        stripped = result[index].strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            next_section_index = index
            break

    seen_keys = set()
    for index in range(section_index + 1, next_section_index):
        stripped = result[index].strip()
        for key, value in values.items():
            if stripped.startswith(f"{key}="):
                result[index] = f"{key}={value}"
                seen_keys.add(key)

    insert_index = next_section_index
    for key, value in values.items():
        if key not in seen_keys:
            result.insert(insert_index, f"{key}={value}")
            insert_index += 1
    return result


def _configure_default_maps() -> bool:
    old_text = DEFAULT_ENGINE_INI.read_text(encoding="utf-8") if DEFAULT_ENGINE_INI.exists() else ""
    old_lines = old_text.splitlines()
    new_lines = _set_or_insert_section_values(old_lines, GAME_MAPS_SECTION, {
        "GameDefaultMap": MAIN_MAP,
        "EditorStartupMap": MAIN_MAP,
    })
    new_text = "\n".join(new_lines).rstrip() + "\n"
    if new_text == old_text:
        return False
    DEFAULT_ENGINE_INI.write_text(new_text, encoding="utf-8")
    return True


def main() -> None:
    report = {
        "created_directories": _ensure_directories(),
        "created_maps": _ensure_maps(),
        "config_changed": _configure_default_maps(),
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
