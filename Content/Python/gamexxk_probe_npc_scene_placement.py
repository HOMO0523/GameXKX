from __future__ import annotations

import json

import unreal


def _object_path(value):
    if value is None:
        return ""
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def _class_path(value):
    if value is None:
        return ""
    try:
        return value.get_class().get_path_name()
    except Exception:
        return ""


def _enum_name(value):
    if value is None:
        return ""
    try:
        return value.name
    except Exception:
        return str(value)


def _vector_to_dict(value):
    if value is None:
        return None
    return {
        "x": float(getattr(value, "x", 0.0)),
        "y": float(getattr(value, "y", 0.0)),
        "z": float(getattr(value, "z", 0.0)),
    }


def _editor_world():
    try:
        subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        return subsystem.get_editor_world() if subsystem else None
    except Exception:
        return None


def _game_world():
    try:
        subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        return subsystem.get_game_world() if subsystem else None
    except Exception:
        return None


def _actors(world):
    if not world:
        return []
    try:
        return unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    except Exception:
        return []


def _label(actor):
    try:
        return actor.get_actor_label()
    except Exception:
        return actor.get_name()


def _visual_summary(actor):
    visual_character = None
    try:
        visual_character = actor.get_visual_character()
    except Exception:
        pass

    visual_component = None
    if visual_character:
        try:
            components = visual_character.get_components_by_class(unreal.PaperFlipbookComponent)
            visual_component = components[0] if components else None
        except Exception:
            pass

    return {
        "visual_character": {
            "name": visual_character.get_name() if visual_character else "",
            "class": _class_path(visual_character),
            "path": _object_path(visual_character),
        },
        "visual_flipbook": _object_path(visual_component.get_flipbook()) if visual_component else "",
    }


def _character_body_summary(actor):
    visual_component = None
    try:
        components = actor.get_components_by_class(unreal.PaperFlipbookComponent)
        visual_component = components[0] if components else None
    except Exception:
        pass
    result = {
        "body_flipbook": _object_path(visual_component.get_flipbook()) if visual_component else "",
    }
    for method_name in ("get_npc_role", "was_last_interaction_successful", "is_follower_active"):
        try:
            value = getattr(actor, method_name)()
            result[method_name] = _enum_name(value) if method_name == "get_npc_role" else value
        except Exception:
            pass
    try:
        result["follow_target"] = _object_path(actor.get_follow_target())
    except Exception:
        pass
    return result


def _interesting_actor(actor):
    class_text = _class_path(actor)
    label = _label(actor)
    return any(
        token in class_text or token in label
        for token in (
            "GameXXKTownNpcActor",
            "GameXXKTownNpcCharacter",
            "BP_MerchantCharacter",
            "BP_NpcCharacter",
            "Merchant",
            "Npc",
            "Quest",
        )
    )


def _world_summary(world):
    result = []
    for actor in _actors(world):
        if not _interesting_actor(actor):
            continue
        item = {
            "name": actor.get_name(),
            "label": _label(actor),
            "class": _class_path(actor),
            "location": _vector_to_dict(actor.get_actor_location()),
            "owner": _object_path(actor.get_owner()),
        }
        if hasattr(actor, "get_npc_role"):
            item.update(_character_body_summary(actor))
        if actor.get_class().get_name() == "GameXXKTownNpcActor":
            try:
                item["role"] = _enum_name(actor.get_npc_role())
            except Exception:
                item["role"] = ""
            try:
                item["visual_character_class"] = _object_path(actor.get_visual_character_class())
            except Exception:
                item["visual_character_class"] = ""
            item.update(_visual_summary(actor))
        result.append(item)
    return result


def main():
    editor_world = _editor_world()
    game_world = _game_world()
    result = {
        "editor_world": _object_path(editor_world),
        "game_world": _object_path(game_world),
        "editor_actors": _world_summary(editor_world),
        "game_actors": _world_summary(game_world),
    }
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
