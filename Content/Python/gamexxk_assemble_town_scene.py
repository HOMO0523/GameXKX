from __future__ import annotations

import json
from typing import Any

import unreal


MAP_PATH = "/Game/GameXXK/Maps/L_QingshanInn"
EXPECTED_GAME_MODE = "/Script/GameXXK.GameXXKMVPGameMode"

CUBE_MESH_PATH = "/Engine/BasicShapes/Cube.Cube"

STATIC_MESH_ACTORS = [
    {
        "label": "QingshanInn_Terrain_Ground",
        "location": [0.0, 0.0, -25.0],
        "rotation": [0.0, 0.0, 0.0],
        "scale": [48.0, 32.0, 0.5],
    },
    {
        "label": "QingshanInn_Terrain_MainRoad",
        "location": [0.0, 0.0, 5.0],
        "rotation": [0.0, 0.0, 0.0],
        "scale": [5.2, 26.0, 0.08],
    },
    {
        "label": "QingshanInn_Terrain_Plaza",
        "location": [-900.0, -350.0, 8.0],
        "rotation": [0.0, 0.0, 0.0],
        "scale": [13.0, 8.0, 0.08],
    },
    {
        "label": "QingshanInn_Bound_North",
        "location": [0.0, 1650.0, 55.0],
        "rotation": [0.0, 0.0, 0.0],
        "scale": [48.0, 0.6, 1.1],
    },
    {
        "label": "QingshanInn_Bound_South",
        "location": [0.0, -1650.0, 55.0],
        "rotation": [0.0, 0.0, 0.0],
        "scale": [48.0, 0.6, 1.1],
    },
    {
        "label": "QingshanInn_Bound_East",
        "location": [2450.0, 0.0, 55.0],
        "rotation": [0.0, 0.0, 0.0],
        "scale": [0.6, 32.0, 1.1],
    },
    {
        "label": "QingshanInn_Bound_West",
        "location": [-2450.0, 0.0, 55.0],
        "rotation": [0.0, 0.0, 0.0],
        "scale": [0.6, 32.0, 1.1],
    },
    {
        "label": "QingshanInn_DungeonGate",
        "location": [0.0, 1380.0, 105.0],
        "rotation": [0.0, 0.0, 0.0],
        "scale": [1.2, 4.5, 2.1],
    },
]

SCENE_ACTORS = [
    {
        "label": "QingshanInn_KeySun",
        "class_path": "/Script/Engine.DirectionalLight",
        "location": [-1200.0, -900.0, 1200.0],
        "rotation": [-45.0, -35.0, 0.0],
        "component_class": "/Script/Engine.DirectionalLightComponent",
        "component_props": {"intensity": 3.5, "source_angle": 2.0},
    },
    {
        "label": "QingshanInn_SkyLight",
        "class_path": "/Script/Engine.SkyLight",
        "location": [0.0, 0.0, 600.0],
        "rotation": [0.0, 0.0, 0.0],
        "component_class": "/Script/Engine.SkyLightComponent",
        "component_props": {"intensity": 0.8},
    },
    {
        "label": "QingshanInn_SkyAtmosphere",
        "class_path": "/Script/Engine.SkyAtmosphere",
        "location": [0.0, 0.0, 0.0],
        "rotation": [0.0, 0.0, 0.0],
    },
    {
        "label": "QingshanInn_MorningFog",
        "class_path": "/Script/Engine.ExponentialHeightFog",
        "location": [0.0, 0.0, 0.0],
        "rotation": [0.0, 0.0, 0.0],
        "component_class": "/Script/Engine.ExponentialHeightFogComponent",
        "component_props": {"fog_density": 0.015, "fog_height_falloff": 0.2},
    },
    {
        "label": "QingshanInn_TownExit",
        "class_path": "/Script/GameXXK.GameXXKTownExitActor",
        "location": [0.0, 1380.0, 120.0],
        "rotation": [0.0, 0.0, 0.0],
    },
    {
        "label": "PlayerStart_QingshanInn",
        "class_path": "/Script/Engine.PlayerStart",
        "location": [-1250.0, -900.0, 120.0],
        "rotation": [0.0, 35.0, 0.0],
    },
]


def _vector(values: list[float]) -> unreal.Vector:
    return unreal.Vector(values[0], values[1], values[2])


def _rotator(values: list[float]) -> unreal.Rotator:
    return unreal.Rotator(values[0], values[1], values[2])


def _get_level_subsystem():
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if subsystem is None:
        raise RuntimeError("LevelEditorSubsystem is unavailable")
    return subsystem


def _load_map() -> None:
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError(f"Required map does not exist: {MAP_PATH}")
    if not _get_level_subsystem().load_level(MAP_PATH):
        raise RuntimeError(f"Failed to load map: {MAP_PATH}")


def _find_actor_by_label(label: str):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        try:
            if actor.get_actor_label() == label:
                return actor
        except Exception:
            continue
    return None


def _same_vector(left, right, tolerance: float = 0.1) -> bool:
    return (
        abs(float(left.x) - float(right.x)) <= tolerance
        and abs(float(left.y) - float(right.y)) <= tolerance
        and abs(float(left.z) - float(right.z)) <= tolerance
    )


