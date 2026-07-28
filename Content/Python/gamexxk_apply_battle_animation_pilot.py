"""Apply the Hero idle pilot only to the live battle PIE actor."""

from __future__ import annotations

import json

try:
    import unreal
except ImportError:  # Allows source contract tests outside the editor.
    unreal = None


FLIPBOOK_PATH = "/Game/GameXXK/Characters/BattleAnimationPilot/Hero/Idle/Flipbooks/FB_Pilot_Hero_Idle"


def _pie_world():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = subsystem.get_game_world()
    if world is None:
        raise RuntimeError("battle animation pilot requires a running PIE world")
    return world


def apply_pilot() -> dict:
    if unreal is None:
        raise RuntimeError("battle animation pilot application requires UE editor Python")
    world = _pie_world()
    flipbook = unreal.load_asset(FLIPBOOK_PATH)
    if flipbook is None:
        raise RuntimeError(f"pilot flipbook is missing: {FLIPBOOK_PATH}")

    hero = None
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if "BattleSceneUnitActor" not in actor.get_class().get_name():
            continue
        if bool(actor.is_enemy_unit()):
            continue
        if str(actor.get_unit_id()) not in ("Hero", "Player"):
            continue
        hero = actor
        break
    if hero is None:
        raise RuntimeError("live battle Hero actor was not found")

    visual = hero.get_battle_visual_component()
    if visual is None:
        raise RuntimeError("live battle Hero has no PaperFlipbookComponent")
    visual.set_flipbook(flipbook)
    visual.set_looping(True)
    visual.set_play_rate(1.0)
    visual.play_from_start()
    return {
        "ok": True,
        "battle_only": True,
        "actor": hero.get_path_name(),
        "unit_id": str(hero.get_unit_id()),
        "is_enemy": bool(hero.is_enemy_unit()),
        "flipbook": flipbook.get_path_name(),
        "fps": float(flipbook.get_editor_property("frames_per_second")),
        "frame_count": len(flipbook.get_editor_property("key_frames")),
        "looping": bool(visual.is_looping()),
        "playing": bool(visual.is_playing()),
        "playback_position": float(visual.get_playback_position()),
    }


def main() -> None:
    print(json.dumps(apply_pilot(), ensure_ascii=False))


if __name__ == "__main__":
    main()
