from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
MAP_PATH = "/Game/GameXXK/Maps/L_QingshanInn"
HERO_BP_CLASS = "/Game/GameXXK/Characters/Hero/BP_HeroCharacter.BP_HeroCharacter_C"
HERO_NATIVE_CLASS = "/Script/GameXXK.GameXXKHeroCharacter"
GAME_MODE_CLASS = "/Script/GameXXK.GameXXKMVPGameMode"
NPC_CLASS = "/Script/GameXXK.GameXXKTownNpcActor"
EXIT_CLASS = "/Script/GameXXK.GameXXKTownExitActor"
PAPER_FLIPBOOK_COMPONENT_CLASS = "/Script/Paper2D.PaperFlipbookComponent"
QINGSHAN_REGION = "Region.Qingshan"
DEFAULT_SAVE_SLOT = "GameXXK_MVP_Autosave.sav"

REPORT_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-mvp-pie-acceptance-latest.json"
STATE_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-mvp-pie-acceptance-state.json"
SAVE_PATH = PROJECT_ROOT / "Saved" / "SaveGames" / DEFAULT_SAVE_SLOT
SAVE_BACKUP_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / f"{DEFAULT_SAVE_SLOT}.bak"


def _identity(value) -> str:
    if value is None:
        return ""
    for name in ("get_path_name", "get_name"):
        function = getattr(value, name, None)
        if function is None:
            continue
        try:
            return str(function())
        except Exception:
            pass
    return str(value)


def _class_name(value) -> str:
    try:
        return value.get_class().get_name()
    except Exception:
        return ""


def _class_path(value) -> str:
    try:
        return value.get_class().get_path_name()
    except Exception:
        return ""


def _vector(value) -> list[float]:
    return [round(float(value.x), 3), round(float(value.y), 3), round(float(value.z), 3)]


def _xy_distance(a: list[float], b: list[float]) -> float:
    dx = float(a[0]) - float(b[0])
    dy = float(a[1]) - float(b[1])
    return round((dx * dx + dy * dy) ** 0.5, 3)


def _call(obj, names: list[str], *args):
    for name in names:
        function = getattr(obj, name, None)
        if function is None:
            continue
        try:
            return function(*args)
        except Exception:
            continue
    return None


def _load_class(path: str):
    return unreal.load_class(None, path)


def _get_level_subsystem():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def _get_game_world():
    for name in ("get_game_world",):
        function = getattr(unreal.EditorLevelLibrary, name, None)
        if function is not None:
            try:
                world = function()
                if world is not None:
                    return world
            except Exception:
                pass
    try:
        worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
        if worlds:
            return worlds[0]
    except Exception:
        pass
    return None


def _get_editor_world():
    try:
        return unreal.EditorLevelLibrary.get_editor_world()
    except Exception:
        return None


def _load_map() -> bool:
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        return False
    subsystem = _get_level_subsystem()
    if subsystem is not None:
        try:
            return bool(subsystem.load_level(MAP_PATH))
        except Exception:
            pass
    try:
        return bool(unreal.EditorLevelLibrary.load_level(MAP_PATH))
    except Exception:
        return False


def _actors_of_class(world, class_path: str) -> list:
    actor_class = _load_class(class_path)
    if world is None or actor_class is None:
        return []
    try:
        return list(unreal.GameplayStatics.get_all_actors_of_class(world, actor_class))
    except Exception:
        return []


def _find_hero(world):
    try:
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if pawn and _class_path(pawn) == HERO_BP_CLASS:
            return pawn
        hero_native_class = _load_class(HERO_NATIVE_CLASS)
        if pawn and hero_native_class and unreal.MathLibrary.class_is_child_of(pawn.get_class(), hero_native_class):
            return pawn
    except Exception:
        pass

    for class_path in (HERO_BP_CLASS, HERO_NATIVE_CLASS):
        actors = _actors_of_class(world, class_path)
        if actors:
            return actors[0]
    return None


