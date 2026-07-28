"""Apply the approved battle backdrop to the one generated encounter-floor slot.

The tool intentionally refuses all non-canonical scene state.  It cannot spawn,
delete, move, rotate, scale, or retarget a camera; its only writable operation
is replacing slot zero on the named generated Plane actor after strict guards.
"""

from __future__ import annotations

import argparse
import json
from typing import Any

try:
    import unreal
except ModuleNotFoundError:
    unreal = None

from gamexxk_import_battle_backdrop import MATERIAL_ASSET_PATH, validate_backdrop_plan


MAP_PATH = "/Game/GameXXK/Maps/L_BattleScene"
FLOOR_LABEL = "GameXXK_Encounter_Floor"
FLOOR_MESH_PATH = "/Engine/BasicShapes/Plane"
WORLD_GRID_MATERIAL_PATH = "/Engine/EngineMaterials/WorldGridMaterial"


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("UE Python is required to apply the battle backdrop")


def _canonical_asset_path(asset_or_path: object) -> str:
    if hasattr(asset_or_path, "get_path_name"):
        value = str(asset_or_path.get_path_name())
    else:
        value = str(asset_or_path)
    return value.split(".", 1)[0]


def _vector_payload(vector: object) -> dict[str, float]:
    return {axis: float(getattr(vector, axis)) for axis in ("x", "y", "z")}


def _rotation_payload(rotation: object) -> dict[str, float]:
    return {axis: float(getattr(rotation, axis)) for axis in ("x", "y", "z", "w")}


def _transform_payload(actor: object) -> dict[str, Any]:
    transform = actor.get_actor_transform()
    return {
        "translation": _vector_payload(transform.translation),
        "rotation": _rotation_payload(transform.rotation),
        "scale": _vector_payload(transform.scale3d),
    }


def _load_battle_map() -> None:
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError(f"could not load battle scene map: {MAP_PATH}")


def _current_map_path() -> str:
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        raise RuntimeError("editor world is unavailable while inspecting the battle floor")
    return str(world.get_outermost().get_path_name()).split(".", 1)[0]


def _find_named_floor() -> object:
    actors = [
        actor
        for actor in unreal.EditorLevelLibrary.get_all_level_actors()
        if str(actor.get_actor_label()) == FLOOR_LABEL
    ]
    if len(actors) != 1:
        raise RuntimeError(f"expected exactly one generated battle floor {FLOOR_LABEL}, got {len(actors)}")
    floor = actors[0]
    if not isinstance(floor, unreal.StaticMeshActor):
        raise RuntimeError(f"battle floor must remain a StaticMeshActor: {floor.get_class().get_name()}")
    return floor


def inspect_floor() -> tuple[object, object, dict[str, Any]]:
    """Read and validate the one safe material-override target; does not mutate it."""
    _require_unreal()
    if _current_map_path() != MAP_PATH:
        raise RuntimeError("current map is not the guarded battle scene")
    floor = _find_named_floor()
    component = floor.get_editor_property("static_mesh_component")
    if component is None:
        raise RuntimeError("battle floor has no static mesh component")
    mesh = component.get_editor_property("static_mesh")
    mesh_path = _canonical_asset_path(mesh)
    if mesh_path != FLOOR_MESH_PATH:
        raise RuntimeError(f"battle floor mesh drifted from the generated Plane: {mesh_path}")
    if component.get_num_materials() != 1:
        raise RuntimeError("battle floor must retain exactly one material slot")
    current_material = component.get_material(0)
    current_material_path = _canonical_asset_path(current_material)
    allowed_materials = {WORLD_GRID_MATERIAL_PATH, MATERIAL_ASSET_PATH}
    if current_material_path not in allowed_materials:
        raise RuntimeError(
            "battle floor material is not the generated WorldGrid baseline or approved backdrop: "
            f"{current_material_path}"
        )
    report = {
        "map": MAP_PATH,
        "floor_label": FLOOR_LABEL,
        "floor_class": str(floor.get_class().get_path_name()),
        "mesh": mesh_path,
        "material_slots": int(component.get_num_materials()),
        "current_material": current_material_path,
        "before_transform": _transform_payload(floor),
    }
    return floor, component, report


def _load_target_material() -> object:
    if not unreal.EditorAssetLibrary.does_asset_exist(MATERIAL_ASSET_PATH):
        raise RuntimeError(
            f"battle backdrop material is missing: {MATERIAL_ASSET_PATH}. "
            "Run gamexxk_import_battle_backdrop.py --execute-import first."
        )
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL_ASSET_PATH)
    if not isinstance(material, unreal.Material) or _canonical_asset_path(material) != MATERIAL_ASSET_PATH:
        raise RuntimeError(f"battle backdrop material is invalid: {_canonical_asset_path(material)}")
    return material


def apply_backdrop() -> dict[str, Any]:
    """Apply exactly one guarded material-slot override and save only that map."""
    _require_unreal()
    plan = validate_backdrop_plan()
    _load_battle_map()
    floor, component, report = inspect_floor()
    target_material = _load_target_material()
    applied = report["current_material"] != MATERIAL_ASSET_PATH
    if applied:
        component.set_material(0, target_material)

    after_transform = _transform_payload(floor)
    if report["before_transform"] != after_transform:
        raise RuntimeError("battle floor transform changed while applying its material override")
    after_material = _canonical_asset_path(component.get_material(0))
    if after_material != MATERIAL_ASSET_PATH:
        raise RuntimeError(f"battle floor material override did not bind: {after_material}")
    saved = False
    if applied:
        saved = bool(unreal.EditorLoadingAndSavingUtils.save_current_level())
        if not saved:
            raise RuntimeError(f"could not save guarded battle scene map: {MAP_PATH}")
    return {
        **plan,
        **report,
        "applied": applied,
        "after_material": after_material,
        "after_transform": after_transform,
        "saved": saved,
    }


def main(argv: list[str] | None = None) -> dict[str, Any]:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--apply",
        action="store_true",
        help="Apply only the guarded material override after the separate import step.",
    )
    args = parser.parse_args(argv)
    result = apply_backdrop() if args.apply else validate_backdrop_plan()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return result


if __name__ == "__main__":
    main()
