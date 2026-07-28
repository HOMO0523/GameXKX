"""Build the UE texture-import plan from the authoritative town PSD package."""

from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path


EXPECTED_BACKGROUND_PAGES = frozenset({"hud", "character", "companion", "task", "map", "backpack"})


@dataclass(frozen=True)
class TownPsdImport:
    group: str
    page: str
    source: Path
    asset_name: str


def _read_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def build_import_plan(package_root: Path) -> tuple[TownPsdImport, ...]:
    """Return unique, project-local PSD texture imports in deterministic order."""
    semantic_map = _read_json(package_root / "semantic-map.json")
    plan: list[TownPsdImport] = []
    asset_names: set[str] = set()

    runtime_backgrounds = semantic_map.get("runtimeBackgrounds", [])
    if not isinstance(runtime_backgrounds, list):
        raise RuntimeError("runtimeBackgrounds must be a list")
    background_pages: set[str] = set()
    for entry in runtime_backgrounds:
        if not isinstance(entry, dict):
            raise RuntimeError("runtime background entry must be an object")
        page = str(entry.get("page", ""))
        source = package_root / str(entry.get("file", ""))
        asset_name = source.stem
        if page in background_pages:
            raise RuntimeError(f"duplicate runtime background page: {page}")
        background_pages.add(page)
        if not source.is_file():
            raise RuntimeError(f"missing runtime background source: {source}")
        plan.append(TownPsdImport("Backgrounds", page, source, asset_name))
        asset_names.add(asset_name)
    if background_pages != EXPECTED_BACKGROUND_PAGES:
        raise RuntimeError(f"runtime background pages must be {sorted(EXPECTED_BACKGROUND_PAGES)}")

    assets = semantic_map.get("assets", [])
    if not isinstance(assets, list):
        raise RuntimeError("assets must be a list")
    for entry in assets:
        if not isinstance(entry, dict):
            raise RuntimeError("PSD asset entry must be an object")
        group = str(entry.get("group", ""))
        source = package_root / str(entry.get("cleanAssetFile", ""))
        asset_name = str(entry.get("ueAssetName", ""))
        if not group or not asset_name or not asset_name.startswith("T_TownPsd_"):
            raise RuntimeError(f"invalid PSD import entry: {entry}")
        if "ActionBlank" in asset_name:
            raise RuntimeError("the authoritative PSD import plan may not use ActionBlank")
        if asset_name in asset_names:
            raise RuntimeError(f"duplicate PSD asset name: {asset_name}")
        if not source.is_file():
            raise RuntimeError(f"missing PSD source: {source}")
        plan.append(TownPsdImport(group, group.lower(), source, asset_name))
        asset_names.add(asset_name)

    return tuple(plan)
