"""Read-only probe: compare projected HUD text vs authoritative runtime HP.

Diagnoses a "HP number HUD frozen at a stale value" report while a battle PIE
session is running.  Never mutates state, never plays actions, never saves.
"""

from __future__ import annotations

import json

import unreal


def _call(target, name, *args):
    if target is None:
        return {"ok": False, "error": "target_missing"}
    try:
        return {"ok": True, "value": getattr(target, name)(*args)}
    except Exception as error:
        return {"ok": False, "error": str(error)}


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


def _name(value):
    return str(value or "")


def _safe(value):
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, (list, tuple)):
        return [_safe(item) for item in value]
    try:
        return str(value)
    except Exception:
        return repr(value)


def _hud_text(hud_widget, resource):
    """Read the resource widget's displayed health text through test seams."""
    if hud_widget is None:
        return None
    text = _call(resource, "get_health_display_text_for_test")
    return text.get("value") if text.get("ok") else None


def _board_hud_snapshot(board):
    """Extract every projected unit HUD's displayed text from one board."""
    if board is None:
        return None
    entries = {}
    for unit_id in ["Player", "CompanionInstance.Companion_Blade_03.18B3EF7D",
                    "Npc.ZhouGuangZu", "Enemy.Civet.P1",
                    "Enemy.IronfeatherRooster.P2", "Enemy.Goat.P3"]:
        hud_widget = _call(board, "get_projected_unit_hud_for_test", unreal.Name(unit_id))
        hud = hud_widget.get("value") if hud_widget.get("ok") else None
        resource = _call(hud, "get_resource_widget_for_test")
        resource_widget = resource.get("value") if resource.get("ok") else None
        text = _call(resource_widget, "get_health_display_text_for_test")
        entries[unit_id] = {
            "hud_text": _safe(text.get("value")) if text.get("ok") else None,
            "hud_present": hud is not None,
        }
    return entries


def _enumerate_actor_boards(world):
    """Find every battle-board holder actor and read its owned board."""
    found = []
    actor_classes = []
    for name in ("AGameXXKOneGameIslandRouteMapBridge", "AGameXXKMVPHUD"):
        cls = getattr(unreal, name, None)
        if cls is not None:
            actor_classes.append(cls)
    for cls in actor_classes:
        try:
            actors = unreal.GameplayStatics.get_all_actors_of_class(world, cls)
        except Exception:
            actors = []
        for actor in actors:
            board = None
            try:
                board = getattr(actor, "battle_board_widget")
            except Exception:
                try:
                    board = actor.get_editor_property("battle_board_widget")
                except Exception:
                    board = None
            found.append(
                {
                    "actor_class": str(cls),
                    "actor_name": _name(getattr(actor, "get_actor_label", lambda: "")(),
                                        ) if hasattr(actor, "get_actor_label") else _name(actor),
                    "board_is_valid": board is not None,
                    "board_huds": _board_hud_snapshot(board) if board is not None else None,
                }
            )
    return found


def main():
    result = {"ok": False}
    try:
        editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = editor.get_game_world() if editor else None
        controller = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        board_result = _call(controller, "get_battle_board_widget_for_test")
        board = board_result.get("value") if board_result.get("ok") else None
        try:
            game_instance = world.get_game_instance() if world else None
        except Exception:
            game_instance = unreal.GameplayStatics.get_game_instance(world) if world else None
        subsystem_type = getattr(unreal, "GameXXKMVPSubsystem", None)
        subsystem = None
        if game_instance and subsystem_type:
            try:
                subsystem = game_instance.get_subsystem(subsystem_type)
            except Exception:
                subsystem = None
        if subsystem is None and board:
            subsystem_result = _call(board, "get_mvp_subsystem")
            subsystem = subsystem_result.get("value") if subsystem_result.get("ok") else None
        state_result = _call(subsystem, "get_runtime_state_copy")
        state = state_result.get("value") if state_result.get("ok") else None

        # Authoritative runtime units.
        run = _property(state, "card_run", "CardRun")
        battle = _property(run, "active_battle", "ActiveBattle")
        units = _property(battle, "units", "Units") or []
        runtime_units = []
        for unit in units:
            runtime_units.append(
                {
                    "unit_id": _name(_property(unit, "unit_id", "UnitId")),
                    "side": _name(_property(unit, "side", "Side")),
                    "hp": int(_property(unit, "hp", "HP") or 0),
                    "max_hp": int(_property(unit, "max_hp", "MaxHP") or 0),
                    "living": bool(_property(unit, "b_living", "bLiving")),
                }
            )

        # Board widget displayed state.
        board_calls = {}
        board_calls["displayed_health_player"] = _safe(
            _call(board, "get_displayed_health_for_test", unreal.Name("Player"))
        )
        board_calls["active_session_token"] = _safe(
            _call(board, "get_active_battle_visual_session_token_for_test")
        )
        board_calls["presentation_queue_count"] = _safe(
            _call(board, "get_battle_presentation_queue_count_for_test")
        )
        board_calls["presentation_active"] = _safe(
            _call(board, "is_battle_presentation_active_for_test")
        )
        board_calls["unit_visual_count"] = _safe(
            _call(board, "get_unit_visual_count_for_test")
        )

        # Per-unit HUD text (what the user actually sees).
        hud_entries = {}
        for unit_id in [u["unit_id"] for u in runtime_units]:
            hud_widget = _call(board, "get_projected_unit_hud_for_test", unreal.Name(unit_id))
            hud = hud_widget.get("value") if hud_widget.get("ok") else None
            resource = _call(hud, "get_resource_widget_for_test")
            resource_widget = resource.get("value") if resource.get("ok") else None
            hud_entries[unit_id] = {
                "hud_text": _safe(_hud_text(hud, resource_widget)),
                "hud_present": hud is not None,
            }

        result = {
            "ok": world is not None and controller is not None and board is not None and state is not None,
            "screen": _name(_property(state, "screen", "Screen")),
            "phase": _name(_property(battle, "phase", "Phase")),
            "runtime_units": runtime_units,
            "hud_entries": hud_entries,
            "board_calls": board_calls,
            "controller_board_ptr": _name(board),
            "actor_boards": _enumerate_actor_boards(world),
        }
    except Exception as error:
        result = {"ok": False, "error": str(error)}
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))


if __name__ == "__main__":
    main()
