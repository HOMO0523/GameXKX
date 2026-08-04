"""Independent validation for GameXXK UI Master Phase A candidates."""

from __future__ import annotations

import argparse
import json
from pathlib import Path, PurePosixPath
import sys
from typing import Any

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from scripts.gamexxk_ui_master_contract import load_contract, validate_source_lock


def _read_json(path: Path, label: str, errors: list[str]) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        errors.append(f"invalid {label}: {exc}")
        return {}
    if not isinstance(value, dict):
        errors.append(f"invalid {label}: root must be an object")
        return {}
    return value


def _safe_package_path(
    package_root: Path, relative_path: str, label: str, errors: list[str]
) -> Path | None:
    pure = PurePosixPath(str(relative_path).replace("\\", "/"))
    if pure.is_absolute() or ".." in pure.parts:
        errors.append(f"unsafe package path: {label}")
        return None
    resolved = (package_root / Path(*pure.parts)).resolve()
    try:
        resolved.relative_to(package_root.resolve())
    except ValueError:
        errors.append(f"unsafe package path: {label}")
        return None
    return resolved


def _check_image_size(path: Path, expected: tuple[int, int], label: str, errors: list[str]) -> None:
    try:
        with Image.open(path) as image:
            size = image.size
    except OSError as exc:
        errors.append(f"unreadable image: {label}: {exc}")
        return
    if size != expected:
        errors.append(f"image size mismatch: {label}")


def _relative_source_path(path_value: str, project_root: Path) -> str:
    path = Path(path_value)
    if path.is_absolute():
        try:
            return path.resolve().relative_to(project_root.resolve()).as_posix()
        except ValueError:
            return path.as_posix()
    return PurePosixPath(path_value.replace("\\", "/")).as_posix()


