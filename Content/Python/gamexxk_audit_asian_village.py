"""Audit and optionally resave the local Stylized Eastern Village asset pack."""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path
from typing import Any

try:
    import unreal  # type: ignore
except ImportError:  # Unit tests import the pure helpers outside Unreal.
    unreal = None


ASSET_ROOT = "/Game/Asian_Village"
MODES = ("source-readonly", "target-upgrade", "target-verify")
GAME_MAPS = (
    "/Game/Asian_Village/maps/Asian_Village_Demo",
    "/Game/Asian_Village/maps/Overview_Map",
)
TARGET_EVIDENCE_ROOT = Path(
    r"D:\UE5 demo\GameXXK\docs\production\evidence\asian-village-migration"
)


class AuditError(RuntimeError):
    pass


def classify_dependency(package_name: str) -> dict[str, Any]:
    if package_name == ASSET_ROOT or package_name.startswith(ASSET_ROOT + "/"):
        return {"kind": "internal", "blocking": False}
    if package_name.startswith("/Engine/") or package_name.startswith("/Script/"):
        return {"kind": "engine", "blocking": False}
    if package_name.startswith("/Game/"):
        return {"kind": "external_game", "blocking": True}
    return {"kind": "plugin_or_unknown", "blocking": False}


def summarize_assets(records: list[dict[str, Any]]) -> dict[str, Any]:
    class_counts: dict[str, int] = {}
    for record in records:
        class_name = str(record["class"])
        class_counts[class_name] = class_counts.get(class_name, 0) + 1
    return {
        "asset_count": len(records),
        "class_counts": dict(sorted(class_counts.items())),
    }


def canonical_report_bytes(report: dict[str, Any]) -> bytes:
    return (
        json.dumps(report, ensure_ascii=False, sort_keys=True, indent=2) + "\n"
    ).encode("utf-8")


def validate_report(report: dict[str, Any], mode: str) -> None:
    required = {
        "schema_version",
        "engine_version",
        "project_dir",
        "mode",
        "asset_root",
        "asset_count",
        "class_counts",
        "packages",
        "external_dependencies",
        "load_failures",
        "blueprint_failures",
        "material_failures",
        "map_failures",
        "save_failures",
        "ok",
    }
    missing = sorted(required.difference(report))
    if missing:
        raise AuditError(f"audit report is missing fields: {missing}")
    if report.get("schema_version") != 1:
        raise AuditError("audit report schema_version must be 1")
    if mode not in MODES or report.get("mode") != mode:
        raise AuditError("audit report mode is invalid")
    if report.get("asset_root") != ASSET_ROOT:
        raise AuditError("audit report asset_root is invalid")
    if type(report.get("asset_count")) is not int:
        raise AuditError("audit report asset_count must be an integer")
    if type(report.get("ok")) is not bool:
        raise AuditError("audit report ok must be a boolean")
    for field in (
        "packages",
        "external_dependencies",
        "load_failures",
        "blueprint_failures",
        "material_failures",
        "map_failures",
        "save_failures",
    ):
        if not isinstance(report.get(field), list):
            raise AuditError(f"audit report {field} must be a list")


def _asset_class_name(asset_data: Any) -> str:
    class_path = getattr(asset_data, "asset_class_path", None)
    if class_path is not None:
        name = getattr(class_path, "asset_name", None)
        if name:
            return str(name)
    return str(getattr(asset_data, "asset_class", "Unknown"))


def _object_path(asset_data: Any) -> str:
    value = getattr(asset_data, "object_path", None)
    if value:
        return str(value)
    package = str(asset_data.package_name)
    name = str(getattr(asset_data, "asset_name", package.rsplit("/", 1)[-1]))
    return f"{package}.{name}"


def _dependency_options(unreal_module: Any) -> Any:
    return unreal_module.AssetRegistryDependencyOptions(
        include_soft_package_references=True,
        include_hard_package_references=True,
        include_searchable_names=False,
        include_soft_management_references=True,
        include_hard_management_references=True,
    )


def _compile_blueprint(unreal_module: Any, asset: Any) -> None:
    if hasattr(unreal_module, "BlueprintEditorLibrary"):
        unreal_module.BlueprintEditorLibrary.compile_blueprint(asset)
    else:
        unreal_module.KismetEditorUtilities.compile_blueprint(asset)


