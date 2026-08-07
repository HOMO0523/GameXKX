"""Probe final town NPC visuals and contextual interaction menus in PIE."""

from __future__ import annotations

import json

import unreal


EXPECTED = {
    "Npc.TusiChief": "剧情",
    "Npc.SongJinBao": "商店",
    "Npc.YueBai": "",
    "Npc.ZhouGuangZu": "",
    "Npc.JinGui": "",
    "Npc.QiongMeiEr": "",
}


def _path(value):
    if value is None:
        return ""
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def main():
    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor_subsystem.get_game_world() if editor_subsystem else None
    if world is None:
        raise RuntimeError("PIE world is unavailable")
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    if pawn is None or controller is None:
        raise RuntimeError("PIE player flow is unavailable")
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.GameXXKTownNpcCharacter)
    by_id = {str(actor.get_npc_id()): actor for actor in actors}
    entries = []
    for npc_id, expected_primary in EXPECTED.items():
        actor = by_id.get(npc_id)
        if actor is None:
            entries.append({"npc_id": npc_id, "exists": False})
            continue
        controller.close_quest_dialog()
        opened = bool(controller.open_town_npc_interaction_for_npc(actor, pawn))
        dialog = controller.get_quest_dialog_widget_for_test()
        primary = "" if dialog is None else str(dialog.get_primary_action_label_for_test())
        recruit = "" if dialog is None else str(dialog.get_recruit_action_label_for_test())
        visual = actor.get_town_visual_component()
        flipbook = None if visual is None else visual.get_flipbook()
        recruited = bool(controller.recruit_pending_town_npc()) if opened else False
        entries.append(
            {
                "npc_id": npc_id,
                "exists": True,
                "opened": opened,
                "dialog_open": False if dialog is None else bool(dialog.is_dialog_open()),
                "primary": primary,
                "expected_primary": expected_primary,
                "recruit": recruit,
                "recruited": recruited,
                "can_join": bool(actor.can_join_party()),
                "has_primary_action": bool(actor.has_primary_interaction_action()),
                "follower_active": bool(actor.is_follower_active()),
                "flipbook": _path(flipbook),
            }
        )
    controller.close_quest_dialog()
    ok = len(actors) == len(EXPECTED) and all(
        entry.get("exists")
        and entry.get("opened")
        and entry.get("primary") == entry.get("expected_primary")
        and entry.get("recruit") == "入队"
        and entry.get("recruited")
        and entry.get("can_join")
        and not entry.get("follower_active")
        and "/PartyDeckNPC/" in entry.get("flipbook", "")
        for entry in entries
    )
    result = {"ok": ok, "npc_count": len(actors), "entries": entries}
    print(json.dumps(result, ensure_ascii=False))
    return result


if __name__ == "__main__":
    main()
