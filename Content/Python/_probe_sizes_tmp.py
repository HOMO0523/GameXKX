import json

import unreal

out = {}
for name in (
    "T_character_00_hero_idle_atlas",
    "T_character_00_hero_2k_idle_atlas",
    "T_character_00_hero_1k_idle_atlas",
    "T_enemy_01_rooster_idle_atlas",
    "T_enemy_01_rooster_2k_idle_atlas",
    "T_enemy_01_rooster_1k_idle_atlas",
):
    path = "/Game/GameXXK/BattleAnimations/Atlases/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        out[name] = "missing"
        continue
    texture = unreal.EditorAssetLibrary.load_asset(path)
    if texture is None:
        out[name] = "load_none"
    else:
        try:
            out[name] = [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())]
        except Exception as exc:
            out[name] = f"ERR:{exc}"
print(json.dumps(out, ensure_ascii=False))
