from __future__ import annotations

import argparse
import json

import unreal


HERO_CLASS_PATH = "/Game/GameXXK/Characters/Hero/BP_HeroCharacter.BP_HeroCharacter_C"


def _object_path(value) -> str:
    if value is None:
        return ""
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def _vector_to_dict(value) -> dict[str, float]:
    return {"x": float(value.x), "y": float(value.y), "z": float(value.z)}


def _rotator_to_dict(value) -> dict[str, float]:
    return {"pitch": float(value.pitch), "yaw": float(value.yaw), "roll": float(value.roll)}


def _first_component(actor, component_type):
    if not actor:
        return None
    components = actor.get_components_by_class(component_type)
    return components[0] if components else None


def _summarize_hero(hero) -> dict:
    if not hero:
        return {}
    visual = _first_component(hero, unreal.PaperFlipbookComponent)
    result = {
        "path": _object_path(hero),
        "class": _object_path(hero.get_class()) if hasattr(hero, "get_class") else "",
    }
    for method_name in (
        "get_default_town_flipbook_path_string",
        "get_current_town_flipbook",
        "get_town_facing_direction",
        "is_town_moving",
    ):
        try:
            value = getattr(hero, method_name)()
            if hasattr(value, "get_path_name"):
                value = _object_path(value)
            else:
                value = str(value)
            result[method_name] = value
        except Exception as exc:
            result[method_name] = {"error": str(exc)}
    if visual:
        result["visual"] = {
            "name": visual.get_name(),
            "path": _object_path(visual),
            "relative_location": _vector_to_dict(visual.get_editor_property("relative_location")),
            "relative_rotation": _rotator_to_dict(visual.get_editor_property("relative_rotation")),
            "relative_scale": _vector_to_dict(visual.get_editor_property("relative_scale3d")),
            "flipbook": _object_path(visual.get_flipbook()),
        }
    return result


def _pie_world():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    try:
        return subsystem.get_game_world()
    except Exception:
        return None


def _player_pawn(world):
    if not world:
        return None
    try:
        return unreal.GameplayStatics.get_player_pawn(world, 0)
    except Exception:
        return None


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--json-only", action="store_true")
    args = parser.parse_args()

    hero_class = unreal.load_class(None, HERO_CLASS_PATH)
    cdo = unreal.get_default_object(hero_class) if hero_class else None
    world = _pie_world()
    pawn = _player_pawn(world)
    report = {
        "ok": hero_class is not None,
        "hero_class": HERO_CLASS_PATH,
        "bp_cdo": _summarize_hero(cdo),
        "pie_pawn": _summarize_hero(pawn),
    }
    print(json.dumps(report, ensure_ascii=False, indent=None if args.json_only else 2, sort_keys=True))


if __name__ == "__main__":
    main()
