import json

import unreal

out = {}
for path in (
    "/Game/GameXXK/BattleAnimations",
    "/Game/GameXXK/BattleAnimations/Atlases",
    "/Game/GameXXK/BattleAnimations/IdleSprites",
    "/Game/GameXXK/BattleAnimations/IdleFlipbooks",
):
    out[path] = {
        "dir_exists": unreal.EditorAssetLibrary.does_directory_exist(path),
        "made": unreal.EditorAssetLibrary.make_directory(path) if not unreal.EditorAssetLibrary.does_directory_exist(path) else "skipped",
    }
out["atlas_asset"] = unreal.EditorAssetLibrary.does_asset_exist(
    "/Game/GameXXK/BattleAnimations/Atlases/T_character_00_hero_idle_atlas"
)
print(json.dumps(out, ensure_ascii=False))