def _same_rotator(left, right, tolerance: float = 0.1) -> bool:
    return (
        abs(float(left.pitch) - float(right.pitch)) <= tolerance
        and abs(float(left.yaw) - float(right.yaw)) <= tolerance
        and abs(float(left.roll) - float(right.roll)) <= tolerance
    )


def _set_prop(obj, prop_names, value) -> bool:
    if obj is None:
        return False
    if isinstance(prop_names, str):
        prop_names = [prop_names]
    for prop_name in prop_names:
        try:
            current = obj.get_editor_property(prop_name)
            if current == value:
                return False
            obj.set_editor_property(prop_name, value)
            return True
        except Exception:
            continue
    return False


def _get_component(actor, class_path: str):
    component_class = unreal.load_class(None, class_path)
    if component_class is None:
        return None
    try:
        return actor.get_component_by_class(component_class)
    except Exception:
        return None


def _move_actor_if_needed(actor, location: unreal.Vector, rotation: unreal.Rotator, scale: unreal.Vector | None = None) -> bool:
    changed = False
    if not _same_vector(actor.get_actor_location(), location):
        actor.set_actor_location(location, False, False)
        changed = True
    if not _same_rotator(actor.get_actor_rotation(), rotation):
        actor.set_actor_rotation(rotation, False)
        changed = True
    if scale is not None and not _same_vector(actor.get_actor_scale3d(), scale):
        actor.set_actor_scale3d(scale)
        changed = True
    return changed


def _spawn_or_get_actor(label: str, class_path: str, location: unreal.Vector, rotation: unreal.Rotator, scale: unreal.Vector | None = None):
    actor = _find_actor_by_label(label)
    created = False
    if actor is None:
        actor_class = unreal.load_class(None, class_path)
        if actor_class is None:
            raise RuntimeError(f"Could not load class: {class_path}")
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, location, rotation)
        if actor is None:
            raise RuntimeError(f"Failed to spawn actor {label} from {class_path}")
        actor.set_actor_label(label)
        created = True

    changed = _move_actor_if_needed(actor, location, rotation, scale)
    return actor, created, changed


def _ensure_static_mesh_actor(spec: dict[str, Any], cube_mesh) -> dict[str, object]:
    actor, created, changed = _spawn_or_get_actor(
        str(spec["label"]),
        "/Script/Engine.StaticMeshActor",
        _vector(spec["location"]),
        _rotator(spec["rotation"]),
        _vector(spec["scale"]),
    )
    component = _get_component(actor, "/Script/Engine.StaticMeshComponent")
    mesh_changed = False
    if component is not None:
        try:
            current_mesh = component.get_editor_property("static_mesh")
        except Exception:
            current_mesh = None
        if current_mesh != cube_mesh:
            if hasattr(component, "set_static_mesh"):
                component.set_static_mesh(cube_mesh)
            else:
                _set_prop(component, ["static_mesh", "StaticMesh"], cube_mesh)
            mesh_changed = True
        _set_prop(component, ["mobility", "Mobility"], unreal.ComponentMobility.STATIC)
    return {"label": spec["label"], "created": created, "changed": changed or mesh_changed}


def _ensure_scene_actor(spec: dict[str, Any]) -> dict[str, object]:
    actor, created, changed = _spawn_or_get_actor(
        str(spec["label"]),
        str(spec["class_path"]),
        _vector(spec["location"]),
        _rotator(spec["rotation"]),
    )
    component_class = spec.get("component_class")
    if component_class:
        component = _get_component(actor, str(component_class))
        for prop_name, value in dict(spec.get("component_props", {})).items():
            changed = _set_prop(component, prop_name, value) or changed
    return {"label": spec["label"], "created": created, "changed": changed}


def _configure_world_settings() -> bool:
    game_mode_class = unreal.load_class(None, EXPECTED_GAME_MODE)
    if game_mode_class is None:
        raise RuntimeError(f"Could not load GameMode class: {EXPECTED_GAME_MODE}")
    world = unreal.EditorLevelLibrary.get_editor_world()
    world_settings = world.get_world_settings() if world else None
    if world_settings is None:
        raise RuntimeError("Editor world settings unavailable")
    changed = _set_prop(world_settings, ["default_game_mode", "DefaultGameMode"], game_mode_class)
    return changed


def main() -> None:
    _load_map()
    cube_mesh = unreal.load_object(None, CUBE_MESH_PATH)
    if cube_mesh is None:
        raise RuntimeError(f"Could not load mesh: {CUBE_MESH_PATH}")

    static_results = [_ensure_static_mesh_actor(spec, cube_mesh) for spec in STATIC_MESH_ACTORS]
    scene_results = [_ensure_scene_actor(spec) for spec in SCENE_ACTORS]
    game_mode_changed = _configure_world_settings()

    unreal.EditorAssetLibrary.save_asset(MAP_PATH, only_if_is_dirty=False)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    report = {
        "map": MAP_PATH,
        "static_mesh_actors": static_results,
        "scene_actors": scene_results,
        "game_mode_changed": game_mode_changed,
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
