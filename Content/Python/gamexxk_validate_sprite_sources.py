from __future__ import annotations

import json
import hashlib
import struct
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]

SPRITE_SOURCES = [
    {
        "id": "hero_deepblue_high_ponytail_walk_8dir",
        "file": PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "hero_deepblue_high_ponytail_walk_8dir.png",
        "asset": "/Game/GameXXK/Sprites/Generated/hero_deepblue_high_ponytail_walk_8dir",
        "expected_class": "Texture2D",
        "sha256": "813c5e2f5b34225518fc8d4f7d4549de184e932790dfb6ddb9f908c67cfeeb50",
        "width": 1024,
        "height": 1024,
    },
    {
        "id": "follower_blue_scholar_walk_8dir",
        "file": PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "follower_blue_scholar_walk_8dir.png",
        "asset": "/Game/GameXXK/Sprites/Generated/follower_blue_scholar_walk_8dir",
        "expected_class": "Texture2D",
        "sha256": "9cb26faff5a39efa31a827c350746708f29ed5f9c3cec5f0595d6fcc8a6546f1",
        "width": 1024,
        "height": 1024,
    },
    {
        "id": "merchant_teal_robed_walk_8dir",
        "file": PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "merchant_teal_robed_walk_8dir.png",
        "asset": "/Game/GameXXK/Sprites/Generated/merchant_teal_robed_walk_8dir",
        "expected_class": "Texture2D",
        "sha256": "5084a737ac77e9edd83b5686031fcd88c35df7b314c98bc3fe85c7589070369c",
        "width": 1024,
        "height": 1024,
    },
]


def _read_png_dimensions(file_path: Path) -> tuple[int, int]:
    with file_path.open("rb") as file:
        header = file.read(24)
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG file")
    return struct.unpack(">II", header[16:24])


def _sha256(file_path: Path) -> str:
    digest = hashlib.sha256()
    with file_path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> None:
    missing_files = []
    invalid_files = []
    missing_assets = []
    invalid_assets = []

    for source in SPRITE_SOURCES:
        file_path = Path(source["file"])
        asset_path = str(source["asset"])
        if not file_path.exists():
            missing_files.append(str(file_path))
        else:
            actual_hash = _sha256(file_path)
            if actual_hash != source["sha256"]:
                invalid_files.append({
                    "file": str(file_path),
                    "reason": "sha256_mismatch",
                    "expected": source["sha256"],
                    "actual": actual_hash,
                })
            try:
                width, height = _read_png_dimensions(file_path)
            except ValueError as exc:
                invalid_files.append({
                    "file": str(file_path),
                    "reason": str(exc),
                })
            else:
                if width != source["width"] or height != source["height"]:
                    invalid_files.append({
                        "file": str(file_path),
                        "reason": "dimension_mismatch",
                        "expected": [source["width"], source["height"]],
                        "actual": [width, height],
                    })
        if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            missing_assets.append(asset_path)
            continue
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        asset_class = asset.get_class().get_name() if asset else ""
        if asset_class != source["expected_class"]:
            invalid_assets.append({
                "asset": asset_path,
                "expected_class": source["expected_class"],
                "actual_class": asset_class,
            })

    report = {
        "ok": not missing_files and not invalid_files and not missing_assets and not invalid_assets,
        "sprite_count": len(SPRITE_SOURCES),
        "missing_files": missing_files,
        "invalid_files": invalid_files,
        "missing_assets": missing_assets,
        "invalid_assets": invalid_assets,
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
