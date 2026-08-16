"""Read-only probe: report whether the seven terrain backdrop textures exist."""

import json

import unreal


def main(argv):
    names = ["Plain", "Cliff", "Forest", "WaterShore", "Ferry", "Village", "Cave"]
    result = {}
    for name in names:
        path = "/Game/GameXXK/UI/Battle/Textures/T_BattleArena_{}_GeneratedV2".format(name)
        exists = unreal.EditorAssetLibrary.does_asset_exist(path)
        result[name] = {"path": path, "exists": exists}
        if exists:
            texture = unreal.EditorAssetLibrary.load_asset(path)
            result[name]["size"] = [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())]
            result[name]["lod_group"] = str(texture.get_editor_property("lod_group"))
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main([])
