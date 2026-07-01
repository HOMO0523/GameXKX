from __future__ import annotations

import json
import shutil
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
GENERATED_DIR = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated"
DESTINATION_PATH = "/Game/GameXXK/Sprites/Generated"

SPRITE_SOURCES = [
    {
        "id": "hero_deepblue_high_ponytail_walk_8dir",
        "bootstrap_source": Path(r"C:\Users\shxuw\Documents\Tencent Files\978420603\nt_qq\nt_data\Pic\2026-07\Ori\9799dd77196a1b110c31300906c6f233.png"),
    },
    {
        "id": "follower_blue_scholar_walk_8dir",
        "bootstrap_source": Path(r"C:\Users\shxuw\Documents\Tencent Files\978420603\nt_qq\nt_data\Pic\2026-07\Ori\7b55047faaee29a3fc1ef33a37375ed0.png"),
    },
    {
        "id": "merchant_teal_robed_walk_8dir",
        "bootstrap_source": Path(r"C:\Users\shxuw\Documents\Tencent Files\978420603\nt_qq\nt_data\Pic\2026-07\Ori\17d068c1ece3b52cae953312e8b3232e.png"),
    },
]


def _stage_source_files() -> list[str]:
    GENERATED_DIR.mkdir(parents=True, exist_ok=True)
    copied = []
    for source in SPRITE_SOURCES:
        target_path = GENERATED_DIR / f"{source['id']}.png"
        if target_path.exists():
            continue
        source_path = Path(source["bootstrap_source"])
        if not source_path.exists():
            raise RuntimeError(
                f"Missing project-local sprite image {target_path} and bootstrap source {source_path}"
            )
        shutil.copy2(source_path, target_path)
        copied.append(str(target_path))
    return copied


def _import_textures() -> list[str]:
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION_PATH):
        unreal.EditorAssetLibrary.make_directory(DESTINATION_PATH)

    tasks = []
    for source in SPRITE_SOURCES:
        asset_path = f"{DESTINATION_PATH}/{source['id']}"
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            continue
        task = unreal.AssetImportTask()
        task.filename = str(GENERATED_DIR / f"{source['id']}.png")
        task.destination_path = DESTINATION_PATH
        task.destination_name = source["id"]
        task.automated = True
        task.save = True
        task.replace_existing = False
        tasks.append(task)

    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    return [f"{DESTINATION_PATH}/{source['id']}" for source in SPRITE_SOURCES]


def main() -> None:
    report = {
        "copied_files": _stage_source_files(),
        "expected_assets": _import_textures(),
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
