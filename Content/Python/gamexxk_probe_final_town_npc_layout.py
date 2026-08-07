"""Read-only probe for the final named-NPC installation in L_QingshanInn."""

from __future__ import annotations

import argparse
import json

import unreal


MAP = "/Game/GameXXK/Maps/L_QingshanInn"
NPC_LAYOUT = (
    ("Npc.TusiChief", 480.0, -660.0),
    ("Npc.SongJinBao", 430.0, 670.0),
    ("Npc.YueBai", -450.0, 700.0),
    ("Npc.ZhouGuangZu", -820.0, 100.0),
    ("Npc.JinGui", -450.0, -700.0),
    ("Npc.QiongMeiEr", 820.0, 0.0),
)


def _path(value):
    if value is None:
        return ""
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def _location(actor):
    value = actor.get_actor_location()
    return {"x": float(value.x), "y": float(value.y), "z": float(value.z)}


def _actor_summary(actor):
    label = actor.get_actor_label()
    class_path = _path(actor.get_class())
    meshes = []
    flipbooks = []
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.get_editor_property("static_mesh")
        if mesh:
            meshes.append(_path(mesh))
    for component in actor.get_components_by_class(unreal.PaperFlipbookComponent):
        flipbook = component.get_flipbook()
        if flipbook:
            flipbooks.append(_path(flipbook))
    npc_id = ""
    if hasattr(actor, "get_npc_id"):
        try:
            npc_id = str(actor.get_npc_id())
        except Exception:
            pass
    return {
        "label": label,
        "name": actor.get_name(),
        "class": class_path,
        "location": _location(actor),
        "npc_id": npc_id,
        "meshes": meshes,
        "flipbooks": flipbooks,
    }


def _is_relevant(summary):
    haystack = " ".join(
        [summary["label"], summary["name"], summary["class"], summary["npc_id"]]
        + summary["meshes"]
        + summary["flipbooks"]
    ).lower()
    return any(
        token in haystack
        for token in (
            "npc",
            "merchant",
            "quest",
            "statue",
            "portal",
            "teleport",
            "playerstart",
            "townexit",
            "route",
        )
    )


def _ground_hit(world, x, y):
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
    actor = payload.get("actor") if isinstance(payload, dict) else None
    return {
        "ok": point is not None,
        "location": None if point is None else {"x": float(point.x), "y": float(point.y), "z": float(point.z)},
        "actor": "" if actor is None else actor.get_actor_label(),
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--map", default=MAP)
    parser.add_argument("--all", action="store_true")
    args = parser.parse_args(argv)
    map_path = str(args.map)
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_subsystem or not level_subsystem.load_level(map_path):
        raise RuntimeError(f"could not load {map_path}")
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors() if actor_subsystem else []
    summaries = [_actor_summary(actor) for actor in actors]
    center_actor = next((actor for actor in actors if actor.get_actor_label() == "QingshanInn_TownExit"), None)
    center = center_actor.get_actor_location() if center_actor else None
    result = {
        "ok": True,
        "map": map_path,
        "actor_count": len(summaries),
        "relevant": [summary for summary in summaries if _is_relevant(summary)],
        "layout_plan": [] if center is None else [
            {
                "npc_id": npc_id,
                "offset": {"x": offset_x, "y": offset_y},
                "ground": _ground_hit(level_subsystem.get_current_level().get_world(), center.x + offset_x, center.y + offset_y),
            }
            for npc_id, offset_x, offset_y in NPC_LAYOUT
        ],
    }
    if args.all:
        result["all_actors"] = summaries
    print(json.dumps(result, ensure_ascii=False))
    return result


if __name__ == "__main__":
    main()
