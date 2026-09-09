"""Transient playtest helper: drive the public flow to the route map so the human can play."""

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
    controller = _call(world, "get_player_controller", 0) if False else unreal.GameplayStatics.get_player_controller(world, 0)
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
    steps = []
    steps.append(("start_new_game", bool(_call(sub, "start_new_game"))))
    steps.append(("accept_quest", bool(_call(sub, "accept_quest"))))
    steps.append(("open_dungeon_from_town_exit", bool(_call(sub, "open_dungeon_from_town_exit"))))
    steps.append(("select_route_node_by_id_start", bool(_call(sub, "select_route_node_by_id", 0))))
    out["steps"] = steps
    out["ok"] = all(isinstance(v, bool) and v for _, v in steps)
    print(json.dumps(out, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
