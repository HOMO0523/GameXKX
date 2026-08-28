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

from scripts.validate_narrative_sequence_json import NarrativeCatalogSnapshot, validate_file

try:
    import unreal
except ModuleNotFoundError:
    unreal = None


DESTINATION_ROOT = "/Game/GameXXK/Narrative/Sequences"
REPORT_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "narrative-sequence-import-report.json"


def _load_catalog(path: Path) -> NarrativeCatalogSnapshot:
    payload = json.loads(path.read_text(encoding="utf-8"))
    return NarrativeCatalogSnapshot(
        character_ids=frozenset(payload.get("characterIds", [])),
        action_ids_by_character={
            key: frozenset(value) for key, value in payload.get("actionIdsByCharacter", {}).items()
        },
        dialogue_ids=frozenset(payload.get("dialogueIds", [])),
        slot_ids_by_stage={
            key: frozenset(value) for key, value in payload.get("slotIdsByStage", {}).items()
        },
        command_types=frozenset(payload.get("commandTypes", [])),
        wait_types=frozenset(payload.get("waitTypes", [])),
        outcome_ids=frozenset(payload.get("outcomeIds", [])),
    )


def _asset_name(sequence_id: str) -> str:
    return "DA_" + "".join(character if character.isalnum() else "_" for character in sequence_id)


def _name_map(values: dict[str, str] | None) -> dict[Any, Any]:
    assert unreal is not None
    return {unreal.Name(key): unreal.Name(value) for key, value in (values or {}).items()}


def _string_map(values: dict[str, str] | None) -> dict[Any, str]:
    assert unreal is not None
    return {unreal.Name(key): value for key, value in (values or {}).items()}


def _step(step_id: str, payload: dict[str, Any]) -> Any:
    assert unreal is not None
    type_map = {
        "command": unreal.GameXXKNarrativeStepType.COMMAND,
        "wait": unreal.GameXXKNarrativeStepType.WAIT,
        "dialogue": unreal.GameXXKNarrativeStepType.DIALOGUE,
        "branchOnOutcome": unreal.GameXXKNarrativeStepType.BRANCH_ON_OUTCOME,
        "end": unreal.GameXXKNarrativeStepType.END,
    }
    definition = unreal.GameXXKNarrativeSequenceStepDefinition()
    definition.set_editor_property("step_id", unreal.Name(step_id))
    definition.set_editor_property("type", type_map[payload["type"]])
    if payload["type"] == "command":
        command = unreal.GameXXKNarrativeCommandDefinition()
        command.set_editor_property("command_id", unreal.Name(payload["commandId"]))
        command.set_editor_property("command_type", unreal.Name(payload["commandType"]))
        command.set_editor_property("arguments", _string_map(payload.get("arguments")))
        command.set_editor_property("optional", bool(payload.get("optional", False)))
        definition.set_editor_property("command", command)
    definition.set_editor_property("wait_type", unreal.Name(payload.get("waitType", "")))
    definition.set_editor_property("wait_arguments", _string_map(payload.get("arguments")))
    definition.set_editor_property("dialogue_id", unreal.Name(payload.get("dialogueId", "")))
    definition.set_editor_property("outcome_to_step_id", _name_map(payload.get("outcomes")))
    definition.set_editor_property("next_step_id", unreal.Name(payload.get("next", "")))
    return definition


def _load_or_create(asset_name: str) -> Any:
    assert unreal is not None
    asset_path = f"{DESTINATION_ROOT}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if asset is None or not isinstance(asset, unreal.GameXXKNarrativeSequenceAsset):
            raise RuntimeError(f"sequence destination has the wrong class: {asset_path}")
        return asset
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.GameXXKNarrativeSequenceAsset)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = asset_tools.create_asset(
        asset_name, DESTINATION_ROOT, unreal.GameXXKNarrativeSequenceAsset, factory
    )
    if asset is None:
        raise RuntimeError(f"failed to create sequence asset: {asset_path}")
    return asset


def import_sequences(paths: list[Path], catalog_path: Path) -> dict[str, Any]:
    if unreal is None:
        raise RuntimeError("gamexxk_import_narrative_sequence_json.py must run inside Unreal Editor")
    catalogs = _load_catalog(catalog_path)
    validated = [(path, validate_file(path, catalogs)) for path in sorted(paths)]
    results = []
    for path, payload in validated:
        asset = _load_or_create(_asset_name(payload["sequenceId"]))
        asset.set_editor_property("sequence_id", unreal.Name(payload["sequenceId"]))
        asset.set_editor_property("sequence_version", int(payload["sequenceVersion"]))
        asset.set_editor_property("stage_contract_id", unreal.Name(payload["stageContractId"]))
        asset.set_editor_property("entry_step_id", unreal.Name(payload["entryStep"]))
        asset.set_editor_property("default_character_id_by_role", _name_map(payload.get("roles")))
        asset.set_editor_property(
            "steps", [_step(step_id, step) for step_id, step in payload["steps"].items()]
        )
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"failed to save sequence asset: {asset.get_path_name()}")
        results.append(
            {
                "source": str(path.relative_to(PROJECT_ROOT)).replace("\\", "/"),
                "sourceSha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "sequenceId": payload["sequenceId"],
                "stepCount": len(payload["steps"]),
                "assetPath": asset.get_path_name(),
            }
        )
    report = {"ok": True, "importedCount": len(results), "assets": results}
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description="Import validated GameXXK narrative sequences.")
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--catalog", required=True, type=Path)
    args = parser.parse_args()
    paths = []
    for path in args.paths:
        paths.extend(sorted(path.rglob("*.sequence.json")) if path.is_dir() else [path])
    print(json.dumps(import_sequences(paths, args.catalog), ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
