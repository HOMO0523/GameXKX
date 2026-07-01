from __future__ import annotations

import json

import unreal


MAP_PATH = "/Game/GameXXK/Maps/L_QingshanInn"
EXPECTED_GAME_MODE = "/Script/GameXXK.GameXXKMVPGameMode"

STATIC_MESH_LABELS = {
    "QingshanInn_Terrain_Ground": {"min_scale": [40.0, 25.0, 0.2]},
    "QingshanInn_Terrain_MainRoad": {"min_scale": [4.0, 20.0, 0.05]},
    "QingshanInn_Terrain_Plaza": {"min_scale": [10.0, 7.0, 0.05]},
    "QingshanInn_Bound_North": {"min_scale": [40.0, 0.5, 0.8]},
    "QingshanInn_Bound_South": {"min_scale": [40.0, 0.5, 0.8]},
    "QingshanInn_Bound_East": {"min_scale": [0.5, 25.0, 0.8]},
    "QingshanInn_Bound_West": {"min_scale": [0.5, 25.0, 0.8]},
    "QingshanInn_DungeonGate": {"min_scale": [0.8, 3.0, 1.2]},
}

LIGHT_LABELS = {
    "QingshanInn_KeySun": "DirectionalLight",
    "QingshanInn_SkyLight": "SkyLight",
    "QingshanInn_SkyAtmosphere": "SkyAtmosphere",
    "QingshanInn_MorningFog": "ExponentialHeightFog",
}

INTERACTION_LABELS = {
    "QingshanInn_TownExit": "GameXXKTownExitActor",
}

REQUIRED_LABELS = [
    *STATIC_MESH_LABELS.keys(),
    *LIGHT_LABELS.keys(),
    *INTERACTION_LABELS.keys(),
    "PlayerStart_QingshanInn",
]


def _get_level_subsystem():
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if subsystem is None:
        raise RuntimeError("LevelEditorSubsystem is unavailable")
    return subsystem


def _load_map() -> bool:
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        return False
    return bool(_get_level_subsystem().load_level(MAP_PATH))


def _find_actor_by_label(label: str):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        try:
            if actor.get_actor_label() == label:
                return actor
        except Exception:
            continue
    return None


def _class_chain_contains(actor, expected_class_name: str) -> bool:
    try:
        actor_class = actor.get_class()
        while actor_class is not None:
            if actor_class.get_name() == expected_class_name:
                return True
            actor_class = actor_class.get_super_class()
    except Exception:
        return False
    return False


def _get_component(actor, class_path: str):
    component_class = unreal.load_class(None, class_path)
    if component_class is None:
        return None
    try:
        return actor.get_component_by_class(component_class)
    except Exception:
        return None


def _get_prop(obj, prop_names, default=None):
    if isinstance(prop_names, str):
        prop_names = [prop_names]
    for prop_name in prop_names:
        try:
            return obj.get_editor_property(prop_name)
        except Exception:
            continue
    return default


def _object_identity(value) -> str:
    if value is None:
        return ""
    for function_name in ("get_path_name", "get_name"):
        function = getattr(value, function_name, None)
        if function is None:
            continue
        try:
            return function()
        except Exception:
            continue
    return str(value)


def _scale_meets_minimum(actor, minimum: list[float]) -> bool:
    scale = actor.get_actor_scale3d()
    return scale.x >= minimum[0] and scale.y >= minimum[1] and scale.z >= minimum[2]


def _world_game_mode_matches() -> dict[str, str | bool]:
    game_mode_class = unreal.load_class(None, EXPECTED_GAME_MODE)
    world = unreal.EditorLevelLibrary.get_editor_world()
    world_settings = world.get_world_settings() if world else None
    current = _get_prop(world_settings, ["default_game_mode", "DefaultGameMode"]) if world_settings else None
    return {
        "ok": _object_identity(current) == _object_identity(game_mode_class),
        "expected": _object_identity(game_mode_class),
        "actual": _object_identity(current),
    }


def main() -> None:
    map_loaded = _load_map()
    missing_labels: list[str] = []
    invalid_actors: list[dict[str, object]] = []

    if map_loaded:
        for label in REQUIRED_LABELS:
            if _find_actor_by_label(label) is None:
                missing_labels.append(label)

        cube_mesh = unreal.load_object(None, "/Engine/BasicShapes/Cube.Cube")
        for label, expectations in STATIC_MESH_LABELS.items():
            actor = _find_actor_by_label(label)
            if actor is None:
                continue
            if not _class_chain_contains(actor, "StaticMeshActor"):
                invalid_actors.append({"label": label, "reason": "wrong_class", "expected": "StaticMeshActor"})
                continue
            component = _get_component(actor, "/Script/Engine.StaticMeshComponent")
            actual_mesh = _get_prop(component, ["static_mesh", "StaticMesh"]) if component else None
            if _object_identity(actual_mesh) != _object_identity(cube_mesh):
                invalid_actors.append({"label": label, "reason": "wrong_mesh", "expected": "/Engine/BasicShapes/Cube"})
            if not _scale_meets_minimum(actor, expectations["min_scale"]):
                scale = actor.get_actor_scale3d()
                invalid_actors.append({
                    "label": label,
                    "reason": "scale_too_small",
                    "expected_min": expectations["min_scale"],
                    "actual": [scale.x, scale.y, scale.z],
                })

        for label, expected_class in LIGHT_LABELS.items():
            actor = _find_actor_by_label(label)
            if actor is not None and not _class_chain_contains(actor, expected_class):
                invalid_actors.append({"label": label, "reason": "wrong_class", "expected": expected_class})

        for label, expected_class in INTERACTION_LABELS.items():
            actor = _find_actor_by_label(label)
            if actor is not None and not _class_chain_contains(actor, expected_class):
                invalid_actors.append({"label": label, "reason": "wrong_class", "expected": expected_class})

        player_start = _find_actor_by_label("PlayerStart_QingshanInn")
        if player_start is not None and not _class_chain_contains(player_start, "PlayerStart"):
            invalid_actors.append({"label": "PlayerStart_QingshanInn", "reason": "wrong_class", "expected": "PlayerStart"})

    game_mode = _world_game_mode_matches() if map_loaded else {"ok": False, "expected": EXPECTED_GAME_MODE, "actual": ""}
    config_errors = [] if game_mode["ok"] else [{"key": "DefaultGameMode", **game_mode}]

    report = {
        "ok": map_loaded and not missing_labels and not invalid_actors and not config_errors,
        "map": MAP_PATH,
        "map_loaded": map_loaded,
        "required_label_count": len(REQUIRED_LABELS),
        "missing_labels": missing_labels,
        "invalid_actors": invalid_actors,
        "config_errors": config_errors,
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
