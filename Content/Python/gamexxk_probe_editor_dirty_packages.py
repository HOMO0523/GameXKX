"""Read-only editor package dirtiness probe for safe MCP orchestration."""

from __future__ import annotations

import json

import unreal


def _package_names(method_name: str) -> list[str]:
    getter = getattr(unreal.EditorLoadingAndSavingUtils, method_name)
    return sorted({str(package.get_name()) for package in getter()})


def main() -> dict[str, object]:
    result = {
        "dirty_content_packages": _package_names("get_dirty_content_packages"),
        "dirty_map_packages": _package_names("get_dirty_map_packages"),
    }
    unreal.log("[GameXXKDirtyPackageProbe] " + json.dumps(result, ensure_ascii=False))
    return result


RESULT = main()
