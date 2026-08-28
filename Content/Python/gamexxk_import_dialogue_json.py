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

from scripts.validate_dialogue_json import CatalogSnapshot, validate_file

try:
    import unreal
except ModuleNotFoundError:
    unreal = None


DESTINATION_ROOT = "/Game/GameXXK/Narrative/Dialogues"
REPORT_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "dialogue-import-report.json"


def _load_catalog(path: Path) -> CatalogSnapshot:
    payload = json.loads(path.read_text(encoding="utf-8"))
    return CatalogSnapshot(
        speakers=frozenset(payload.get("speakers", [])),
        roles=frozenset(payload.get("roles", [])),
        outcomes=frozenset(payload.get("outcomes", [])),
    )


def _asset_name(dialogue_id: str) -> str:
    return "DA_" + "".join(character if character.isalnum() else "_" for character in dialogue_id)


def _name_map(values: dict[str, str] | None) -> dict[Any, str]:
    assert unreal is not None
    return {unreal.Name(key): value for key, value in (values or {}).items()}


def _text(value: object) -> Any:
    assert unreal is not None
    return unreal.Text(str(value or ""))


def _build_option(payload: dict[str, Any]) -> Any:
    assert unreal is not None
    option = unreal.GameXXKDialogueOptionDefinition()
    option.set_editor_property("option_id", unreal.Name(payload["optionId"]))
    option.set_editor_property("text_id", unreal.Name(payload["textId"]))
    option.set_editor_property("text", _text(payload["text"]))
    option.set_editor_property("outcome_id", unreal.Name(payload["outcomeId"]))
    option.set_editor_property("next_node_id", unreal.Name(payload["next"]))
    option.set_editor_property("conditions", _name_map(payload.get("conditions")))
    option.set_editor_property("disabled_reason", _text(payload.get("disabledReason", "")))
    return option


def _build_node(node_id: str, payload: dict[str, Any]) -> Any:
    assert unreal is not None
    node_type_map = {
        "line": unreal.GameXXKDialogueNodeType.LINE,
        "choice": unreal.GameXXKDialogueNodeType.CHOICE,
        "end": unreal.GameXXKDialogueNodeType.END,
    }
    presentation_map = {
        "bubble": unreal.GameXXKDialoguePresentation.BUBBLE,
        "dialogue": unreal.GameXXKDialoguePresentation.DIALOGUE_PANEL,
    }
    node = unreal.GameXXKDialogueNodeDefinition()
    node.set_editor_property("node_id", unreal.Name(node_id))
    node.set_editor_property("type", node_type_map[payload["type"]])
    node.set_editor_property(
        "presentation",
        presentation_map.get(payload.get("presentation"), unreal.GameXXKDialoguePresentation.NONE),
    )
    node.set_editor_property("speaker_id", unreal.Name(payload.get("speaker", "")))
    node.set_editor_property("text_id", unreal.Name(payload.get("textId", "")))
    node.set_editor_property("text", _text(payload.get("text", "")))
    node.set_editor_property("end_outcome_id", unreal.Name(payload.get("outcomeId", "")))
    node.set_editor_property("next_node_id", unreal.Name(payload.get("next", "")))
    node.set_editor_property(
        "options",
        [_build_option(option) for option in payload.get("options", [])],
    )
    node.set_editor_property("conditions", _name_map(payload.get("conditions")))
    return node


def _load_or_create_asset(asset_name: str) -> Any:
    assert unreal is not None
    asset_path = f"{DESTINATION_ROOT}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        existing = unreal.EditorAssetLibrary.load_asset(asset_path)
        if existing is None:
            raise RuntimeError(f"destination exists but cannot be loaded: {asset_path}")
        if not isinstance(existing, unreal.GameXXKDialogueAsset):
            raise RuntimeError(f"destination exists with wrong class: {asset_path}")
        return existing

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.GameXXKDialogueAsset)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = asset_tools.create_asset(
        asset_name,
        DESTINATION_ROOT,
        unreal.GameXXKDialogueAsset,
        factory,
    )
    if created is None:
        raise RuntimeError(f"failed to create dialogue asset: {asset_path}")
    return created


def _apply_payload(asset: Any, payload: dict[str, Any]) -> None:
    assert unreal is not None
    asset.set_editor_property("dialogue_id", unreal.Name(payload["dialogueId"]))
    asset.set_editor_property("dialogue_version", int(payload["dialogueVersion"]))
    asset.set_editor_property("entry_node_id", unreal.Name(payload["entryNode"]))
    asset.set_editor_property(
        "nodes",
        [_build_node(node_id, node) for node_id, node in payload["nodes"].items()],
    )


def import_dialogues(source_paths: list[Path], catalog_path: Path) -> dict[str, Any]:
    if unreal is None:
        raise RuntimeError("gamexxk_import_dialogue_json.py must run inside Unreal Editor")
    catalogs = _load_catalog(catalog_path)

    validated: list[tuple[Path, dict[str, Any]]] = []
    for source_path in sorted(source_paths):
        validated.append((source_path, validate_file(source_path, catalogs)))

    results: list[dict[str, Any]] = []
    for source_path, payload in validated:
        asset_name = _asset_name(payload["dialogueId"])
        asset = _load_or_create_asset(asset_name)
        _apply_payload(asset, payload)
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"failed to save dialogue asset: {asset.get_path_name()}")
        results.append(
            {
                "source": str(source_path.relative_to(PROJECT_ROOT)).replace("\\", "/"),
                "sourceSha256": hashlib.sha256(source_path.read_bytes()).hexdigest(),
                "dialogueId": payload["dialogueId"],
                "dialogueVersion": payload["dialogueVersion"],
                "nodeCount": len(payload["nodes"]),
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
        if path.is_dir():
            sources.extend(sorted(path.rglob("*.dialogue.json")))
        else:
            sources.append(path)
    return sorted(set(sources))


def main() -> int:
    parser = argparse.ArgumentParser(description="Import validated GameXXK dialogue JSON.")
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--catalog", required=True, type=Path)
    args = parser.parse_args()
    report = import_dialogues(_collect_sources(args.paths), args.catalog)
    print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
