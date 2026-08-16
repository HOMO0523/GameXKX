"""Transient debug probe: dump battle units and deck card ids."""

import json
import sys

import unreal


def _world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


def _subsystem(world):
    game_instance = None
    try:
        game_instance = world.get_game_instance() if world else None
    except Exception:
        game_instance = unreal.GameplayStatics.get_game_instance(world) if world else None
    subsystem = None
    if game_instance:
        try:
            subsystem = game_instance.get_subsystem(unreal.GameXXKMVPSubsystem)
        except Exception:
            subsystem = None
    if subsystem is None:
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if pc:
            try:
                board = pc.get_battle_board_widget_for_test()
                subsystem = board.get_mvp_subsystem() if board else None
            except Exception:
                subsystem = None
    return subsystem


def _property(target, *names):
    for name in names:
        try:
            return getattr(target, name)
        except Exception:
            pass
        try:
            return target.get_editor_property(name)
        except Exception:
            pass
    return None


def main(argv):
    world = _world()
    subsystem = _subsystem(world)
    if not subsystem:
        print(json.dumps({"ok": False, "reason": "subsystem_missing"}))
        return
    state = subsystem.get_runtime_state_copy()
    run = _property(state, "card_run", "CardRun")
    battle = _property(run, "active_battle", "ActiveBattle")

    units = []
    for unit in _property(battle, "units", "Units") or []:
        units.append(
            {
                "unit_id": str(_property(unit, "unit_id", "UnitId")),
                "side": str(_property(unit, "side", "Side")),
                "role": str(_property(unit, "role", "Role")),
                "living": bool(_property(unit, "b_living", "bLiving")),
            }
        )

    deck = _property(battle, "deck", "Deck")
    groups = {}
    for group in ("hand", "draw_pile", "discard_pile"):
        cards = _property(deck, group, group.title().replace("_", "")) or []
        entries = []
        for card in cards:
            entries.append(
                {
                    "card_id": str(_property(card, "card_id", "CardId")),
                    "instance_id": str(_property(card, "instance_id", "InstanceId")),
                }
            )
        groups[group] = entries

    formation_anywhere = [
        c for group in groups.values() for c in group if "FormationMaster" in c["card_id"]
    ]
    print(
        json.dumps(
            {"ok": True, "units": units, "deck": groups, "formation_anywhere": formation_anywhere},
            ensure_ascii=False,
        )
    )


if __name__ == "__main__":
    main(sys.argv[1:])
