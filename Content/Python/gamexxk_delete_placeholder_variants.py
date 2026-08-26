import json

import unreal

out = {}
for name in (
    "T_character_00_hero_2k_idle_atlas",
    "T_character_00_hero_1k_idle_atlas",
    "T_enemy_01_rooster_2k_idle_atlas",
    "T_enemy_01_rooster_1k_idle_atlas",
):
    path = "/Game/GameXXK/BattleAnimations/Atlases/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        out[name] = "absent"
        continue
    out[name] = bool(unreal.EditorAssetLibrary.delete_asset(path))
print(json.dumps(out, ensure_ascii=False))