def validate_package(package_root: Path, project_root: Path) -> dict[str, Any]:
    package_root = package_root.resolve()
    project_root = project_root.resolve()
    errors: list[str] = []

    try:
        contract = load_contract(package_root / "ui-master-spec.json")
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        errors.append(f"invalid UI master contract: {exc}")
        return {"ok": False, "errors": errors}
    expected_groups = [page.name for page in contract.pages]

    source_report = validate_source_lock(package_root / "source-lock.json", project_root)
    errors.extend(source_report["errors"])
    source_lock = _read_json(package_root / "source-lock.json", "source lock", errors)
    manifest = _read_json(package_root / "master-manifest.json", "master manifest", errors)
    runtime = _read_json(
        package_root / "runtime-assets-manifest.json", "runtime manifest", errors
    )
    if not manifest:
        return {"ok": False, "errors": errors}

    document = manifest.get("document", {})
    if (document.get("width"), document.get("height")) != contract.master_size:
        errors.append("master canvas mismatch")
    pages = manifest.get("pages", [])
    manifest_groups = [page.get("group") for page in pages if isinstance(page, dict)]
    if manifest_groups != expected_groups:
        errors.append("master manifest page roster mismatch")

    previews_root = package_root / "Previews"
    preview_files = list(previews_root.glob("*.png")) if previews_root.is_dir() else []
    if len(preview_files) != 18:
        errors.append(f"preview count mismatch: expected 18, found {len(preview_files)}")
    for page in pages:
        if not isinstance(page, dict):
            continue
        preview_path = _safe_package_path(
            package_root, page.get("preview", ""), f"preview {page.get('group')}", errors
        )
        if preview_path is None or not preview_path.is_file():
            errors.append(f"missing preview: {page.get('group')}")
        else:
            _check_image_size(preview_path, (1920, 1080), f"preview {page.get('group')}", errors)
    contact_sheet = package_root / "GameXXK_UI_Master_ContactSheet.png"
    if not contact_sheet.is_file():
        errors.append("missing contact sheet")
    else:
        _check_image_size(contact_sheet, (2400, 1080), "contact sheet", errors)

    runtime_assets = runtime.get("assets", {}) if isinstance(runtime, dict) else {}
    if not isinstance(runtime_assets, dict) or not runtime_assets:
        errors.append("runtime assets are missing")
    else:
        for key, record in runtime_assets.items():
            if not isinstance(record, dict):
                errors.append(f"invalid runtime asset record: {key}")
                continue
            if record.get("textBaked") is not False:
                errors.append(f"runtime asset contains baked text: {key}")
            runtime_path = _safe_package_path(
                package_root, record.get("file", ""), f"runtime asset {key}", errors
            )
            if runtime_path is None or not runtime_path.is_file():
                errors.append(f"missing runtime asset: {key}")
                continue
            try:
                with Image.open(runtime_path) as image:
                    if "A" not in image.getbands():
                        errors.append(f"runtime asset missing alpha: {key}")
            except OSError:
                errors.append(f"unreadable runtime asset: {key}")

    image_layers = manifest.get("imageLayers", [])
    text_layers = manifest.get("textLayers", [])
    hero_path = str(source_lock.get("heroIdle", {}).get("path", ""))
    hero_absolute = (project_root / Path(*PurePosixPath(hero_path).parts)).resolve()
    hero_layers = []
    retired_roots = [
        str(value).replace("\\", "/").rstrip("/")
        for value in source_lock.get("retiredSourceRoots", [])
    ]
    for layer in image_layers:
        if not isinstance(layer, dict):
            continue
        source_value = str(layer.get("path", ""))
        relative_source = _relative_source_path(source_value, project_root)
        for retired_root in retired_roots:
            if relative_source == retired_root or relative_source.startswith(retired_root + "/"):
                errors.append(f"retired source path in manifest: {source_value}")
        try:
            layer_path = Path(source_value).resolve() if Path(source_value).is_absolute() else (package_root / source_value).resolve()
        except OSError:
            continue
        if layer_path == hero_absolute:
            hero_layers.append(layer)
    if not hero_layers:
        errors.append("missing Hero Idle placement")
    for layer in hero_layers:
        if layer.get("fitMode") != "contain_canvas":
            errors.append("Hero placement must use contain_canvas")
        scale_x = layer.get("scaleX")
        scale_y = layer.get("scaleY")
        if not isinstance(scale_x, (int, float)) or not isinstance(scale_y, (int, float)) or abs(scale_x - scale_y) > 1e-9:
            errors.append("Hero placement must use one uniform scale")

    output_value = document.get("outputPsd", "")
    output_path = Path(output_value)
    if not output_path.is_absolute():
        output_path = (project_root / output_path).resolve()
    if not output_path.is_file():
        errors.append("candidate PSD is missing")
        validation = {}
    else:
        validation = _read_json(
            output_path.with_suffix(".validation.json"), "Photoshop validation", errors
        )

    if validation:
        if (validation.get("width"), validation.get("height")) != contract.master_size:
            errors.append("Photoshop canvas mismatch")
        actual_groups = validation.get("actualTopLevelGroups", [])
        if not isinstance(actual_groups, list):
            actual_groups = []
            errors.append("invalid Photoshop top-level group list")
        for group in expected_groups:
            if group not in actual_groups:
                errors.append(f"missing top-level group: {group}")
        for group in actual_groups:
            if group not in expected_groups:
                errors.append(f"unexpected top-level group: {group}")
        if validation.get("expectedTopLevelGroups") != 18:
            errors.append("Photoshop expected top-level group count mismatch")
        if validation.get("expectedImageLayers") != len(image_layers):
            errors.append("Photoshop expected image layer count mismatch")
        if validation.get("expectedTextLayers") != len(text_layers):
            errors.append("Photoshop expected text layer count mismatch")
        if validation.get("actualTextLayers") != len(text_layers):
            errors.append("Photoshop actual text layer count mismatch")
        if validation.get("textRoundTripMatch") is not True:
            errors.append("Photoshop text round trip failed")

    return {
        "ok": not errors,
        "errors": errors,
        "pageGroups": len(expected_groups),
        "previews": len(preview_files),
        "imageLayers": len(image_layers),
        "textLayers": len(text_layers),
        "runtimeAssets": len(runtime_assets) if isinstance(runtime_assets, dict) else 0,
        "sourceLock": source_report,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-root", required=True, type=Path)
    parser.add_argument("--project-root", type=Path, default=PROJECT_ROOT)
    args = parser.parse_args()
    report = validate_package(args.package_root, args.project_root)
    print(json.dumps(report, ensure_ascii=False))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
