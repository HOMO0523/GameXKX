from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parents[2]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from scripts.validate_narrative_sequence_json import validate_character_catalog

try:
    import unreal
except ModuleNotFoundError:
    unreal = None


SOURCE_PATH = PROJECT_ROOT / "SourceAssets" / "Narrative" / "characters.json"
ASSET_PATH = "/Game/GameXXK/Narrative/Characters/DA_CharacterCatalog"
REPORT_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "character-catalog-import-report.json"


def _load_source() -> dict[str, Any]:
    payload = json.loads(SOURCE_PATH.read_text(encoding="utf-8"))
    errors = validate_character_catalog(payload)
    if errors:
        raise ValueError("\n".join(errors))
    return payload


def _load_or_create_asset() -> Any:
    assert unreal is not None
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        asset = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
        if asset is None or not isinstance(asset, unreal.GameXXKCharacterCatalog):
            raise RuntimeError(f"character catalog destination has the wrong class: {ASSET_PATH}")
        return asset
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.GameXXKCharacterCatalog)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = asset_tools.create_asset(
        "DA_CharacterCatalog",
        "/Game/GameXXK/Narrative/Characters",
        unreal.GameXXKCharacterCatalog,
        factory,
    )
    if asset is None:
        raise RuntimeError("failed to create CharacterCatalog")
    return asset


def _definition(entry: dict[str, Any]) -> Any:
    assert unreal is not None
    definition = unreal.GameXXKCharacterDefinition()
    definition.set_editor_property("character_id", unreal.Name(entry["characterId"]))
    definition.set_editor_property("display_name", unreal.Text(entry["displayName"]))
    if entry.get("portraitPath"):
        definition.set_editor_property("portrait_path", unreal.SoftObjectPath(entry["portraitPath"]))
    if entry.get("animationLibraryPath"):
        definition.set_editor_property(
            "animation_library_path", unreal.SoftObjectPath(entry["animationLibraryPath"])
        )
    definition.set_editor_property(
        "supported_action_ids", [unreal.Name(action) for action in entry.get("actions", [])]
    )
    definition.set_editor_property(
        "default_interaction_sequence_id",
        unreal.Name(entry.get("defaultInteractionSequenceId", "")),
    )
    return definition


def import_character_catalog() -> dict[str, Any]:
    if unreal is None:
        raise RuntimeError("gamexxk_import_character_catalog.py must run inside Unreal Editor")
    payload = _load_source()
    asset = _load_or_create_asset()
    asset.set_editor_property(
        "characters", [_definition(entry) for entry in payload["characters"]]
    )
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"failed to save {ASSET_PATH}")
    report = {
        "ok": True,
        "source": str(SOURCE_PATH.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "sourceSha256": hashlib.sha256(SOURCE_PATH.read_bytes()).hexdigest(),
        "assetPath": asset.get_path_name(),
        "characterCount": len(payload["characters"]),
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


if __name__ == "__main__":
    result = import_character_catalog()
    print(json.dumps(result, ensure_ascii=False, indent=2))
