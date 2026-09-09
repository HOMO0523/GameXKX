"""Read-only package dirtiness probe for the desktop-training layout cold-build gate."""

from __future__ import annotations

import json

import unreal


def _package_name(package: object) -> str:
    getter = getattr(package, "get_name", None)
    return str(getter() if callable(getter) else package)


payload = {
    "content": sorted(
        _package_name(package)
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    ),
    "maps": sorted(
        _package_name(package)
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    ),
}
payload["ok"] = True
print(json.dumps(payload, ensure_ascii=False, sort_keys=True))
