"""Place the audited Town actors into the playable Asian Village map.

The user positioned the source objects in the vendor demo.  This script never
opens that source map: it applies the frozen transform snapshot below directly
to the project-owned playable map, so it is safe to rerun without copying
editor-only demo state or dynamic material instances.
"""

from __future__ import annotations

import json
import sys

try:
    import unreal
except ModuleNotFoundError:  # Allows the pure migration contract to be unit-tested outside UE.
    unreal = None


TARGET_MAP = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
SOURCE_MAP = "offline-editor-audit-snapshot"
EXPECTED_GAME_MODE = "/Script/GameXXK.GameXXKMVPGameMode"
OWNERSHIP_TAG = "GameXXKAsianVillageTownPlacement"

QUEST_NPC = {
    "label": "QingshanTown_QuestNpc",
    "class_path": "/Game/GameXXK/Characters/Follower/BP_NpcCharacter.BP_NpcCharacter_C",
    "npc_role": "Quest",
    "location": (20410.0, 4580.0, 1558.0),
    "yaw": 0.0,
}
MERCHANT_NPC = {
    "label": "QingshanTown_MerchantNpc",
    "class_path": "/Game/GameXXK/Characters/Merchant/BP_MerchantCharacter.BP_MerchantCharacter_C",
    "npc_role": "Merchant",
    "location": (20360.0, 5910.0, 1598.0),
    "yaw": 0.0,
}
GATE_VISUAL = {
    "label": "QingshanTown_GateVisual",
    "class_path": "/Script/Engine.StaticMeshActor",
    "mesh": "/Game/Asian_Village/meshes/props/SM_statue_01.SM_statue_01",
    "location": (19930.0, 5240.0, 1610.0),
    "yaw": 80.000061,
}
GATE_PROXY = {
    "label": "QingshanInn_TownExit",
    "class_path": "/Script/GameXXK.GameXXKTownExitActor",
    "location": GATE_VISUAL["location"],
    "yaw": GATE_VISUAL["yaw"],
}
PLAYER_START = {
    "label": "PlayerStart_QingshanAsianVillage",
    "class_path": "/Script/Engine.PlayerStart",
    "location": (20170.0, 1700.0, 1592.000732),
    "yaw": 0.0,
}
STOCK_TARGET_PLAYER_START = {
    "label": "Player Start",
    "location": (15740.0, 5240.0, 1162.000732421875),
}


def required_actor_specs():
    return [QUEST_NPC, MERCHANT_NPC, GATE_VISUAL, GATE_PROXY, PLAYER_START]


def gate_orientation_specs():
    """The only actors a live gate-orientation repair is allowed to touch."""
    return [GATE_VISUAL, GATE_PROXY]


def class_paths_match(actual_path, expected_path):
    return str(actual_path).strip() == str(expected_path).strip()


def is_known_stock_player_start(label, location):
    if str(label) != STOCK_TARGET_PLAYER_START["label"]:
        return False
    if hasattr(location, "x"):
        coordinates = (float(location.x), float(location.y), float(location.z))
    else:
        coordinates = tuple(float(value) for value in location)
    return all(
        abs(actual - expected) <= 0.01
        for actual, expected in zip(coordinates, STOCK_TARGET_PLAYER_START["location"])
    )


def _require_unreal():
    if unreal is None:
        raise RuntimeError("This migration must run inside the Unreal Editor Python environment")


def _vector(values):
    return unreal.Vector(float(values[0]), float(values[1]), float(values[2]))


def _rotator(yaw):
    return unreal.Rotator(pitch=0.0, yaw=float(yaw), roll=0.0)


def _current_map_path():
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        return ""
    outer = world.get_outermost()
    return "" if outer is None else str(outer.get_name()).split(".", 1)[0]


def _load_target_map():
    if _current_map_path() == TARGET_MAP:
        return
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if level_subsystem is None or not level_subsystem.load_level(TARGET_MAP):
        raise RuntimeError(f"could not load target Town map: {TARGET_MAP}")
    if _current_map_path() != TARGET_MAP:
        raise RuntimeError(f"target Town map did not become current: {_current_map_path()}")


def _all_actors():
    return list(unreal.EditorLevelLibrary.get_all_level_actors())


