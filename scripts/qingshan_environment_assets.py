"""Validate and update the Qingshan environment concept asset catalog."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path
from typing import Any


class CatalogError(ValueError):
    """Raised when the Qingshan environment catalog violates its contract."""


EXPECTED_VIEWS = {
    "reference": ("board",),
    "registry": ("existing_asset_registry",),
    "building": ("hero_3q", "structure_sheet"),
    "landmark": ("route_3q", "structure_connection_sheet"),
    "near_mountain": ("module_lineup", "assembly_player_scale"),
    "far_mountain": ("silhouette_layers", "player_camera_composite"),
    "rock_kit": ("variant_lineup", "silhouette_scale"),
    "paper2d_plant": ("neutral_variants", "silhouette_scale_cluster", "wind_poses"),
    "surface": ("seamless_transition_sheet", "player_camera_material_sheet"),
    "linear_kit": ("variant_lineup", "usage_scale_sheet"),
    "prop_kit": ("variant_lineup", "usage_scale_sheet"),
}

BATCH_COUNTS = {"REGISTRY": 1, "B0": 4, "B1": 12, "B2": 13, "B3": 5}
BATCH_CALL_CAPS = {"REGISTRY": 0, "B0": 12, "B1": 48, "B2": 52, "B3": 20}

ASSET_ID_PATTERN = re.compile(r"^(REF|BLD|LMK|ENV|P2D|SRF|KIT|PROP)_QS_[A-Z0-9_]+$")
ALLOWED_VERSIONS = ("v001", "v002", "v003")
REQUIRED_FIELDS = (
    "schema_version",
    "asset_id",
    "display_name",
    "category",
    "batch",
    "priority",
    "status",
    "style_profile",
    "intended_zone",
    "gameplay_role",
    "target_dimensions_m",
    "pivot",
    "forward_axis",
    "palette",
    "silhouette_keywords",
    "material_language",
    "negative_prompt",
    "dependencies",
    "source_provenance",
    "reference_images",
    "generation",
    "unreal",
    "pcg",
    "workflow_gates",
    "acceptance",
)
GENERATION_FIELDS = (
    "use_case",
    "asset_type",
    "required_view_kinds",
    "max_versions_per_view",
    "max_generation_calls",
    "generation_calls_used",
    "blocked_after_revisions",
)
WORKFLOW_GATE_FIELDS = (
    "style_locked",
    "reference_approved",
    "concept_generation_allowed",
    "batch_unlocked",
    "previous_batch_approved",
    "batch_approval_id",
    "model_or_sprite_production_allowed",
    "tripo_allowed",
    "ue_import_allowed",
)
PRODUCTION_GATES = (
    "tripo_allowed",
    "model_or_sprite_production_allowed",
    "ue_import_allowed",
)


def _require_mapping(value: Any, field: str) -> dict:
    if not isinstance(value, dict):
        raise CatalogError(f"{field} must be an object")
    return value


def _missing_fields(data: dict, fields: tuple[str, ...], prefix: str = "") -> None:
    missing = [field for field in fields if field not in data]
    if missing:
        label = f"{prefix} " if prefix else ""
        raise CatalogError(f"missing {label}fields: {missing}")


def _reject_empty(value: Any, path: str) -> None:
    if isinstance(value, str) and not value.strip():
        raise CatalogError(f"empty field: {path}")
    if isinstance(value, (list, dict)) and not value:
        raise CatalogError(f"empty field: {path}")
    if isinstance(value, dict):
        for key, child in value.items():
            _reject_empty(child, f"{path}.{key}")
    elif isinstance(value, list):
        for index, child in enumerate(value):
            _reject_empty(child, f"{path}[{index}]")


def _require_nonnegative_integer(value: Any, field: str) -> int:
    if type(value) is not int or value < 0:
        raise CatalogError(f"{field} must be a nonnegative integer")
    return value


def validate_asset(data: dict) -> None:
    """Validate one asset record against the authoritative host contract."""

    if not isinstance(data, dict):
        raise CatalogError("asset must be an object")
    _missing_fields(data, REQUIRED_FIELDS)
    _reject_empty(data, "asset")

    if data["schema_version"] != 1:
        raise CatalogError("schema_version must be 1")
    asset_id = data["asset_id"]
    if not isinstance(asset_id, str) or ASSET_ID_PATTERN.fullmatch(asset_id) is None:
        raise CatalogError("asset_id does not match the Qingshan naming contract")

    category = data["category"]
    if category not in EXPECTED_VIEWS:
        raise CatalogError(f"unknown category: {category!r}")
    batch = data["batch"]
    if batch not in BATCH_COUNTS:
        raise CatalogError(f"unknown batch: {batch!r}")

    generation = _require_mapping(data["generation"], "generation")
    _missing_fields(generation, GENERATION_FIELDS, "generation")
    expected = EXPECTED_VIEWS[category]
    actual_views = generation["required_view_kinds"]
    if not isinstance(actual_views, list) or tuple(actual_views) != expected:
        raise CatalogError(f"required_view_kinds must be {expected}, got {actual_views!r}")

    reference_images = data["reference_images"]
    if not isinstance(reference_images, list) or not all(isinstance(item, dict) for item in reference_images):
        raise CatalogError("reference_images must be a list of objects")
    reference_kinds = tuple(item.get("kind") for item in reference_images)
    if reference_kinds != expected:
        raise CatalogError(f"reference_images kinds must be {expected}, got {reference_kinds}")

    if generation["max_versions_per_view"] != 3:
        raise CatalogError("max_versions_per_view must be 3")
    maximum_calls = _require_nonnegative_integer(
        generation["max_generation_calls"], "max_generation_calls"
    )
    calls_used = _require_nonnegative_integer(
        generation["generation_calls_used"], "generation_calls_used"
    )
    if calls_used > maximum_calls:
        raise CatalogError("generation call budget exceeded")

    gates = _require_mapping(data["workflow_gates"], "workflow_gates")
    _missing_fields(gates, WORKFLOW_GATE_FIELDS, "workflow_gates")
    if any(gates[gate] is not False for gate in PRODUCTION_GATES):
        raise CatalogError("production gates must remain false during concept production")


def _load_json(path: Path, label: str) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise CatalogError(f"missing {label}: {path}") from exc
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise CatalogError(f"could not read {label} {path}: {exc}") from exc
    if not isinstance(data, dict):
        raise CatalogError(f"{label} must contain a JSON object: {path}")
    return data


def _write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(data, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    path.write_text(text, encoding="utf-8", newline="\n")


def _manifest_ids(root: Path) -> list[str]:
    manifest = _load_json(root / "manifest.json", "manifest")
    ids = manifest.get("asset_ids")
    if not isinstance(ids, list) or not all(isinstance(asset_id, str) for asset_id in ids):
        raise CatalogError("manifest asset_ids must be a list of strings")
    return ids


def _asset_path(root: Path, asset_id: str) -> Path:
    return root / "assets" / asset_id / "asset.json"


def validate_catalog(root: Path) -> dict:
    """Validate all 35 catalog records and return a stable summary."""

    root = Path(root)
    ids = _manifest_ids(root)
    if len(ids) != 35 or len(set(ids)) != 35:
        raise CatalogError("catalog must contain 35 unique asset ids")

    assets: list[dict] = []
    for asset_id in ids:
        data = _load_json(_asset_path(root, asset_id), f"asset {asset_id}")
        validate_asset(data)
        if data["asset_id"] != asset_id:
            raise CatalogError(
                f"asset_id mismatch for {asset_id}: JSON contains {data['asset_id']!r}"
            )
        assets.append(data)

    counts = {
        batch: sum(item["batch"] == batch for item in assets)
        for batch in BATCH_COUNTS
    }
    if counts != BATCH_COUNTS:
        raise CatalogError(f"batch counts mismatch: {counts}")

    plant_cells = sum(
        item.get("paper2d", {}).get("atlas_cell_count", 0)
        for item in assets
        if item["category"] == "paper2d_plant"
    )
    if plant_cells != 40:
        raise CatalogError(f"Paper2D plant atlas must total 40 cells, got {plant_cells}")

    for batch, cap in BATCH_CALL_CAPS.items():
        calls_used = sum(
            item["generation"]["generation_calls_used"]
            for item in assets
            if item["batch"] == batch
        )
        if calls_used > cap:
            raise CatalogError(f"{batch} call cap exceeded: {calls_used}/{cap}")

    return {
        "ok": True,
        "asset_count": len(assets),
        "batch_counts": counts,
        "plant_cells": plant_cells,
    }


def _all_asset_records(root: Path) -> list[dict]:
    records: list[dict] = []
    assets_root = root / "assets"
    if not assets_root.exists():
        return records
    for path in sorted(assets_root.glob("*/asset.json")):
        records.append(_load_json(path, "asset"))
    return records


def _stored_output_path(root: Path, file: Path) -> str:
    resolved_file = file.resolve()
    try:
        return resolved_file.relative_to(root.resolve()).as_posix()
    except ValueError:
        return resolved_file.as_posix()


def register_output(
    root: Path,
    asset_id: str,
    view_kind: str,
    version: str,
    file: Path,
) -> dict:
    """Register a generated image without permitting overwrite or budget drift."""

    root = Path(root)
    file = Path(file)
    if ASSET_ID_PATTERN.fullmatch(asset_id) is None:
        raise CatalogError("asset_id does not match the Qingshan naming contract")
    if version not in ALLOWED_VERSIONS:
        raise CatalogError("version must be one of v001, v002, or v003; v004 is forbidden")
    if not file.is_file():
        raise CatalogError(f"output file does not exist: {file}")

    path = _asset_path(root, asset_id)
    data = _load_json(path, f"asset {asset_id}")
    validate_asset(data)
    if data["asset_id"] != asset_id:
        raise CatalogError(f"asset_id mismatch: requested {asset_id}, JSON contains {data['asset_id']}")

    required_views = tuple(data["generation"]["required_view_kinds"])
    if view_kind not in required_views:
        raise CatalogError(f"{view_kind!r} is not a required view for {asset_id}")
    matching = [item for item in data["reference_images"] if item.get("kind") == view_kind]
    if len(matching) != 1:
        raise CatalogError(f"expected exactly one reference_images entry for {view_kind}")

    generation = data["generation"]
    used = generation["generation_calls_used"]
    maximum = generation["max_generation_calls"]
    if used >= maximum:
        raise CatalogError(f"asset generation budget exhausted for {asset_id}: {used}/{maximum}")

    batch = data["batch"]
    batch_calls = sum(
        item.get("generation", {}).get("generation_calls_used", 0)
        for item in _all_asset_records(root)
        if item.get("batch") == batch
    )
    batch_cap = BATCH_CALL_CAPS[batch]
    if batch_calls >= batch_cap:
        raise CatalogError(f"{batch} generation budget exhausted: {batch_calls}/{batch_cap}")

    reference = matching[0]
    previous_version = reference.get("version")
    if previous_version is not None:
        if previous_version not in ALLOWED_VERSIONS:
            raise CatalogError(f"stored version is invalid: {previous_version!r}")
        if ALLOWED_VERSIONS.index(version) <= ALLOWED_VERSIONS.index(previous_version):
            raise CatalogError(
                f"version must be newer than {previous_version}; registered outputs are never overwritten"
            )

    digest = hashlib.sha256(file.read_bytes()).hexdigest()
    output_path = _stored_output_path(root, file)
    reference.update(
        {
            "output_path": output_path,
            "sha256": digest,
            "approval_state": "generated_pending_review",
            "version": version,
        }
    )
    generation["generation_calls_used"] = used + 1
    _write_json(path, data)
    return {
        "asset_id": asset_id,
        "view_kind": view_kind,
        "version": version,
        "output_path": output_path,
        "sha256": digest,
        "approval_state": "generated_pending_review",
        "generation_calls_used": generation["generation_calls_used"],
        "max_generation_calls": maximum,
    }


def _report_assets(root: Path, batch: str) -> list[dict]:
    if batch not in BATCH_COUNTS:
        raise CatalogError(f"unknown batch: {batch!r}")
    ids = _manifest_ids(root)
    assets: list[dict] = []
    for asset_id in ids:
        data = _load_json(_asset_path(root, asset_id), f"asset {asset_id}")
        if data.get("batch") == batch:
            validate_asset(data)
            assets.append(data)
    if not assets:
        raise CatalogError(f"no assets found for batch {batch}")
    return assets


def _verified_reference_hash(root: Path, reference: dict) -> str:
    missing = [key for key in ("output_path", "sha256", "approval_state", "version") if key not in reference]
    if missing:
        raise CatalogError(f"reference image has not been registered; missing {missing}")
    output = Path(reference["output_path"])
    if not output.is_absolute():
        output = root / output
    if not output.is_file():
        raise CatalogError(f"registered output is missing: {output}")
    digest = hashlib.sha256(output.read_bytes()).hexdigest()
    if digest != reference["sha256"]:
        raise CatalogError(f"registered SHA-256 mismatch for {reference['output_path']}")
    return digest


def write_batch_report(root: Path, batch: str, output: Path) -> None:
    """Write a deterministic Markdown report from registered catalog evidence."""

    root = Path(root)
    output = Path(output)
    assets = _report_assets(root, batch)
    rows = []
    for asset in assets:
        generation = asset["generation"]
        gates = asset["workflow_gates"]
        gate_summary = "; ".join(f"{gate}={str(gates[gate]).lower()}" for gate in PRODUCTION_GATES)
        for reference in asset["reference_images"]:
            digest = _verified_reference_hash(root, reference)
            rows.append(
                "| {asset_id} | {kind} | {path} | `{sha}` | {approval} | calls used {used}/{maximum} | {gates} |".format(
                    asset_id=asset["asset_id"],
                    kind=reference["kind"],
                    path=reference["output_path"],
                    sha=digest,
                    approval=reference["approval_state"],
                    used=generation["generation_calls_used"],
                    maximum=generation["max_generation_calls"],
                    gates=gate_summary,
                )
            )

    lines = [
        f"# Qingshan Environment Concept {batch} Report",
        "",
        "Generated deterministically from registered asset JSON and verified output files.",
        "",
        "| Asset ID | View | Output path | SHA-256 | Approval state | Generation budget | Production gates |",
        "|---|---|---|---|---|---|---|",
        *rows,
        "",
    ]
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="utf-8", newline="\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    actions = parser.add_mutually_exclusive_group(required=True)
    actions.add_argument("--check", action="store_true")
    actions.add_argument(
        "--register-output",
        nargs=4,
        metavar=("ASSET_ID", "VIEW_KIND", "VERSION", "FILE"),
    )
    actions.add_argument(
        "--write-batch-report",
        nargs=2,
        metavar=("BATCH", "OUTPUT"),
    )
    args = parser.parse_args()
    if args.check:
        print(json.dumps(validate_catalog(args.root), ensure_ascii=False, sort_keys=True))
        return 0
    if args.register_output:
        asset_id, view_kind, version, file_name = args.register_output
        result = register_output(args.root, asset_id, view_kind, version, Path(file_name))
        print(json.dumps(result, ensure_ascii=False, sort_keys=True))
        return 0
    batch, output = args.write_batch_report
    write_batch_report(args.root, batch, Path(output))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
