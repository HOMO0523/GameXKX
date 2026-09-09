import json
import time

import unreal

out = {}
registry = unreal.AssetRegistryHelpers.get_asset_registry()
out["rescan"] = bool(registry.search_all_assets(True))
for _ in range(120):
    if not registry.is_loading_assets():
        break
    time.sleep(0.5)
out["still_loading"] = bool(registry.is_loading_assets())
out["dir_exists"] = unreal.EditorAssetLibrary.does_directory_exist("/Game/GameXXK/BattleAnimations/Atlases")
out["atlas_asset"] = unreal.EditorAssetLibrary.does_asset_exist(
    "/Game/GameXXK/BattleAnimations/Atlases/T_character_00_hero_idle_atlas"
)
print(json.dumps(out, ensure_ascii=False))