def _player_start_transform(world) -> tuple[unreal.Vector, unreal.Rotator]:
    player_starts = _actors_of_class(world, "/Script/Engine.PlayerStart")
    if player_starts:
        start = player_starts[0]
        return start.get_actor_location(), start.get_actor_rotation()
    return unreal.Vector(0.0, 0.0, 120.0), unreal.Rotator(0.0, 0.0, 0.0)


def _make_transform(location: unreal.Vector, rotation: unreal.Rotator):
    for args in (
        (location, rotation, unreal.Vector(1.0, 1.0, 1.0)),
        (rotation, location, unreal.Vector(1.0, 1.0, 1.0)),
        (),
    ):
        try:
            transform = unreal.Transform(*args)
            if not args:
                try:
                    transform.translation = location
                    transform.rotation = rotation
                    transform.scale3d = unreal.Vector(1.0, 1.0, 1.0)
                except Exception:
                    pass
            return transform
        except Exception:
            continue
    return None


def _try_spawn_hero_with_gameplay_statics(world, hero_class, location, rotation) -> tuple[object | None, list[str]]:
    errors: list[str] = []
    transform = _make_transform(location, rotation)
    if transform is None:
        return None, ["unreal.Transform construction failed"]

    begin_spawn = getattr(unreal.GameplayStatics, "begin_deferred_actor_spawn_from_class", None)
    finish_spawn = getattr(unreal.GameplayStatics, "finish_spawning_actor", None)
    if begin_spawn is None or finish_spawn is None:
        return None, ["GameplayStatics deferred spawn functions are unavailable"]

    call_variants = [(world, hero_class, transform)]
    collision_enum = getattr(unreal, "SpawnActorCollisionHandlingMethod", None)
    if collision_enum is None:
        collision_enum = getattr(unreal, "ESpawnActorCollisionHandlingMethod", None)
    collision_value = None
    if collision_enum is not None:
        for name in ("ALWAYS_SPAWN", "ALWAYS_SPAWN_IGNORE_COLLISIONS"):
            collision_value = getattr(collision_enum, name, None)
            if collision_value is not None:
                break
    if collision_value is not None:
        call_variants.append((world, hero_class, transform, collision_value))
        call_variants.append((world, hero_class, transform, collision_value, None))
    for args in call_variants:
        try:
            pending = begin_spawn(*args)
            if pending is None:
                errors.append(f"begin_deferred_actor_spawn_from_class returned None for {len(args)} args")
                continue
            actor = finish_spawn(pending, transform)
            if actor is not None:
                return actor, errors
            errors.append("finish_spawning_actor returned None")
        except Exception as exc:
            errors.append(f"deferred spawn {len(args)} args failed: {exc}")
    return None, errors


def _try_summon_hero_with_console(world) -> tuple[object | None, list[str]]:
    errors: list[str] = []
    before = {_identity(actor) for actor in _actors_of_class(world, HERO_BP_CLASS)}
    commands = [
        f"summon {HERO_BP_CLASS}",
        "summon BP_HeroCharacter_C",
    ]
    for command in commands:
        try:
            unreal.SystemLibrary.execute_console_command(world, command)
        except Exception as exc:
            errors.append(f"{command} failed: {exc}")
            continue
        for actor in _actors_of_class(world, HERO_BP_CLASS):
            if _identity(actor) not in before:
                return actor, errors
        found = _actors_of_class(world, HERO_BP_CLASS)
        if found:
            return found[0], errors
        errors.append(f"{command} executed but no BP_HeroCharacter actor was found yet")
    return None, errors


def _possess_hero(world, hero) -> dict[str, object]:
    try:
        controller = unreal.GameplayStatics.get_player_controller(world, 0)
    except Exception:
        controller = None
    if controller is None:
        return {"ok": False, "error": "player controller unavailable"}
    try:
        controller.possess(hero)
        return {"ok": True, "controller": _class_path(controller), "pawn": _identity(hero)}
    except Exception as exc:
        return {"ok": False, "controller": _class_path(controller), "error": str(exc)}


