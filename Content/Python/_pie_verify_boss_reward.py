"""Transient playtest helper: after a boss win, verify the boss-card reward evidence."""

import json

import unreal


def _world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


def _call(obj, name, *args):
    fn = getattr(obj, name, None)
    if fn is None or not callable(fn):
        return None
    try:
        return fn(*args)
    except Exception as exc:
        return f"ERR:{exc}"


def _prop(value, name):
    try:
        return value.get_editor_property(name)
    except Exception:
        return None


def _subsystem(world):
    gi = _call(world, "get_game_instance")
    if gi is None:
        try:
            gi = unreal.GameplayStatics.get_game_instance(world)
        except Exception:
            gi = None
    subsystem_type = getattr(unreal, "GameXXKMVPSubsystem", None)
    if gi and subsystem_type:
        sub = _call(gi, "get_subsystem", subsystem_type)
        if sub and not isinstance(sub, str):
            return sub
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    for getter in ("get_battle_board_widget_for_test", "get_route_map_widget_for_test", "get_town_overlay_widget_for_test"):
        widget = _call(controller, getter)
        sub = _call(widget, "get_mvp_subsystem") if widget and not isinstance(widget, str) else None
        if sub and not isinstance(sub, str):
            return sub
    return None


def main():
    out = {"ok": False}
    world = _world()
    if not world:
        print(json.dumps({"ok": False, "reason": "no_pie_world"}, ensure_ascii=False))
        return
    sub = _subsystem(world)
    if not sub:
        print(json.dumps({"ok": False, "reason": "no_subsystem"}, ensure_ascii=False))
        return
    state = _call(sub, "get_runtime_state_copy")
    if state is None or isinstance(state, str):
        out["state"] = "unavailable"
        print(json.dumps(out, ensure_ascii=False, indent=2))
        return
    run = _prop(state, "card_run")
    slots = _prop(run, "boss_card_slots") if run else None
    out["boss_card_slots"] = [str(v) for v in (slots or [])]
    active = _prop(run, "active_battle") if run else None
    deck = _prop(active, "deck") if active else None
    hand = _prop(deck, "hand") if deck else None
    out["hand_card_ids"] = []
    if hand is not None:
        for item in hand:
            try:
                out["hand_card_ids"].append(str(item.get_editor_property("card_id")))
            except Exception:
                pass
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    board = _call(controller, "get_battle_board_widget_for_test")
    if board and not isinstance(board, str):
        _call(board, "refresh_from_state")
        out["has_pending_route_reward"] = bool(_call(board, "has_pending_route_reward"))
        pending = _call(board, "get_pending_route_reward_card_ids")
        out["pending_reward_card_ids"] = [str(v) for v in (pending or [])]
    out["boss_card_in_hand"] = sorted(set(out.get("boss_card_slots") or []) & set(out.get("hand_card_ids") or []))
    out["ok"] = bool(out.get("boss_card_slots")) and bool(out.get("boss_card_in_hand"))
    print(json.dumps(out, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