def _find_by_label(label):
    return [actor for actor in _all_actors() if str(actor.get_actor_label()) == label]


def _load_class(class_path):
    actor_class = unreal.load_class(None, class_path)
    if actor_class is None:
        raise RuntimeError(f"could not load actor class: {class_path}")
    return actor_class


def _same_class(actor, expected_class):
    try:
        if class_paths_match(actor.get_class().get_path_name(), expected_class.get_path_name()):
            return True
    except Exception:
        pass
    try:
        return bool(actor.get_class().is_child_of(expected_class))
    except Exception:
        return False


def _ensure_owned_tags(actor, role_tag):
    current = []
    try:
        current = [str(tag) for tag in actor.get_editor_property("tags")]
    except Exception:
        pass
    values = list(dict.fromkeys([*current, OWNERSHIP_TAG, role_tag]))
    actor.set_editor_property("tags", [unreal.Name(value) for value in values])


def _move_exact(actor, location, yaw):
    actor.set_actor_location(_vector(location), False, False)
    actor.set_actor_rotation(_rotator(yaw), False)
    actor.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))


def _ensure_labeled_actor(spec, role_tag):
    matches = _find_by_label(spec["label"])
    expected_class = _load_class(spec["class_path"])
    if len(matches) > 1:
        raise RuntimeError(f"ambiguous target actor label {spec['label']}: {len(matches)} matches")
    if matches:
        actor = matches[0]
        if not _same_class(actor, expected_class):
            raise RuntimeError(
                f"target actor {spec['label']} has unexpected class {actor.get_class().get_path_name()}")
        created = False
    else:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            expected_class, _vector(spec["location"]), _rotator(spec["yaw"]))
        if actor is None:
            raise RuntimeError(f"failed to spawn target actor {spec['label']}")
        actor.set_actor_label(spec["label"])
        created = True
    _move_exact(actor, spec["location"], spec["yaw"])
    _ensure_owned_tags(actor, role_tag)
    return actor, created


def _town_npc_role(role_name):
    enum_type = getattr(unreal, "GameXXKTownNpcRole", None) or getattr(unreal, "EGameXXKTownNpcRole", None)
    if enum_type is None:
        raise RuntimeError("GameXXKTownNpcRole enum is unavailable; build GameXXKEditor first")
    for candidate in (role_name, role_name.upper(), role_name.capitalize()):
        if hasattr(enum_type, candidate):
            return getattr(enum_type, candidate)
    raise RuntimeError(f"GameXXKTownNpcRole has no {role_name} value")


def _configure_npc(spec):
    actor, created = _ensure_labeled_actor(spec, f"TownRole_{spec['npc_role']}")
    role = _town_npc_role(spec["npc_role"])
    try:
        actor.set_npc_role(role)
    except Exception:
        actor.set_editor_property("npc_role", role)
    return {"label": spec["label"], "created": created, "class": actor.get_class().get_path_name()}


def _configure_gate_visual():
    actor, created = _ensure_labeled_actor(GATE_VISUAL, "TownRole_GateVisual")
    mesh = unreal.EditorAssetLibrary.load_asset(GATE_VISUAL["mesh"])
    if mesh is None:
        raise RuntimeError(f"could not load gate visual mesh: {GATE_VISUAL['mesh']}")
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if component is None:
        raise RuntimeError("gate visual StaticMeshActor lacks a StaticMeshComponent")
    component.set_static_mesh(mesh)
    return {"label": GATE_VISUAL["label"], "created": created, "mesh": mesh.get_path_name()}


def _configure_gate_proxy():
    actor, created = _ensure_labeled_actor(GATE_PROXY, "TownRole_GateInteraction")
    return {"label": GATE_PROXY["label"], "created": created, "class": actor.get_class().get_path_name()}


