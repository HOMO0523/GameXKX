import json

import unreal

out = {"counts": {}, "bad": []}
for name in (
    "T_character_00_hero_idle_atlas",
    "T_character_04_hunter_idle_atlas",
    "T_character_04_hunter_attack_atlas",
    "T_character_04_hunter_hit_atlas",
    "T_enemy_21_tiger_boss_idle_atlas",
):
    path = "/Game/GameXXK/BattleAnimations/Atlases/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        out["bad"].append([name, "missing"])
        continue
    texture = unreal.EditorAssetLibrary.load_asset(path)
    if texture is None:
        out["bad"].append([name, "load_none"])
        continue
    try:
        size = [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())]
    except Exception:
        size = "?"
    out["counts"][name] = size
print(json.dumps(out, ensure_ascii=False))
