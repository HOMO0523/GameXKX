#!/usr/bin/env python3
"""Validate GameXXK inventory UI asset manifests."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MANIFEST_DIR = PROJECT_ROOT / "Content" / "GameXXK" / "UI" / "Inventory" / "Manifests"
REQUIRED_FIELDS = {"id", "widget", "assetType", "targetTexture", "canvasSize", "states"}
TARGET_PREFIX = "/Game/GameXXK/UI/Inventory/Textures/"


def validate_manifest(path: Path) -> list[str]:
    errors: list[str] = []
    try:
        data: dict[str, Any] = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        return [f"{path.name}: invalid json: {exc}"]

    missing = sorted(REQUIRED_FIELDS.difference(data))
    if missing:
        errors.append(f"{path.name}: missing fields {missing}")

    target = data.get("targetTexture")
    if not isinstance(target, str) or not target.startswith(TARGET_PREFIX):
        errors.append(f"{path.name}: targetTexture must start with {TARGET_PREFIX}")

    canvas = data.get("canvasSize")
    if (
        not isinstance(canvas, list)
        or len(canvas) != 2
        or not all(isinstance(value, int) and value > 0 for value in canvas)
    ):
        errors.append(f"{path.name}: canvasSize must be [positive_int, positive_int]")

    states = data.get("states")
    if not isinstance(states, list) or not states or not all(isinstance(state, str) and state for state in states):
        errors.append(f"{path.name}: states must be a non-empty string array")

    return errors


def main() -> int:
    manifest_paths = sorted(MANIFEST_DIR.glob("*.json"))
    errors: list[str] = []
    if not manifest_paths:
        errors.append(f"no manifest json files found under {MANIFEST_DIR}")

    for path in manifest_paths:
        errors.extend(validate_manifest(path))

    result = {
        "ok": not errors,
        "manifest_dir": str(MANIFEST_DIR),
        "manifest_count": len(manifest_paths),
        "errors": errors,
    }
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
