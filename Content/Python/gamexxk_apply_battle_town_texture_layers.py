"""Guarded non-Landscape fallback for the Qingshan battle ground.

UE 5.8 asserts while loading the town's Landscape outside a live viewport, so
this writer uses the town's verified ground texture layer directly.  It never
loads the town map and may change only material slot zero on the exact battle
floor actor after an explicit preflight and execute request.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

try:
    import unreal
except ModuleNotFoundError:
    unreal = None


PROJECT_ROOT = Path(__file__).resolve().parents[2]
BATTLE_MAP = "/Game/GameXXK/Maps/L_BattleScene"
ENCOUNTER_FLOOR_ID = "GameXXK_Encounter_Floor"
V1_TERRAIN_MESH = "/Game/GameXXK/Environment/Battle/TownTerrain/SM_Battle_QingshanGround_01"
V1_CAPTURE_MATERIAL = "/Game/GameXXK/Environment/Battle/TownTerrain/M_Battle_QingshanGround_01"
OUTPUT_ROOT = "/Game/GameXXK/Environment/Battle/TownTerrainV3"
OUTPUT_MATERIAL = OUTPUT_ROOT + "/M_Battle_QingshanGround_TownLayer_01"
AUDITED_BATTLE_SHA256 = "0056e4a93ff7cc9d89ba1103c3615fec8b4ac76b0258e7bb295a97855fc94493"

# These are direct source textures of the town's primary ground layer.  The
# Landscape parent material itself is intentionally not reused: it requires
# Landscape weightmaps and may fall back to an empty/gray layer on a mesh.
TOWN_TEXTURE_ASSETS = {
    "ground_base_color": "/Game/Asian_Village/textures/landscape_textures/ground/T_ground_BaseColor",
    "ground_normal": "/Game/Asian_Village/textures/landscape_textures/ground/T_ground_Normal",
}


def _parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--preflight", action="store_true", help="Verify only; write no UE asset or map.")
    parser.add_argument("--execute", action="store_true", help="Create and apply the owned town-layer material.")
    args = parser.parse_args(argv)
    if args.preflight == args.execute:
        parser.error("pass exactly one of --preflight or --execute")
    return args


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("UE Python is required for the town-layer material pipeline")


def _require_town_texture_assets(asset_library: object) -> dict[str, str]:
    missing = [
        path
        for path in TOWN_TEXTURE_ASSETS.values()
        if not bool(asset_library.does_asset_exist(path))
    ]
    if missing:
        raise RuntimeError("town terrain source texture is missing: " + ", ".join(missing))
    return dict(TOWN_TEXTURE_ASSETS)


def _battle_package_path() -> Path:
    return PROJECT_ROOT / "Content" / Path(BATTLE_MAP.removeprefix("/Game/")).with_suffix(".umap")


def _verify_battle_baseline() -> str:
    package = _battle_package_path()
    if not package.is_file():
        raise RuntimeError(f"battle map package is missing: {package}")
    current = hashlib.sha256(package.read_bytes()).hexdigest()
    if current != AUDITED_BATTLE_SHA256:
        raise RuntimeError(
            "battle map baseline changed since the material fallback was audited; "
            "review the current floor binding before applying"
        )
    return current


def _require_output_absent(asset_library: object) -> None:
    if asset_library.does_asset_exist(OUTPUT_MATERIAL):
        raise RuntimeError(f"refusing to overwrite owned town-layer material: {OUTPUT_MATERIAL}")


def _connect_expression(material: object, source: object, source_output: str, target: object, inputs: tuple[str, ...]) -> None:
    for input_name in inputs:
        if unreal.MaterialEditingLibrary.connect_material_expressions(
            source,
            source_output,
            target,
            input_name,
        ):
            return
    raise RuntimeError(f"could not connect material input candidates: {inputs}")


def _load_source_textures(asset_library: object) -> dict[str, object]:
    textures: dict[str, object] = {}
    for name, path in _require_town_texture_assets(asset_library).items():
        texture = asset_library.load_asset(path)
        if texture is None:
            raise RuntimeError(f"could not load verified town terrain source texture: {path}")
        textures[name] = texture
    return textures


def _create_town_layer_material(textures: dict[str, object]) -> object:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = asset_tools.create_asset(
        OUTPUT_MATERIAL.rsplit("/", 1)[-1],
        OUTPUT_ROOT,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if material is None:
        raise RuntimeError("could not create owned town-layer material")

    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_SURFACE)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property("two_sided", False)
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    coordinates = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureCoordinate,
        -650,
        -60,
    )
    coordinates.set_editor_property("u_tiling", 10.0)
    coordinates.set_editor_property("v_tiling", 6.0)
    base_color = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        -360,
        -60,
    )
    base_color.set_editor_property("texture", textures["ground_base_color"])
    base_color.set_editor_property("parameter_name", "TownGroundBaseColor")
    _connect_expression(material, coordinates, "", base_color, ("UVs",))

    # The original source also carries a normal map (validated in preflight),
    # but the battle screen uses a deliberately matte unlit ground so its
    # painterly town colors do not become gray under a different light rig.
    unlit = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionSubstrateUnlitBSDF,
        20,
        0,
    )
    _connect_expression(material, base_color, "RGB", unlit, ("Emissive Color", "EmissiveColor"))
    if not unreal.MaterialEditingLibrary.connect_material_property(
        unlit,
        "",
        unreal.MaterialProperty.MP_FRONT_MATERIAL,
    ):
        raise RuntimeError("could not connect town-layer material to Front Material")

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    errors = [str(error) for error in unreal.MaterialEditingLibrary.recompile_material(material)]
    if errors:
        raise RuntimeError(f"town-layer material failed to compile: {errors}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("could not save owned town-layer material")
    return material


def _find_floor_component(audit: Any) -> tuple[object, object]:
    floor = audit.find_single_encounter_floor()
    components = list(floor.get_components_by_class(unreal.StaticMeshComponent))
    if len(components) != 1:
        raise RuntimeError(
            f"{ENCOUNTER_FLOOR_ID} must expose exactly one StaticMeshComponent, got {len(components)}"
        )
    return floor, components[0]


def _apply_material_only(audit: Any, material: object) -> dict[str, object]:
    floor, component = _find_floor_component(audit)
    before_transform = audit._transform_payload(floor)
    mesh_path = audit._object_path(component.get_editor_property("static_mesh"))
    if mesh_path != V1_TERRAIN_MESH:
        raise RuntimeError(
            "battle floor mesh no longer matches the audited V1 town terrain; "
            f"expected {V1_TERRAIN_MESH}, got {mesh_path}"
        )
    previous_material = audit._object_path(component.get_material(0))
    component.set_material(0, material)
    after_transform = audit._transform_payload(floor)
    if before_transform != after_transform:
        raise RuntimeError("refusing to continue: battle floor transform changed during material replacement")
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("could not save L_BattleScene after material-only replacement")
    return {
        "actor_label": audit._actor_label(floor),
        "mesh": mesh_path,
        "previous_material": previous_material,
        "material": audit._object_path(component.get_material(0)),
        "transform": after_transform,
    }


def _preflight() -> dict[str, object]:
    _require_unreal()
    asset_library = unreal.EditorAssetLibrary
    sources = _require_town_texture_assets(asset_library)
    baseline = _verify_battle_baseline()
    return {
        "ok": True,
        "operation": "preflight",
        "battle_map": BATTLE_MAP,
        "battle_sha256": baseline,
        "source_textures": sources,
        "v1_terrain_mesh_exists": bool(asset_library.does_asset_exist(V1_TERRAIN_MESH)),
        "v1_capture_material_exists": bool(asset_library.does_asset_exist(V1_CAPTURE_MATERIAL)),
        "output_material": OUTPUT_MATERIAL,
        "output_absent": not bool(asset_library.does_asset_exist(OUTPUT_MATERIAL)),
    }


def _execute() -> dict[str, object]:
    import gamexxk_audit_battle_town_terrain as audit

    preflight = _preflight()
    if not preflight["v1_terrain_mesh_exists"] or not preflight["v1_capture_material_exists"]:
        raise RuntimeError("the audited V1 terrain rollback resources are missing")
    if not bool(preflight["output_absent"]):
        _require_output_absent(unreal.EditorAssetLibrary)

    audit._require_audit_session_safe()
    original_map = audit._capture_original_map()
    try:
        audit._load_map_for_reading(BATTLE_MAP)
        material = _create_town_layer_material(_load_source_textures(unreal.EditorAssetLibrary))
        floor = _apply_material_only(audit, material)
        audit._require_audit_session_safe()
    finally:
        audit._restore_original_map(original_map)
    return {"ok": True, "operation": "execute", "preflight": preflight, "encounter_floor": floor}


def main(argv: list[str] | None = None) -> dict[str, object]:
    args = _parse_args(argv)
    result = _preflight() if args.preflight else _execute()
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return result


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except RuntimeError as exc:
        print(str(exc), file=sys.stderr)
        raise SystemExit(1) from exc
