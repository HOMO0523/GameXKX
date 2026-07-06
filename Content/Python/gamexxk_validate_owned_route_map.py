import json

import unreal


ROUTE_MAP = "/Game/GameXXK/Maps/L_RouteMap"


def _path(obj):
    return obj.get_path_name() if obj else ""


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(ROUTE_MAP)
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        raise RuntimeError("No editor world after loading L_RouteMap")

    settings = world.get_world_settings()
    game_mode = settings.get_editor_property("default_game_mode")
    game_mode_path = _path(game_mode)
    if "/Game/1Game/" in game_mode_path:
        raise RuntimeError(f"L_RouteMap uses original 1Game GameMode: {game_mode_path}")
    if "GameXXK" not in game_mode_path:
        raise RuntimeError(f"L_RouteMap does not use a GameXXK GameMode: {game_mode_path}")

    result = {
        "ok": True,
        "route_map": ROUTE_MAP,
        "game_mode": game_mode_path,
    }
    unreal.log("[GameXXK][OwnedRouteMap] " + json.dumps(result, ensure_ascii=False))
    return result


if __name__ == "__main__":
    print(json.dumps(main(), ensure_ascii=False))
