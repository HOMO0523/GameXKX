import json

import unreal

out = {"bad": [], "checked": 0}
folder = "/Game/GameXXK/BattleAnimations/Atlases"
assets = unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(
    folder, recursive=False
)
for asset in assets:
    package_name = str(asset.package_name)
    if not package_name.startswith("/Game/GameXXK/BattleAnimations/Atlases/T_"):
        continue
    texture = unreal.EditorAssetLibrary.load_asset(package_name)
    if texture is None:
        out["bad"].append([package_name, "missing"])
        continue
    try:
        w = int(texture.blueprint_get_size_x())
        h = int(texture.blueprint_get_size_y())
    except Exception:
        w = h = 0
    out["checked"] += 1
    if w != 4096 or h != 4096:
        out["bad"].append([package_name, [w, h]])
print(json.dumps(out, ensure_ascii=False))
