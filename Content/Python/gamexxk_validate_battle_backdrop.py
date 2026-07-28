"""Read-only verification for the generated battle backdrop and guarded floor slot."""

from __future__ import annotations

import argparse
import json
from typing import Any

try:
    import unreal
except ModuleNotFoundError:
    unreal = None

from gamexxk_apply_battle_backdrop import inspect_floor
from gamexxk_import_battle_backdrop import (
    MATERIAL_ASSET_PATH,
    TEXTURE_ASSET_PATH,
    _validate_material,
    _validate_texture,
    validate_backdrop_plan,
)


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("UE Python is required to validate imported battle backdrop assets")


def validate_imported_backdrop() -> dict[str, Any]:
    """Validate the isolated Texture2D and Substrate material without editing them."""
    _require_unreal()
    plan = validate_backdrop_plan()
    if not unreal.EditorAssetLibrary.does_asset_exist(TEXTURE_ASSET_PATH):
        raise RuntimeError(f"battle backdrop texture is missing: {TEXTURE_ASSET_PATH}")
    if not unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_ASSET_PATH):
        raise RuntimeError(f"battle backdrop material is missing: {MATERIAL_ASSET_PATH}")
    texture = unreal.EditorAssetLibrary.load_asset(TEXTURE_ASSET_PATH)
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL_ASSET_PATH)
    texture_report = _validate_texture(texture, plan["source"], plan["source_size"])
    material_report = _validate_material(material, texture)
    return {**plan, "texture": texture_report, "material": material_report}


def validate_applied_floor() -> dict[str, Any]:
    """Validate the guarded floor's current material binding without changing it."""
    _require_unreal()
    floor, component, report = inspect_floor()
    if report["current_material"] != MATERIAL_ASSET_PATH:
        raise RuntimeError(
            "battle backdrop is not applied to the guarded floor: "
            f"{report['current_material']}"
        )
    return {
        **report,
        "after_transform": report["before_transform"],
        "applied_material": report["current_material"],
        "read_only": True,
    }


def main(argv: list[str] | None = None) -> dict[str, Any]:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--verify-ue",
        action="store_true",
        help="Read only the imported assets and currently applied guarded floor.",
    )
    args = parser.parse_args(argv)
    if not args.verify_ue:
        result = validate_backdrop_plan()
    else:
        result = {"assets": validate_imported_backdrop(), "floor": validate_applied_floor()}
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return result


if __name__ == "__main__":
    main()
