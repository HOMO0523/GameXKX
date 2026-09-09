import json

import unreal

out = {}
folder = "/Game/GameXXK/BattleAnimations/Atlases"
assets = unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(folder, recursive=False)
two_k = []
four_k = []
one_k = []
other = []
for asset in assets:
    name = str(asset.asset_name)
    if "_2k_" in name:
        two_k.append(name)
    elif "_1k_" in name:
        one_k.append(name)
    elif "_4k_" in name:
        four_k.append(name)
    else:
        other.append(name)
out["counts"] = {"base_4k_masters": len(other), "_2k_siblings": len(two_k), "_1k_siblings": len(one_k)}
samples = [
    "/Game/GameXXK/BattleAnimations/Atlases/T_character_00_hero_2k_idle_atlas",
    "/Game/GameXXK/BattleAnimations/Atlases/T_enemy_21_tiger_boss_2k_attack_atlas",
    "/Game/GameXXK/BattleAnimations/Atlases/T_character_00_hero_idle_atlas",
]
out["samples"] = {}
for path in samples:
    texture = unreal.EditorAssetLibrary.load_asset(path)
    if texture is None:
        out["samples"][path.rsplit("/", 1)[-1]] = "missing"
        continue
    try:
        out["samples"][path.rsplit("/", 1)[-1]] = [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())]
    except Exception as exc:
        out["samples"][path.rsplit("/", 1)[-1]] = f"ERR:{exc}"
print(json.dumps(out, ensure_ascii=False))
