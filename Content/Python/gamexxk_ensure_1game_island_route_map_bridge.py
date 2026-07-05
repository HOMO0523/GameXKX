from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
REPORT_PATH = PROJECT_ROOT / "Saved" / "AssetAnalysis" / "1game_island_route_map_bridge.json"

MAP_PATH = "/Game/GameXXK/Maps/L_Battle_1Game"
BRIDGE_LABEL = "GameXXK_1GameIslandRouteMapBridge"
BRIDGE_CLASS = "/Script/GameXXK.GameXXKOneGameIslandRouteMapBridge"
ROUTE_MAP_WIDGET_CLASS = "/Game/1Game/UI/UI_地图选择.UI_地图选择_C"
ROUTE_NODE_WIDGET_CLASS = "/Game/1Game/UI/UI_地图选择-关卡.UI_地图选择-关卡_C"


def _load_map() -> None:
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if subsystem is None:
        raise RuntimeError("LevelEditorSubsystem is unavailable")
    if not subsystem.load_level(MAP_PATH):
        raise RuntimeError(f"Failed to load map: {MAP_PATH}")


def _find_actor_by_label(label: str):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        try:
            if actor.get_actor_label() == label:
                return actor
        except Exception:
            continue
    return None


def _spawn_or_get_bridge():
    actor = _find_actor_by_label(BRIDGE_LABEL)
    created = False
    if actor is None:
        actor_class = unreal.load_class(None, BRIDGE_CLASS)
        if actor_class is None:
            raise RuntimeError(f"Could not load bridge class: {BRIDGE_CLASS}")
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, unreal.Vector(0.0, 0.0, 120.0), unreal.Rotator(0.0, 0.0, 0.0))
        if actor is None:
            raise RuntimeError("Failed to spawn 1Game island route map bridge")
        actor.set_actor_label(BRIDGE_LABEL)
        created = True
    return actor, created


def _soft_class_values(path: str):
    loaded_class = unreal.load_class(None, path)
    values = [loaded_class, path]
    soft_class_path = getattr(unreal, "SoftClassPath", None)
    if soft_class_path is not None:
        try:
            values.insert(0, soft_class_path(path))
        except Exception:
            pass
    return [value for value in values if value is not None]


def _set_property(actor, names: list[str], values) -> bool:
    for name in names:
        for value in values if isinstance(values, list) else [values]:
            try:
                before = actor.get_editor_property(name)
                actor.set_editor_property(name, value)
                after = actor.get_editor_property(name)
                return str(before) != str(after)
            except Exception:
                continue
    raise RuntimeError(f"Failed to set any property from {names}")


def _property_string(actor, names: list[str]) -> str:
    for name in names:
        try:
            return str(actor.get_editor_property(name))
        except Exception:
            continue
    return ""


def main() -> None:
    _load_map()
    actor, created = _spawn_or_get_bridge()
    changed = False
    changed |= _set_property(actor, ["route_map_widget_class", "RouteMapWidgetClass"], _soft_class_values(ROUTE_MAP_WIDGET_CLASS))
    changed |= _set_property(actor, ["route_node_widget_class", "RouteNodeWidgetClass"], _soft_class_values(ROUTE_NODE_WIDGET_CLASS))
    changed |= _set_property(actor, ["scroll_top_padding", "ScrollTopPadding"], 280.0)
    changed |= _set_property(actor, ["sync_interval_seconds", "SyncIntervalSeconds"], 0.25)
    changed |= _set_property(actor, ["max_sync_attempts", "MaxSyncAttempts"], 240)
    changed |= _set_property(actor, ["original_battle_start_level", "OriginalBattleStartLevel"], 1)
    changed |= _set_property(actor, ["original_current_level_int_property_index", "OriginalCurrentLevelIntPropertyIndex"], 1)

    save_current_level = bool(unreal.EditorLoadingAndSavingUtils.save_current_level())
    save_dirty_packages = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    result = {
        "ok": True,
        "map": MAP_PATH,
        "actor": actor.get_path_name(),
        "label": actor.get_actor_label(),
        "created": created,
        "changed": changed,
        "route_map_widget_class": _property_string(actor, ["route_map_widget_class", "RouteMapWidgetClass"]),
        "route_node_widget_class": _property_string(actor, ["route_node_widget_class", "RouteNodeWidgetClass"]),
        "original_battle_start_level": _property_string(actor, ["original_battle_start_level", "OriginalBattleStartLevel"]),
        "original_current_level_int_property_index": _property_string(actor, ["original_current_level_int_property_index", "OriginalCurrentLevelIntPropertyIndex"]),
        "save_current_level": save_current_level,
        "save_dirty_packages": save_dirty_packages,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
