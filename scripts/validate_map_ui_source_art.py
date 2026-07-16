#!/usr/bin/env python3
"""Validate WorldMap source atoms without altering their pixels."""

from __future__ import annotations

import argparse
from hashlib import sha256
import json
from pathlib import Path
import sys
from typing import Any

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ROOT = PROJECT_ROOT / "docs" / "ui" / "maps" / "source_art" / "WorldMap"

REQUIRED_LAYERS: dict[str, dict[str, object]] = {
    "world_map_terrain": {
        "file": "world_map_terrain.png",
        "kind": "terrain",
        "requiresAlpha": False,
        "forbiddenContent": frozenset(
            {"route", "node", "label", "player_marker", "settlement_marker", "skull", "text", "number"}
        ),
    },
    "world_map_region_paths": {
        "file": "world_map_region_paths.png",
        "kind": "decorative_path",
        "requiresAlpha": True,
        "forbiddenContent": frozenset(
            {"node", "label", "player_marker", "skull", "text", "number", "marker", "terrain", "frame"}
        ),
    },
    "world_map_qingshan_marker": {
        "file": "world_map_qingshan_marker.png",
        "kind": "marker",
        "requiresAlpha": True,
        "forbiddenContent": frozenset({"text", "number", "map", "terrain", "frame"}),
    },
    "world_map_locked_marker": {
        "file": "world_map_locked_marker.png",
        "kind": "marker",
        "requiresAlpha": True,
        "forbiddenContent": frozenset({"text", "number", "map", "terrain", "frame"}),
    },
    "world_map_player_marker": {
        "file": "world_map_player_marker.png",
        "kind": "marker",
        "requiresAlpha": True,
        "forbiddenContent": frozenset({"text", "number", "map", "terrain", "frame"}),
    },
    "world_map_label_plate": {
        "file": "world_map_label_plate.png",
        "kind": "label_plate",
        "requiresAlpha": True,
        "forbiddenContent": frozenset({"text", "number", "icon", "map", "frame"}),
    },
}