def _ensure_hero(world) -> dict[str, object]:
    if world is None:
        return {"ok": False, "error": "PIE world is not active"}
    existing = _find_hero(world)
    if existing is not None:
        possession = _possess_hero(world, existing)
        return {"ok": True, "created": False, "hero": _hero_snapshot(world), "possession": possession}

    hero_class = _load_class(HERO_BP_CLASS)
    if hero_class is None:
        return {"ok": False, "error": f"failed to load {HERO_BP_CLASS}"}

    location, rotation = _player_start_transform(world)
    location.z += 40.0
    actor, spawn_errors = _try_spawn_hero_with_gameplay_statics(world, hero_class, location, rotation)
    if actor is None:
        actor, summon_errors = _try_summon_hero_with_console(world)
        spawn_errors.extend(summon_errors)
    if actor is None:
        return {
            "ok": False,
            "error": "failed to spawn BP_HeroCharacter in PIE world",
            "spawn_errors": spawn_errors,
            "spawn_api": _spawn_api_snapshot(world),
        }

    possession = _possess_hero(world, actor)
    return {
        "ok": possession.get("ok", False),
        "created": True,
        "spawned_actor": _identity(actor),
        "spawned_class": _class_path(actor),
        "spawn_location": _vector(actor.get_actor_location()),
        "possession": possession,
        "hero": _hero_snapshot(world),
        "spawn_errors": spawn_errors,
    }


def _spawn_api_snapshot(world) -> dict[str, object]:
    gameplay_spawn_names = [name for name in dir(unreal.GameplayStatics) if "spawn" in name.lower()]
    world_spawn_names = [name for name in dir(world) if "spawn" in name.lower()] if world is not None else []
    return {
        "gameplay_statics_spawn": gameplay_spawn_names,
        "world_spawn": world_spawn_names,
        "has_spawn_collision_enum": hasattr(unreal, "SpawnActorCollisionHandlingMethod") or hasattr(unreal, "ESpawnActorCollisionHandlingMethod"),
    }


def _get_flipbook_path(hero) -> str:
    flipbook = _call(hero, ["get_current_town_flipbook", "get_default_town_flipbook"])
    if flipbook is not None:
        return _identity(flipbook)
    component_class = _load_class(PAPER_FLIPBOOK_COMPONENT_CLASS)
    if component_class is None:
        return ""
    try:
        component = hero.get_component_by_class(component_class)
        flipbook = component.get_flipbook() if component else None
        return _identity(flipbook)
    except Exception:
        return ""


def _hero_snapshot(world) -> dict[str, object]:
    hero = _find_hero(world)
    if hero is None:
        return {"ok": False, "error": "BP_HeroCharacter was not found in the PIE world"}

    location = hero.get_actor_location()
    visual = _call(hero, ["has_town_visual"])
    assigned = _call(hero, ["has_assigned_town_flipbook"])
    facing = _call(hero, ["get_town_facing_direction"])
    default_path = _call(hero, ["get_default_town_flipbook_path_string"])
    movement = _call(hero, ["get_movement_component"])
    interaction = _call(hero, ["get_interaction_component"])
    focused_actor = _call(interaction, ["get_focused_actor"]) if interaction else None

    return {
        "ok": True,
        "actor_name": hero.get_name(),
        "actor_class": _class_path(hero),
        "native_parent_expected": HERO_NATIVE_CLASS,
        "location": _vector(location),
        "has_visual": bool(visual),
        "has_assigned_flipbook": bool(assigned),
        "current_flipbook": _get_flipbook_path(hero),
        "default_flipbook": str(default_path or ""),
        "facing": str(facing),
        "movement_component": _class_name(movement),
        "interaction_component": _class_name(interaction),
        "focused_actor": _identity(focused_actor),
    }


