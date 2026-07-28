#!/usr/bin/env python3
"""Derive alpha PNGs for the approved generated PartyDeck scrollbar source."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "ui-scrollbar"
MANIFEST_PATH = SOURCE_ROOT / "scrollbar_generated_manifest_v1.json"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _load_manifest() -> dict[str, Any]:
    return json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))


def _require_image(path: Path, expected_hash: str, expected_pixels: list[int]) -> Image.Image:
    if not path.is_file():
        raise FileNotFoundError(path)
    if _sha256(path).lower() != expected_hash.lower():
        raise ValueError(f"source hash changed: {path}")
    image = Image.open(path).convert("RGBA")
    if list(image.size) != expected_pixels:
        raise ValueError(f"source pixels changed: {path}: {image.size}")
    if image.getchannel("A").getpixel((0, 0)) != 0:
        raise ValueError("alpha source must have transparent corners")
    return image


def build_plan() -> dict[str, Any]:
    manifest = _load_manifest()
    phase2 = manifest["phase2"]
    alpha_path = SOURCE_ROOT / phase2["alphaSource"]
    image = _require_image(alpha_path, phase2["sha256"], phase2["pixels"])
    records: list[dict[str, Any]] = []
    for component in phase2["components"]:
        left, top, right, bottom = component["region"]
        region = image.crop((left, top, right, bottom))
        bbox = region.getchannel("A").getbbox()
        if bbox is None:
            raise ValueError(f"no opaque component for {component['id']}")
        padding = int(component["padding"])
        crop = (
            max(left, left + bbox[0] - padding),
            max(top, top + bbox[1] - padding),
            min(right, left + bbox[2] + padding),
            min(bottom, top + bbox[3] + padding),
        )
        derived = image.crop(crop)
        if derived.getchannel("A").getpixel((0, 0)) != 0:
            raise ValueError(f"component lost transparent padding: {component['id']}")
        records.append(
            {
                **component,
                "source": alpha_path,
                "crop": crop,
                "pixels": list(derived.size),
                "derived_path": SOURCE_ROOT / component["derivedSource"],
                "image": derived,
            }
        )
    return {"ok": True, "assets": records}


def write_derived_assets() -> dict[str, Any]:
    plan = build_plan()
    for record in plan["assets"]:
        target = record["derived_path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        record["image"].save(target, "PNG")
        record["sha256"] = _sha256(target)
        record.pop("image")
    return plan


def _jsonable(value: Any) -> Any:
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, Image.Image):
        return {"mode": value.mode, "size": list(value.size)}
    if isinstance(value, dict):
        return {key: _jsonable(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_jsonable(item) for item in value]
    return value


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true", help="write the two derived alpha PNGs")
    args = parser.parse_args()
    plan = write_derived_assets() if args.write else build_plan()
    print(json.dumps(_jsonable(plan), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