def read_manifest(root: Path) -> dict[str, Any]:
    manifest_path = root / "manifest.json"
    if not manifest_path.is_file():
        raise ValueError(f"missing manifest: {manifest_path}")
    try:
        payload = json.loads(manifest_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise ValueError(f"invalid manifest JSON: {error}") from error
    if not isinstance(payload, dict):
        raise ValueError("manifest root must be an object")
    return payload


def get_manifest_layers(manifest: dict[str, Any]) -> dict[str, dict[str, Any]]:
    raw_layers = manifest.get("layers")
    if not isinstance(raw_layers, list):
        raise ValueError("manifest layers must be a list")

    layers_by_name: dict[str, dict[str, Any]] = {}
    for raw_layer in raw_layers:
        if not isinstance(raw_layer, dict):
            raise ValueError("each manifest layer must be an object")
        name = raw_layer.get("name")
        if not isinstance(name, str) or not name:
            raise ValueError("each manifest layer needs a non-empty name")
        if name in layers_by_name:
            raise ValueError(f"duplicate map source layer: {name}")
        layers_by_name[name] = raw_layer

    missing_names = sorted(set(REQUIRED_LAYERS) - set(layers_by_name))
    if missing_names:
        raise ValueError(f"missing required layers: {', '.join(missing_names)}")
    unexpected_names = sorted(set(layers_by_name) - set(REQUIRED_LAYERS))
    if unexpected_names:
        raise ValueError(f"unexpected map source layers: {', '.join(unexpected_names)}")
    return layers_by_name


def validate_reference(manifest: dict[str, Any]) -> None:
    source_reference = manifest.get("sourceReference")
    if not isinstance(source_reference, dict):
        raise ValueError("manifest sourceReference must describe reference-only provenance")
    path = source_reference.get("path")
    if path != "PSD clean_assets_v2/094.png":
        raise ValueError("sourceReference path must be PSD clean_assets_v2/094.png")
    usage = source_reference.get("usage")
    if not isinstance(usage, str) or "reference only" not in usage.lower() or "not a runtime" not in usage.lower():
        raise ValueError("sourceReference must explicitly remain reference only and not a runtime source")


def validate_canvas(manifest: dict[str, Any]) -> None:
    canvas = manifest.get("canvas")
    if not isinstance(canvas, dict) or canvas.get("width") != 1920 or canvas.get("height") != 1080:
        raise ValueError("canvas must be 1920x1080")


def validate_layer(root: Path, layer: dict[str, Any]) -> dict[str, object]:
    name = str(layer["name"])
    expected = REQUIRED_LAYERS[name]
    file_name = str(layer.get("file", ""))
    if file_name == "094.png" or "094.png" in file_name or "完整底图" in file_name:
        raise ValueError("baked PSD map cannot be imported as runtime source")
    if file_name != expected["file"]:
        raise ValueError(f"{name} must use {expected['file']}, not {file_name}")

    image_path = (root / file_name).resolve()
    root_path = root.resolve()
    if root_path not in image_path.parents:
        raise ValueError(f"map source atom must stay under source root: {file_name}")
    if not image_path.is_file():
        raise ValueError(f"missing map source atom: {image_path}")

    requires_alpha = expected["requiresAlpha"]
    if layer.get("requiresAlpha") is not requires_alpha:
        raise ValueError(f"{name} requiresAlpha does not match the source-art contract")
    if layer.get("kind") != expected["kind"]:
        raise ValueError(f"{name} kind does not match the source-art contract")
    forbidden_content = layer.get("forbiddenContent")
    if not isinstance(forbidden_content, list) or not all(isinstance(item, str) for item in forbidden_content):
        raise ValueError(f"{name} forbiddenContent must be a list of strings")
    if set(forbidden_content) != expected["forbiddenContent"]:
        raise ValueError(f"{name} forbiddenContent does not match the source-art contract")

    with Image.open(image_path) as raw:
        if requires_alpha and raw.mode != "RGBA":
            raise ValueError(f"transparent atom must be RGBA: {image_path.name}")
        rgba = raw.convert("RGBA")
        alpha = rgba.getchannel("A")
        alpha_extrema = list(alpha.getextrema())
        if requires_alpha and alpha_extrema[0] >= 255:
            raise ValueError(f"transparent atom has no transparent pixels: {image_path.name}")
        if requires_alpha and alpha_extrema[1] <= 0:
            raise ValueError(f"transparent atom has no visible pixels: {image_path.name}")
        result: dict[str, object] = {
            "name": name,
            "file": file_name,
            "size": list(raw.size),
            "mode": raw.mode,
            "alphaExtrema": alpha_extrema,
            "sha256": sha256(image_path.read_bytes()).hexdigest(),
        }

    for key in ("size", "mode", "alphaExtrema", "sha256"):
        if layer.get(key) != result[key]:
            raise ValueError(f"{name} manifest {key} does not match {file_name}")
    return result


def validate(root: Path) -> dict[str, object]:
    report: dict[str, object] = {
        "ok": False,
        "root": str(root),
        "layers": [],
        "errors": [],
    }
    try:
        manifest = read_manifest(root)
        validate_canvas(manifest)
        validate_reference(manifest)
        layers_by_name = get_manifest_layers(manifest)
        validated_layers = [validate_layer(root, layers_by_name[name]) for name in REQUIRED_LAYERS]
        report["layers"] = validated_layers
        report["ok"] = True
    except (OSError, ValueError) as error:
        report["errors"] = [str(error)]
    return report


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate GameXXK WorldMap source-art provenance and alpha contract")
    parser.add_argument("--check", action="store_true", help="validate source art and emit a JSON report")
    parser.add_argument("--root", type=Path, default=DEFAULT_ROOT, help="WorldMap source-art directory")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not args.check:
        raise SystemExit("--check is required")
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    report = validate(args.root)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
