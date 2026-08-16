"""Print dirty package names to stdout for MCP orchestration."""

import json

import unreal


def main(argv):
    dirty_content = sorted({str(p.get_name()) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()})
    dirty_maps = sorted({str(p.get_name()) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()})
    print(json.dumps({"dirty_content": dirty_content, "dirty_maps": dirty_maps}, ensure_ascii=False))


if __name__ == "__main__":
    main([])