def collect_audit(unreal_module: Any, mode: str) -> dict[str, Any]:
    if mode not in MODES:
        raise AuditError(f"unsupported audit mode: {mode}")
    registry = unreal_module.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    assets = sorted(
        list(registry.get_assets_by_path(ASSET_ROOT, recursive=True)),
        key=lambda item: str(item.package_name),
    )
    options = _dependency_options(unreal_module)
    packages: list[dict[str, Any]] = []
    external: dict[str, dict[str, Any]] = {}
    load_failures: list[str] = []
    blueprint_failures: list[str] = []
    material_failures: list[str] = []

    for data in assets:
        package_name = str(data.package_name)
        object_path = _object_path(data)
        class_name = _asset_class_name(data)
        dependencies = sorted(
            str(item) for item in registry.get_dependencies(package_name, options)
        )
        for dependency in dependencies:
            classification = classify_dependency(dependency)
            if classification["kind"] != "internal":
                external.setdefault(
                    dependency,
                    {"package": dependency, **classification},
                )
        asset = unreal_module.EditorAssetLibrary.load_asset(object_path)
        loaded = asset is not None
        if not loaded:
            load_failures.append(object_path)
        elif mode == "target-upgrade" and class_name == "Blueprint":
            try:
                _compile_blueprint(unreal_module, asset)
            except Exception as exc:
                blueprint_failures.append(f"{object_path}: {exc}")
        elif mode == "target-upgrade" and class_name == "Material":
            try:
                unreal_module.MaterialEditingLibrary.recompile_material(asset)
            except Exception as exc:
                material_failures.append(f"{object_path}: {exc}")
        packages.append(
            {
                "package": package_name,
                "object_path": object_path,
                "class": class_name,
                "loaded": loaded,
                "dependencies": dependencies,
            }
        )

    map_failures: list[str] = []
    save_failures: list[str] = []
    if mode == "target-upgrade":
        for map_path in GAME_MAPS:
            try:
                world = unreal_module.EditorLoadingAndSavingUtils.load_map(map_path)
                if world is None:
                    raise RuntimeError("load_map returned None")
                unreal_module.SystemLibrary.execute_console_command(world, "MAP CHECK")
            except Exception as exc:
                map_failures.append(f"{map_path}: {exc}")
        try:
            saved = unreal_module.EditorAssetLibrary.save_directory(
                ASSET_ROOT,
                only_if_is_dirty=False,
                recursive=True,
            )
            if not saved:
                save_failures.append(f"{ASSET_ROOT}: save_directory returned false")
        except Exception as exc:
            save_failures.append(f"{ASSET_ROOT}: {exc}")

    summary = summarize_assets(packages)
    blocking = [item for item in external.values() if item["blocking"]]
    failures = (
        load_failures
        + blueprint_failures
        + material_failures
        + map_failures
        + save_failures
    )
    return {
        "schema_version": 1,
        "engine_version": str(unreal_module.SystemLibrary.get_engine_version()),
        "project_dir": str(unreal_module.Paths.project_dir()),
        "mode": mode,
        "asset_root": ASSET_ROOT,
        **summary,
        "packages": packages,
        "external_dependencies": sorted(external.values(), key=lambda item: item["package"]),
        "load_failures": load_failures,
        "blueprint_failures": blueprint_failures,
        "material_failures": material_failures,
        "map_failures": map_failures,
        "save_failures": save_failures,
        "ok": not blocking and not failures,
    }


def _allowed_output_roots(unreal_module: Any) -> tuple[Path, Path]:
    project_root = Path(str(unreal_module.Paths.project_dir())).resolve()
    return (
        (project_root / "Saved" / "AsianVillageAudit").resolve(),
        TARGET_EVIDENCE_ROOT.resolve(),
    )


def write_report(path: Path, report: dict[str, Any], unreal_module: Any) -> None:
    validate_report(report, str(report.get("mode")))
    candidate = path.resolve(strict=False)
    if not any(candidate.is_relative_to(root) for root in _allowed_output_roots(unreal_module)):
        raise AuditError(f"audit output escaped approved roots: {candidate}")
    candidate.parent.mkdir(parents=True, exist_ok=True)
    temporary = candidate.with_suffix(candidate.suffix + ".tmp")
    if candidate.exists() or temporary.exists():
        raise AuditError(f"audit output already exists: {candidate}")
    with temporary.open("xb") as handle:
        handle.write(canonical_report_bytes(report))
        handle.flush()
        os.fsync(handle.fileno())
    temporary.replace(candidate)


def main() -> None:
    if unreal is None:
        raise AuditError("this script must run inside Unreal Editor")
    mode = os.environ.get("GAMEXXK_AV_AUDIT_MODE", "")
    output_value = os.environ.get("GAMEXXK_AV_AUDIT_OUTPUT", "")
    if mode not in MODES:
        raise AuditError(f"GAMEXXK_AV_AUDIT_MODE must be one of {MODES}")
    if not output_value:
        raise AuditError("GAMEXXK_AV_AUDIT_OUTPUT is required")
    report = collect_audit(unreal, mode)
    write_report(Path(output_value), report, unreal)
    print("GAMEXXK_ASIAN_VILLAGE_AUDIT=" + canonical_report_bytes(report).decode("utf-8").strip())
    if not report["ok"]:
        raise AuditError("Asian Village audit reported blocking failures")


if __name__ == "__main__":
    main()
