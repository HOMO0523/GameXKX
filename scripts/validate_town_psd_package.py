"""Validate the project-local authoritative town PSD package."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def load_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def validate_package(root: Path) -> list[str]:
    manifest = load_json(root / "manifest.json")
    document = manifest.get("document", {})
    errors: list[str] = []
    if not isinstance(document, dict) or (
        document.get("width"),
        document.get("height"),
        document.get("scale"),
    ) != (4096, 4096, 4):
        errors.append("document must be 4096x4096 at scale 4")
    output_psd = str(document.get("outputPsd", "")).replace("\\", "/") if isinstance(document, dict) else ""
    if not (output_psd.startswith("outputs/UI_PSD/") or "/outputs/UI_PSD/" in output_psd):
        errors.append("document outputPsd must be under outputs/UI_PSD/")
    image_layers = manifest.get("imageLayers", [])
    if isinstance(image_layers, list):
        for layer in image_layers:
            if isinstance(layer, dict):
                if "文字" in str(layer.get("name", "")):
                    errors.append("image layer name must not declare baked runtime text")
                image_path = str(layer.get("path", ""))
                if not (root / image_path).is_file():
                    errors.append(f"missing image layer file: {image_path}")
    text_layers = manifest.get("textLayers", [])
    required_text_fields = {"name", "text", "font", "color", "x", "y", "fontSize"}
    if isinstance(text_layers, list):
        for index, layer in enumerate(text_layers):
            if not isinstance(layer, dict) or not required_text_fields.issubset(layer):
                errors.append(f"invalid editable text layer: {index}")
                continue
            if not all(str(layer[field]).strip() for field in ("name", "text", "font", "color")):
                errors.append(f"invalid editable text layer: {index}")
    semantic_map = load_json(root / "semantic-map.json")
    runtime_backgrounds = semantic_map.get("runtimeBackgrounds", [])
    expected_runtime_pages = {"hud", "character", "companion", "task", "map", "backpack"}
    present_runtime_pages: set[str] = set()
    if not isinstance(runtime_backgrounds, list):
        errors.append("missing runtime page backgrounds")
    else:
        for background in runtime_backgrounds:
            if not isinstance(background, dict):
                continue
            page = str(background.get("page", ""))
            present_runtime_pages.add(page)
            background_file = str(background.get("file", ""))
            if not background_file or not (root / background_file).is_file():
                errors.append(f"missing runtime background file: {background_file}")
        missing_runtime_pages = sorted(expected_runtime_pages.difference(present_runtime_pages))
        if missing_runtime_pages:
            errors.append("missing runtime page backgrounds: " + ", ".join(missing_runtime_pages))
    semantic_assets = semantic_map.get("assets", [])
    asset_names: set[str] = set()
    present_semantics = {
        str(asset.get("buttonSemantic", ""))
        for asset in semantic_assets
        if isinstance(asset, dict)
    }
    if isinstance(semantic_assets, list):
        for asset in semantic_assets:
            if isinstance(asset, dict):
                asset_name = str(asset.get("ueAssetName", ""))
                if asset_name in asset_names:
                    errors.append(f"duplicate ueAssetName: {asset_name}")
                asset_names.add(asset_name)
    for semantic in sorted({"neutral", "primary", "destructive"}.difference(present_semantics)):
        errors.append(f"missing semantic button family: {semantic}")
    return errors


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Validate the authoritative town PSD package.")
    parser.add_argument("--root", type=Path, required=True, help="PSD package root directory")
    args = parser.parse_args(argv)
    errors = validate_package(args.root)
    print(json.dumps({"ok": not errors, "errors": errors}, ensure_ascii=False))
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())
