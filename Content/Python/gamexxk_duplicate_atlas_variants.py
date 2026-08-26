import json

import unreal

out = {}
tools = unreal.AssetToolsHelpers.get_asset_tools()
for source_name, target_names in (
    (
        "/Game/GameXXK/BattleAnimations/Atlases/T_character_00_hero_idle_atlas",
        ("/Game/GameXXK/BattleAnimations/Atlases/T_character_00_hero_2k_idle_atlas",
         "/Game/GameXXK/BattleAnimations/Atlases/T_character_00_hero_1k_idle_atlas"),
    ),
    (
        "/Game/GameXXK/BattleAnimations/Atlases/T_enemy_01_rooster_idle_atlas",
        ("/Game/GameXXK/BattleAnimations/Atlases/T_enemy_01_rooster_2k_idle_atlas",
         "/Game/GameXXK/BattleAnimations/Atlases/T_enemy_01_rooster_1k_idle_atlas"),
    ),
):
    for target in target_names:
        target_name = target.rsplit("/", 1)[-1]
        if unreal.EditorAssetLibrary.does_asset_exist(target):
            out[target_name] = "already_exists"
            continue
        source = unreal.EditorAssetLibrary.load_asset(source_name)
        duplicated = tools.duplicate_asset(
            target_name,
            "/Game/GameXXK/BattleAnimations/Atlases",
            source,
        )
        if duplicated is None:
            out[target_name] = "duplicate_failed"
            continue
        unreal.EditorAssetLibrary.save_loaded_asset(duplicated)
        texture = unreal.EditorAssetLibrary.load_asset(target)
        try:
            size = [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())]
        except Exception:
            size = "?"
        out[target_name] = size
print(json.dumps(out, ensure_ascii=False))
