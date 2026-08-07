"""Install the six final static named NPCs around the Asian Village town portal."""

from __future__ import annotations

import argparse
import json

import unreal


MAP = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
CENTER_LABEL = "QingshanInn_TownExit"
MANAGED_PREFIX = "QingshanTown_Npc_"
RETIRED_LABELS = {"QingshanTown_QuestNpc", "QingshanTown_MerchantNpc"}
CAPSULE_HALF_HEIGHT = 72.0
NPC_LAYOUT = (
    {"npc_id": "Npc.TusiChief", "stem": "TusiChief", "offset": (480.0, -660.0)},
    {"npc_id": "Npc.SongJinBao", "stem": "SongJinBao", "offset": (430.0, 670.0)},
    {"npc_id": "Npc.YueBai", "stem": "YueBai", "offset": (-450.0, 700.0)},
    {"npc_id": "Npc.ZhouGuangZu", "stem": "ZhouGuangZu", "offset": (-820.0, 100.0)},
    {"npc_id": "Npc.JinGui", "stem": "JinGui", "offset": (-450.0, -700.0)},
    {"npc_id": "Npc.QiongMeiEr", "stem": "QiongMeiEr", "offset": (820.0, 0.0)},
)


def _vector_payload(value):
    return {"x": float(value.x), "y": float(value.y), "z": float(value.z)}


def _load_map():
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_subsystem or not level_subsystem.load_level(MAP):
        raise RuntimeError(f"could not load final town map: {MAP}")
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if not actor_subsystem or not editor_subsystem:
        raise RuntimeError("required editor subsystems are unavailable")
    world = editor_subsystem.get_editor_world()
    if world is None:
        raise RuntimeError("final town editor world is unavailable")
    return level_subsystem, actor_subsystem, world


def _ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(
        world,
        unreal.Vector(float(x), float(y), 50000.0),
        unreal.Vector(float(x), float(y), -50000.0),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
        True,
        [],
        unreal.DrawDebugTrace.NONE,
        True,
    )
    payload = hit.to_dict() if hit is not None else {}
    point = payload.get("impact_point") if isinstance(payload, dict) else None
    if point is None:
        raise RuntimeError(f"could not find walkable ground at ({x}, {y})")
    return float(point.z)


def _desired_layout(actors, world):
    center_actor = next((actor for actor in actors if actor.get_actor_label() == CENTER_LABEL), None)
    if center_actor is None:
        raise RuntimeError(f"central portal actor is missing: {CENTER_LABEL}")
    center = center_actor.get_actor_location()
    result = []
    for spec in NPC_LAYOUT:
        x = float(center.x) + float(spec["offset"][0])
        y = float(center.y) + float(spec["offset"][1])
        z = _ground_z(world, x, y) + CAPSULE_HALF_HEIGHT
        result.append(
            {
                **spec,
                "label": f"{MANAGED_PREFIX}{spec['stem']}",
                "location": unreal.Vector(x, y, z),
                "idle_flipbook": (
                    f"/Game/GameXXK/Characters/PartyDeckNPC/{spec['stem']}/Flipbooks/"
                    f"FB_PartyDeckNPC_{spec['stem']}_Idle_South."
                    f"FB_PartyDeckNPC_{spec['stem']}_Idle_South"
                ),
            }
        )
    return center, result


def _report(center, desired, actors, changed=False, removed=None, saved=False):
    by_label = {actor.get_actor_label(): actor for actor in actors}
    installed = []
    for spec in desired:
        actor = by_label.get(spec["label"])
        installed.append(
            {
                "npc_id": spec["npc_id"],
                "label": spec["label"],
                "desired_location": _vector_payload(spec["location"]),
                "idle_flipbook": spec["idle_flipbook"],
                "exists": actor is not None,
                "class": "" if actor is None else actor.get_class().get_path_name(),
                "actual_location": None if actor is None else _vector_payload(actor.get_actor_location()),
                "actual_npc_id": "" if actor is None else str(actor.get_npc_id()),
                "can_join": False if actor is None else bool(actor.can_join_party()),
                "has_primary_action": False if actor is None else bool(actor.has_primary_interaction_action()),
                "actual_idle_flipbook": "" if actor is None else str(actor.get_default_town_flipbook_path_string()),
            }
        )
    return {
        "ok": all(
            entry["exists"]
            and entry["actual_npc_id"] == entry["npc_id"]
            and entry["can_join"]
            and entry["actual_idle_flipbook"] == entry["idle_flipbook"]
            for entry in installed
        ),
        "map": MAP,
        "center": _vector_payload(center),
        "changed": bool(changed),
        "removed": list(removed or []),
        "saved": bool(saved),
        "installed": installed,
    }


def install():
    level_subsystem, actor_subsystem, world = _load_map()
    actors = list(actor_subsystem.get_all_level_actors())
    center, desired = _desired_layout(actors, world)
    desired_labels = {spec["label"] for spec in desired}
    removed = []

    for actor in list(actors):
        label = actor.get_actor_label()
        if label in RETIRED_LABELS or (label.startswith(MANAGED_PREFIX) and label not in desired_labels):
            if actor_subsystem.destroy_actor(actor):
                removed.append(label)

    actors = list(actor_subsystem.get_all_level_actors())
    by_label = {actor.get_actor_label(): actor for actor in actors}
    changed = bool(removed)
    for spec in desired:
        actor = by_label.get(spec["label"])
        if actor is None or not isinstance(actor, unreal.GameXXKTownNpcCharacter):
            if actor is not None:
                if not actor_subsystem.destroy_actor(actor):
                    raise RuntimeError(f"could not replace invalid managed actor: {spec['label']}")
            actor = actor_subsystem.spawn_actor_from_class(
                unreal.GameXXKTownNpcCharacter,
                spec["location"],
                unreal.Rotator(),
                False,
            )
            if actor is None:
                raise RuntimeError(f"could not spawn named NPC: {spec['npc_id']}")
            actor.set_actor_label(spec["label"], True)
            changed = True
        actor.set_actor_location(spec["location"], False, False)
        actor.set_actor_rotation(unreal.Rotator(), False)
        actor.set_npc_id(unreal.Name(spec["npc_id"]))
        try:
            actor.set_folder_path("GameXXK/Town/NPCs")
        except Exception:
            pass

    saved = bool(level_subsystem.save_current_level())
    if not saved:
        raise RuntimeError("could not save final town NPC installation")
    actors = list(actor_subsystem.get_all_level_actors())
    report = _report(center, desired, actors, changed=changed, removed=removed, saved=saved)
    if not report["ok"]:
        raise RuntimeError("final town NPC post-save verification failed: " + json.dumps(report, ensure_ascii=False))
    return report


def inspect():
    _level_subsystem, actor_subsystem, world = _load_map()
    actors = list(actor_subsystem.get_all_level_actors())
    center, desired = _desired_layout(actors, world)
    return _report(center, desired, actors)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--execute", action="store_true")
    args = parser.parse_args(argv)
    result = install() if args.execute else inspect()
    print(json.dumps(result, ensure_ascii=False))
    return result


if __name__ == "__main__":
    main()
