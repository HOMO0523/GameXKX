"""Validate production battle-animation texture build settings inside UE."""

from __future__ import annotations

import argparse
import json
import sys

try:
    import unreal
except ImportError:  # Pure report tests run outside the editor.
    unreal = None


ATLAS_ASSET_DIR = "/Game/GameXXK/BattleAnimations/Atlases"
ATLAS_SIZE = 4096
MAX_RESOURCE_SIZE_BYTES = 20 * 1024 * 1024
PILOT_ASSET_IDS = {
    "character_00_hero_idle",
    "character_00_hero_attack",
    "character_00_hero_hit",
    "enemy_01_rooster_idle",
    "enemy_01_rooster_attack",
    "enemy_01_rooster_hit",
}


def build_report(records: list[dict]) -> dict:
    failed = sorted(
        str(record.get("asset_id", ""))
        for record in records
        if not bool(record.get("ok"))
    )
    resource_sizes = [
        int(record.get("resource_size_bytes") or 0)
        for record in records
    ]
    return {
        "ok": not failed,
        "requested_count": len(records),
        "failed_asset_ids": failed,
        "max_resource_size_bytes": max(resource_sizes, default=0),
        "records": records,
    }


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("battle animation texture validation requires UE editor Python")


def _texture_size(texture: object) -> tuple[int, int]:
    for width_name, height_name in (
        ("blueprint_get_size_x", "blueprint_get_size_y"),
        ("get_size_x", "get_size_y"),
    ):
        width = getattr(texture, width_name, None)
        height = getattr(texture, height_name, None)
        if callable(width) and callable(height):
            return int(width()), int(height())
    return 0, 0


def _resource_size_bytes(texture: object) -> int:
    blueprint_getter = getattr(texture, "blueprint_get_memory_size", None)
    if callable(blueprint_getter):
        try:
            return int(blueprint_getter())
        except Exception:
            pass
    getter = getattr(texture, "get_resource_size_bytes", None)
    mode_type = getattr(unreal, "ResourceSizeMode", None)
    estimated_total = getattr(mode_type, "ESTIMATED_TOTAL", None) if mode_type else None
    if callable(getter) and estimated_total is not None:
        try:
            return int(getter(estimated_total))
        except Exception:
            return 0
    return 0


def validate_texture(asset_id: str) -> dict:
    _require_unreal()
    texture_name = f"T_{asset_id}_atlas"
    asset_path = f"{ATLAS_ASSET_DIR}/{texture_name}"
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if texture is None:
        return {
            "asset_id": asset_id,
            "path": asset_path,
            "ok": False,
            "errors": ["asset_missing"],
            "resource_size_bytes": 0,
        }

    size = list(_texture_size(texture))
    compression = str(texture.get_editor_property("compression_settings"))
    mip_gen = str(texture.get_editor_property("mip_gen_settings"))
    texture_filter = str(texture.get_editor_property("filter"))
    srgb = bool(texture.get_editor_property("srgb"))
    resource_size_bytes = _resource_size_bytes(texture)
    errors: list[str] = []
    if size != [ATLAS_SIZE, ATLAS_SIZE]:
        errors.append(f"size={size}")
    if "BC7" not in compression.upper():
        errors.append(f"compression={compression}")
    if "NO_MIPMAPS" not in mip_gen.upper():
        errors.append(f"mip_gen={mip_gen}")
    if "BILINEAR" not in texture_filter.upper():
        errors.append(f"filter={texture_filter}")
    if not srgb:
        errors.append("srgb=false")
    if resource_size_bytes <= 0:
        errors.append("resource_size_unavailable")
    elif resource_size_bytes > MAX_RESOURCE_SIZE_BYTES:
        errors.append(f"resource_size_bytes={resource_size_bytes}")
    return {
        "asset_id": asset_id,
        "path": asset_path,
        "size": size,
        "compression": compression,
        "mip_gen": mip_gen,
        "filter": texture_filter,
        "srgb": srgb,
        "resource_size_bytes": resource_size_bytes,
        "ok": not errors,
        "errors": errors,
    }


def main(argv: list[str]) -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--asset-id", action="append", default=[])
    args = parser.parse_args(argv)
    asset_ids = sorted(set(args.asset_id) or PILOT_ASSET_IDS)
    print(json.dumps(build_report([validate_texture(asset_id) for asset_id in asset_ids]), ensure_ascii=False))


if __name__ == "__main__":
    main(sys.argv[1:])
