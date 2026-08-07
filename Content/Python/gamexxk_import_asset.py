"""Transient helper: import one PNG into the approved MasterV2 folder with an
explicit destination asset name."""

import json
import sys
import unreal


def main():
    argv = sys.argv[1:]
    if len(argv) < 2:
        print(json.dumps({"ok": False, "reason": "usage: gamexxk_import_asset.py <png> <asset_name> [destination_path]"}))
        return
    source = argv[0]
    asset_name = argv[1]
    destination = argv[2] if len(argv) > 2 else "/Game/GameXXK/UI/MasterV2/Approved"
    task = unreal.AssetImportTask()
    task.filename = source
    task.destination_path = destination
    task.destination_name = asset_name
    task.replace_existing = True
    task.automated = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    obj = unreal.load_asset(destination + "/" + asset_name)
    result = {"ok": obj is not None, "asset": asset_name}
    if obj:
        try:
            result["size"] = [obj.blueprint_get_size_x(), obj.blueprint_get_size_y()]
        except Exception:
            pass
    print(json.dumps(result, ensure_ascii=True))


if __name__ == "__main__":
    main()
