#!/usr/bin/env python3
"""Prepare 2K staging copies of approved 4K battle-animation atlases (disk-only).

Never touches UE assets or the 4K production masters: reads each production atlas
PNG, writes a 2048x2048 LANCZOS copy plus an adjusted manifest under
SourceAssets/AnimationProcessing/Production2K, so the production importer can
re-import the same grid at half resolution.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:  # pragma: no cover
    Image = None


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PRODUCTION_ROOT = PROJECT_ROOT / "SourceAssets/AnimationProcessing/Production"
STAGING_BASE = PROJECT_ROOT / "SourceAssets/AnimationProcessing"
DEFAULT_ASSET_IDS = (
    "character_00_hero_idle",
    "character_00_hero_attack",
    "enemy_01_rooster_idle",
)


def staging_root_for(atlas_size: int) -> Path:
    label = f"{atlas_size // 1024}K" if atlas_size >= 1024 else str(atlas_size)
    return STAGING_BASE / f"Production{label}"


def png_size(path: Path) -> tuple[int, int] | None:
    data = path.read_bytes()[:33]
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    width, height = int.from_bytes(data[16:20], "big"), int.from_bytes(data[20:24], "big")
    return width, height


def prepare_asset(asset_id: str, atlas_size: int = 2048) -> dict:
    if atlas_size <= 0 or atlas_size % 8 != 0:
        raise RuntimeError(f"atlas size must be a positive multiple of 8, got {atlas_size}")
    cell_size = atlas_size // 8
    source_dir = PRODUCTION_ROOT / asset_id
    manifest_path = source_dir / "manifest.json"
    if not manifest_path.is_file():
        raise RuntimeError(f"{asset_id}: production manifest missing: {manifest_path}")
    payload = json.loads(manifest_path.read_text(encoding="utf-8"))
    atlas_path = Path(str(payload.get("atlas", "")))
    if atlas_path.name != f"{asset_id}_atlas.png":
        raise RuntimeError(f"{asset_id}: unexpected atlas path: {atlas_path}")
    size = png_size(atlas_path)
    if size != (4096, 4096):
        raise RuntimeError(f"{asset_id}: expected 4096x4096 source atlas, got {size}")

    if Image is None:
        raise RuntimeError("Pillow is required to prepare downscaled atlases")

    staging_root = staging_root_for(atlas_size)
    staging_dir = staging_root / asset_id / "atlas"
    staging_dir.mkdir(parents=True, exist_ok=True)
    staging_atlas = staging_dir / f"{asset_id}_atlas.png"
    with Image.open(atlas_path) as image:
        if image.mode not in ("RGB", "RGBA"):
            image = image.convert("RGBA")
        resized = image.resize((atlas_size, atlas_size), Image.LANCZOS)
        resized.save(staging_atlas, optimize=False)

    staged_payload = dict(payload)
    staged_payload["canvasSize"] = cell_size
    staged_payload["atlas"] = str(staging_atlas.resolve())
    staged_payload["atlasGrid"] = {
        "columns": 8,
        "rows": 8,
        "cellWidth": cell_size,
        "cellHeight": cell_size,
    }
    (staging_root / asset_id / "manifest.json").write_text(
        json.dumps(staged_payload, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return {
        "asset_id": asset_id,
        "staging_atlas": str(staging_atlas),
        "staging_size": png_size(staging_atlas),
        "cell_size": cell_size,
        "staging_root": str(staging_root),
    }


def discover_all_asset_ids() -> list[str]:
    """Enumerate every production asset that owns a valid manifest (disk-only)."""
    ids = []
    for manifest_path in sorted(PRODUCTION_ROOT.glob("*/manifest.json")):
        asset_id = manifest_path.parent.name
        if asset_id:
            ids.append(asset_id)
    return ids


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("asset_ids", nargs="*", default=list(DEFAULT_ASSET_IDS))
    parser.add_argument("--atlas-size", type=int, default=2048)
    parser.add_argument("--all", action="store_true")
    args = parser.parse_args()
    if args.all:
        args.asset_ids = discover_all_asset_ids()
    results = []
    for asset_id in args.asset_ids:
        results.append(prepare_asset(asset_id, args.atlas_size))
    print(json.dumps({"ok": True, "results": results}, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:  # pragma: no cover
        print(json.dumps({"ok": False, "error": str(exc)}, ensure_ascii=False))
        sys.exit(1)