def _get_game_instance_subsystem(world):
    subsystem_class = _load_class("/Script/GameXXK.GameXXKMVPSubsystem")
    if world is None or subsystem_class is None:
        return None
    contexts = [world]
    try:
        game_instance = world.get_game_instance()
        if game_instance is not None:
            contexts.insert(0, game_instance)
    except Exception:
        game_instance = None
    library = getattr(unreal, "SubsystemBlueprintLibrary", None)
    if library is not None:
        function = getattr(library, "get_game_instance_subsystem", None)
        if function is not None:
            for context in contexts:
                try:
                    subsystem = function(context, subsystem_class)
                    if subsystem is not None:
                        return subsystem
                except Exception:
                    pass
    try:
        function = getattr(game_instance, "get_subsystem", None)
        if function is not None:
            return function(subsystem_class)
    except Exception:
        pass
    return None


def _subsystem_api_snapshot(world) -> dict[str, object]:
    game_instance = None
    try:
        game_instance = world.get_game_instance() if world is not None else None
    except Exception:
        pass
    library = getattr(unreal, "SubsystemBlueprintLibrary", None)
    return {
        "world": _identity(world),
        "game_instance": _identity(game_instance),
        "game_instance_class": _class_path(game_instance) if game_instance else "",
        "game_instance_subsystem_methods": [name for name in dir(game_instance) if "subsystem" in name.lower()] if game_instance else [],
        "subsystem_blueprint_library_methods": [name for name in dir(library) if "subsystem" in name.lower()] if library else [],
        "mvp_subsystem_class": _identity(_load_class("/Script/GameXXK.GameXXKMVPSubsystem")),
        "resolved_subsystem": _identity(_get_game_instance_subsystem(world)),
    }


def _state_snapshot(subsystem) -> dict[str, object]:
    if subsystem is None:
        return {"ok": False, "error": "GameXXKMVPSubsystem unavailable"}
    state = _call(subsystem, ["get_runtime_state_copy"])
    if state is None:
        return {"ok": False, "error": "runtime state unavailable"}
    fields = {}
    for name in ("screen", "quest_state", "current_region", "b_follower_joined", "b_dungeon_active", "player_gold"):
        try:
            fields[name] = str(getattr(state, name))
        except Exception:
            pass
    return {"ok": True, "fields": fields}


def _start_pie() -> dict[str, object]:
    errors: list[str] = []
    existing_world = _get_game_world()
    if existing_world is not None:
        return {"ok": True, "pie_already_active": True, "world": _identity(existing_world)}

    if not _load_map():
        errors.append(f"failed to load map {MAP_PATH}")
    try:
        unreal.EditorLevelLibrary.editor_play_simulate()
    except Exception as exc:
        errors.append(f"editor_play_simulate failed: {exc}")
    return {"ok": not errors, "pie_requested": not errors, "errors": errors, "map": MAP_PATH}


def _stop_pie() -> dict[str, object]:
    try:
        unreal.EditorLevelLibrary.editor_end_play()
        return {"ok": True, "pie_end_requested": True}
    except Exception as exc:
        return {"ok": False, "error": str(exc)}


def _overview(label: str) -> dict[str, object]:
    world = _get_game_world()
    editor_world = _get_editor_world()
    game_mode = None
    controller = None
    hud = None
    if world is not None:
        try:
            game_mode = unreal.GameplayStatics.get_game_mode(world)
            controller = unreal.GameplayStatics.get_player_controller(world, 0)
            hud = controller.get_hud() if controller else None
        except Exception:
            pass

    return {
        "ok": world is not None,
        "label": label,
        "pie_active": world is not None,
        "pie_world": _identity(world),
        "editor_world": _identity(editor_world),
        "expected_game_mode": GAME_MODE_CLASS,
        "game_mode": _class_path(game_mode) if game_mode else "",
        "player_controller": _class_path(controller) if controller else "",
        "hud": _class_path(hud) if hud else "",
        "hero": _hero_snapshot(world) if world else {"ok": False, "error": "PIE world is not active"},
        "npc_count": len(_actors_of_class(world, NPC_CLASS)) if world else 0,
        "town_exit_count": len(_actors_of_class(world, EXIT_CLASS)) if world else 0,
    }


def _write_state(state: dict[str, object]) -> None:
    STATE_PATH.parent.mkdir(parents=True, exist_ok=True)
    STATE_PATH.write_text(json.dumps(state, ensure_ascii=False, indent=2), encoding="utf-8")