def _repair_gate_orientation_only():
    """Correct historic Pitch/Yaw corruption without changing any placed transforms."""
    _require_unreal()
    _load_target_map()
    report = {"map": TARGET_MAP, "actors": []}
    for spec in gate_orientation_specs():
        matches = _find_by_label(spec["label"])
        if len(matches) != 1:
            raise RuntimeError(f"expected exactly one gate actor {spec['label']}, found {len(matches)}")
        actor = matches[0]
        location_before = actor.get_actor_location()
        actor.set_actor_rotation(_rotator(spec["yaw"]), False)
        location_after = actor.get_actor_location()
        if not location_after.equals(location_before):
            raise RuntimeError(f"gate orientation repair moved {spec['label']}; refusing to save")
        rotation = actor.get_actor_rotation()
        if abs(float(rotation.pitch)) > 0.01 or abs(float(rotation.yaw) - float(spec["yaw"])) > 0.01:
            raise RuntimeError(f"gate orientation repair did not apply yaw for {spec['label']}")
        report["actors"].append({
            "label": spec["label"],
            "location": [float(location_after.x), float(location_after.y), float(location_after.z)],
            "yaw": float(rotation.yaw),
        })
    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError(f"failed to save gate orientation repair for {TARGET_MAP}")
    report["ok"] = True
    return report


def _find_or_create_player_start():
    matches = _find_by_label(PLAYER_START["label"])
    expected_class = _load_class(PLAYER_START["class_path"])
    if len(matches) > 1:
        raise RuntimeError(f"ambiguous target PlayerStart label: {len(matches)} matches")
    if matches:
        actor = matches[0]
        if not _same_class(actor, expected_class):
            raise RuntimeError(f"{PLAYER_START['label']} is not a PlayerStart")
        created = False
    else:
        starts = [actor for actor in _all_actors() if _same_class(actor, expected_class)]
        if len(starts) > 1:
            raise RuntimeError("target map has multiple unlabeled PlayerStart actors; refusing to choose one")
        if starts:
            actor = starts[0]
            actor.set_actor_label(PLAYER_START["label"])
            created = False
        else:
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
                expected_class, _vector(PLAYER_START["location"]), _rotator(PLAYER_START["yaw"]))
            if actor is None:
                raise RuntimeError("failed to create target PlayerStart")
            actor.set_actor_label(PLAYER_START["label"])
            created = True
    _move_exact(actor, PLAYER_START["location"], PLAYER_START["yaw"])
    _ensure_owned_tags(actor, "TownRole_PlayerStart")
    return {"label": PLAYER_START["label"], "created": created, "class": actor.get_class().get_path_name()}


def _remove_known_stock_player_start():
    expected_class = _load_class(PLAYER_START["class_path"])
    matches = [
        actor
        for actor in _all_actors()
        if _same_class(actor, expected_class)
        and is_known_stock_player_start(actor.get_actor_label(), actor.get_actor_location())
    ]
    if len(matches) > 1:
        raise RuntimeError(f"ambiguous stock PlayerStart cleanup: {len(matches)} matches")
    if not matches:
        return False
    if not unreal.EditorLevelLibrary.destroy_actor(matches[0]):
        raise RuntimeError("could not remove the known stock PlayerStart")
    return True


def _configure_game_mode():
    world = unreal.EditorLevelLibrary.get_editor_world()
    world_settings = world.get_world_settings() if world else None
    if world_settings is None:
        raise RuntimeError("target Town map has no WorldSettings")
    game_mode = _load_class(EXPECTED_GAME_MODE)
    world_settings.set_editor_property("default_game_mode", game_mode)
    return game_mode.get_path_name()


def main():
    _require_unreal()
    if "--repair-gate-orientation" in sys.argv[1:]:
        print(json.dumps(_repair_gate_orientation_only(), ensure_ascii=False, sort_keys=True))
        return
    _load_target_map()
    player_start = _find_or_create_player_start()
    report = {
        "map": TARGET_MAP,
        "source": SOURCE_MAP,
        "quest_npc": _configure_npc(QUEST_NPC),
        "merchant_npc": _configure_npc(MERCHANT_NPC),
        "gate_visual": _configure_gate_visual(),
        "gate_proxy": _configure_gate_proxy(),
        "player_start": player_start,
        "removed_known_stock_player_start": _remove_known_stock_player_start(),
        "default_game_mode": _configure_game_mode(),
    }
    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError(f"failed to save target Town map: {TARGET_MAP}")
    report["ok"] = True
    print(json.dumps(report, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
