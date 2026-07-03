from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
REPORT_PATH = PROJECT_ROOT / "Saved" / "AssetAnalysis" / "active_widgets_probe.json"


def _class_path(value) -> str:
    if value is None:
        return ""
    try:
        return value.get_class().get_path_name()
    except Exception:
        return str(type(value))


def _class_name(value) -> str:
    if value is None:
        return ""
    try:
        return value.get_class().get_name()
    except Exception:
        return type(value).__name__


def _object_path(value) -> str:
    if value is None:
        return ""
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def _enum_name(value) -> str:
    try:
        return str(value.name)
    except Exception:
        return str(value)


def _get_game_world():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if subsystem is None:
        return None
    try:
        return subsystem.get_game_world()
    except Exception:
        return None


def _first_player_controller(world):
    if world is None:
        return None
    try:
        return unreal.GameplayStatics.get_player_controller(world, 0)
    except Exception:
        return None


def _widget_summary(widget) -> dict:
    result = {
        "path": _object_path(widget),
        "class": _class_path(widget),
        "class_name": _class_name(widget),
    }
    try:
        result["is_in_viewport"] = bool(widget.is_in_viewport())
    except Exception:
        pass
    try:
        result["visibility"] = _enum_name(widget.get_visibility())
    except Exception:
        pass
    return result


def _all_user_widgets(world) -> list[dict]:
    widgets = []
    raw_widgets = []
    if hasattr(unreal, "WidgetBlueprintLibrary"):
        try:
            raw_widgets = unreal.WidgetBlueprintLibrary.get_all_widgets_of_class(world, unreal.UserWidget, False)
        except Exception as exc:
            return [{"error": str(exc)}]
    elif hasattr(unreal, "get_objects_of_class"):
        try:
            raw_widgets = unreal.get_objects_of_class(unreal.UserWidget)
        except Exception as exc:
            return [{"error": str(exc)}]
    elif hasattr(unreal, "ObjectLibrary"):
        return [{"error": "no runtime user widget enumeration API is exposed"}]
    else:
        return [{"error": "no runtime user widget enumeration API is exposed"}]

    for widget in raw_widgets:
        widgets.append(_widget_summary(widget))
    return widgets


def main() -> None:
    world = _get_game_world()
    player_controller = _first_player_controller(world)
    widgets = _all_user_widgets(world)
    result = {
        "ok": True,
        "has_pie_world": world is not None,
        "world": _object_path(world),
        "player_controller": {
            "path": _object_path(player_controller),
            "class": _class_path(player_controller),
            "class_name": _class_name(player_controller),
        },
        "widgets": widgets,
        "onegame_route_widgets": [
            widget for widget in widgets
            if "UI_地图选择" in widget.get("class", "") or "UI_地图选择" in widget.get("class_name", "")
        ],
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