def _read_state() -> dict[str, object]:
    if not STATE_PATH.exists():
        return {}
    try:
        return json.loads(STATE_PATH.read_text(encoding="utf-8"))
    except Exception:
        return {}


def _move(horizontal: float, vertical: float, expected_direction: str) -> dict[str, object]:
    world = _get_game_world()
    ensure = _ensure_hero(world)
    hero = _find_hero(world)
    if hero is None:
        return {"ok": False, "error": "hero unavailable for movement", "ensure": ensure}

    before = _hero_snapshot(world)
    _call(hero, ["move_horizontal"], float(horizontal))
    _call(hero, ["move_vertical"], float(vertical))
    after_input = _hero_snapshot(world)
    state = {
        "before": before,
        "after_input": after_input,
        "horizontal": float(horizontal),
        "vertical": float(vertical),
        "expected_direction": expected_direction,
    }
    _write_state(state)
    return {"ok": True, "ensure": ensure, "movement_pending": state}


def _assert_moved(min_distance: float) -> dict[str, object]:
    world = _get_game_world()
    ensure = _ensure_hero(world)
    hero = _find_hero(world)
    pending = _read_state()
    if hero is None:
        return {"ok": False, "error": "hero unavailable for movement assertion", "ensure": ensure, "pending": pending}

    after_tick = _hero_snapshot(world)
    if hero is not None:
        _call(hero, ["move_horizontal"], 0.0)
        _call(hero, ["move_vertical"], 0.0)

    before_location = pending.get("before", {}).get("location", [])
    after_location = after_tick.get("location", [])
    distance = _xy_distance(before_location, after_location) if len(before_location) == 3 and len(after_location) == 3 else 0.0
    expected_direction = str(pending.get("expected_direction", ""))
    facing_text = str(after_tick.get("facing", ""))
    flipbook_text = str(after_tick.get("current_flipbook", ""))
    direction_ok = expected_direction in facing_text or expected_direction in flipbook_text

    return {
        "ok": distance >= float(min_distance) and direction_ok,
        "distance_xy": distance,
        "min_distance_xy": float(min_distance),
        "expected_direction": expected_direction,
        "direction_ok": direction_ok,
        "after_tick": after_tick,
        "ensure": ensure,
        "pending": pending,
    }


def _backup_save() -> dict[str, object]:
    SAVE_BACKUP_PATH.parent.mkdir(parents=True, exist_ok=True)
    if SAVE_PATH.exists():
        shutil.copy2(SAVE_PATH, SAVE_BACKUP_PATH)
        return {"had_save": True, "save": str(SAVE_PATH), "backup": str(SAVE_BACKUP_PATH)}
    if SAVE_BACKUP_PATH.exists():
        SAVE_BACKUP_PATH.unlink()
    return {"had_save": False, "save": str(SAVE_PATH), "backup": str(SAVE_BACKUP_PATH)}


def _restore_save(info: dict[str, object]) -> None:
    SAVE_PATH.parent.mkdir(parents=True, exist_ok=True)
    if info.get("had_save") and SAVE_BACKUP_PATH.exists():
        shutil.copy2(SAVE_BACKUP_PATH, SAVE_PATH)
    elif SAVE_PATH.exists():
        SAVE_PATH.unlink()


