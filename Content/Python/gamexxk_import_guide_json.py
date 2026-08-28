from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parents[2]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from scripts.validate_guide_json import DEFAULT_CATALOGS, load_catalog, validate_file

try:
    import unreal
except ModuleNotFoundError:
    unreal = None


DESTINATION_ROOT = "/Game/GameXXK/Narrative/Guides"
REPORT_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "guide-import-report.json"


def _asset_name(guide_id: str) -> str:
    return "DA_" + "".join(character if character.isalnum() else "_" for character in guide_id)


def _build_step(step_id: str, payload: dict[str, Any]) -> Any:
    assert unreal is not None
    policy_map = {
        "soft": unreal.GameXXKGuideInputPolicy.SOFT,
        "forced": unreal.GameXXKGuideInputPolicy.FORCED,
    }
    step = unreal.GameXXKGuideStepDefinition()
    step.set_editor_property("step_id", unreal.Name(step_id))
    step.set_editor_property("trigger_event_id", unreal.Name(payload["triggerEvent"]))
    step.set_editor_property("target_id", unreal.Name(payload.get("target", "")))
    step.set_editor_property("input_policy", policy_map[payload["inputPolicy"]])
    step.set_editor_property("text", unreal.Text(payload["text"]))
    step.set_editor_property(
        "allowed_action_ids",
        {unreal.Name(action_id) for action_id in payload.get("allowedActions", [])},
    )
    step.set_editor_property("completion_event_id", unreal.Name(payload["completionEvent"]))
    step.set_editor_property("next_step_id", unreal.Name(payload.get("next", "")))
    return step


def _load_or_create(asset_name: str) -> Any:
    assert unreal is not None
    asset_path = f"{DESTINATION_ROOT}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if asset is None or not isinstance(asset, unreal.GameXXKGuideAsset):
            raise RuntimeError(f"guide destination has the wrong class: {asset_path}")
        return asset
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.GameXXKGuideAsset)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = asset_tools.create_asset(
        asset_name, DESTINATION_ROOT, unreal.GameXXKGuideAsset, factory
    )
    if asset is None:
        raise RuntimeError(f"failed to create guide asset: {asset_path}")
    return asset


def import_guides(
    paths: list[Path], catalog_path: Path | None = None
) -> dict[str, Any]:
    if unreal is None:
        raise RuntimeError("gamexxk_import_guide_json.py must run inside Unreal Editor")
    catalogs = load_catalog(catalog_path) if catalog_path else DEFAULT_CATALOGS

    validated = [(path, validate_file(path, catalogs)) for path in sorted(paths)]
    results = []
    for path, payload in validated:
        asset = _load_or_create(_asset_name(payload["guideId"]))
        asset.set_editor_property("guide_id", unreal.Name(payload["guideId"]))
        asset.set_editor_property("guide_version", int(payload["guideVersion"]))
        asset.set_editor_property("entry_step_id", unreal.Name(payload["entryStep"]))
        asset.set_editor_property(
            "steps",
            [_build_step(step_id, step) for step_id, step in payload["steps"].items()],
        )
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"failed to save guide asset: {asset.get_path_name()}")
        results.append(
            {
                "source": str(path.relative_to(PROJECT_ROOT)).replace("\\", "/"),
                "sourceSha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "guideId": payload["guideId"],
                "stepCount": len(payload["steps"]),
                "assetPath": asset.get_path_name(),
            }
        )

    report = {
        "ok": True,
        "destination": DESTINATION_ROOT,
        "importedCount": len(results),
        "assets": results,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(
        json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True),
        encoding="utf-8",
    )
    return report


def _collect_sources(paths: list[Path]) -> list[Path]:
    sources: list[Path] = []
    for path in paths:
        sources.extend(sorted(path.rglob("*.guide.json")) if path.is_dir() else [path])
    return sorted(set(sources))


def main() -> int:
    parser = argparse.ArgumentParser(description="Import validated GameXXK guide JSON.")
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--catalog", type=Path)
    args = parser.parse_args()
    report = import_guides(_collect_sources(args.paths), args.catalog)
    print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