def _interact_quest() -> dict[str, object]:
    world = _get_game_world()
    ensure = _ensure_hero(world)
    hero = _find_hero(world)
    if world is None or hero is None:
        return {"ok": False, "error": "PIE world or hero unavailable for interaction", "ensure": ensure, "subsystem_api": _subsystem_api_snapshot(world)}

    subsystem = _get_game_instance_subsystem(world)
    if subsystem is not None:
        _call(subsystem, ["open_world_map"])
        _call(subsystem, ["select_world_region"], unreal.Name(QINGSHAN_REGION))

    quest_npc = None
    for npc in _actors_of_class(world, NPC_CLASS):
        if bool(_call(npc, ["can_offer_quest"])):
            quest_npc = npc
            break
    if quest_npc is None:
        return {"ok": False, "error": "quest NPC unavailable", "npc_count": len(_actors_of_class(world, NPC_CLASS))}

    save_info = _backup_save()
    try:
        interaction = _call(hero, ["get_interaction_component"])
        if interaction is not None:
            _call(interaction, ["set_focused_actor"], quest_npc)
        before_state = _state_snapshot(subsystem)
        focused_before = _identity(_call(interaction, ["get_focused_actor"]) if interaction else None)
        _call(hero, ["interact"])
        after_state = _state_snapshot(subsystem)
        focused_after = _identity(_call(interaction, ["get_focused_actor"]) if interaction else None)
        npc_success = bool(_call(quest_npc, ["was_last_interaction_successful"]))
        follower_active = bool(_call(quest_npc, ["is_follower_active"]))
        follow_target = _identity(_call(quest_npc, ["get_follow_target"]))
        return {
            "ok": npc_success and follower_active,
            "ensure": ensure,
            "quest_npc": _identity(quest_npc),
            "focused_before": focused_before,
            "focused_after": focused_after,
            "npc_success": npc_success,
            "follower_active": follower_active,
            "follow_target": follow_target,
            "before_state": before_state,
            "after_state": after_state,
            "subsystem_api": _subsystem_api_snapshot(world),
            "save_restored": True,
        }
    finally:
        _restore_save(save_info)


def _request_screenshot() -> dict[str, object]:
    world = _get_game_world()
    ensure = _ensure_hero(world)
    hero = _find_hero(world)
    if world is None or hero is None:
        return {"ok": False, "error": "PIE world or hero unavailable for screenshot", "ensure": ensure}
    try:
        location = hero.get_actor_location()
        camera_location = unreal.Vector(location.x - 700.0, location.y - 900.0, location.z + 520.0)
        camera_rotation = unreal.Rotator(-32.0, 38.0, 0.0)
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(camera_location, camera_rotation)
    except Exception:
        pass
    try:
        unreal.SystemLibrary.execute_console_command(world, "HighResShot 1280x720")
        return {
            "ok": True,
            "ensure": ensure,
            "screenshot_requested": True,
            "screenshot_dir": str(PROJECT_ROOT / "Saved" / "Screenshots"),
        }
    except Exception as exc:
        return {"ok": False, "error": str(exc)}


def _emit(action: str, payload: dict[str, object]) -> None:
    report = {"action": action, **payload}
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["start", "snapshot", "ensure-hero", "spawn-api", "subsystem-api", "move", "assert-moved", "interact-quest", "screenshot", "stop"])
    parser.add_argument("--label", default="mvp-pie")
    parser.add_argument("--horizontal", type=float, default=1.0)
    parser.add_argument("--vertical", type=float, default=0.0)
    parser.add_argument("--expected-direction", default="East")
    parser.add_argument("--min-distance", type=float, default=10.0)
    args = parser.parse_args()

    if args.action == "start":
        _emit(args.action, _start_pie())
    elif args.action == "snapshot":
        _emit(args.action, _overview(args.label))
    elif args.action == "ensure-hero":
        _emit(args.action, _ensure_hero(_get_game_world()))
    elif args.action == "spawn-api":
        _emit(args.action, {"ok": True, "spawn_api": _spawn_api_snapshot(_get_game_world())})
    elif args.action == "subsystem-api":
        _emit(args.action, {"ok": True, "subsystem_api": _subsystem_api_snapshot(_get_game_world())})
    elif args.action == "move":
        _emit(args.action, _move(args.horizontal, args.vertical, args.expected_direction))
    elif args.action == "assert-moved":
        _emit(args.action, _assert_moved(args.min_distance))
    elif args.action == "interact-quest":
        _emit(args.action, _interact_quest())
    elif args.action == "screenshot":
        _emit(args.action, _request_screenshot())
    elif args.action == "stop":
        _emit(args.action, _stop_pie())


if __name__ == "__main__":
    main()
