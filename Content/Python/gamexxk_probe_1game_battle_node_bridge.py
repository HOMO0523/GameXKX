from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
INSPECTOR_REPORT = PROJECT_ROOT / "Saved" / "AssetAnalysis" / "1game_blueprint_inspector_check.json"
REPORT_PATH = PROJECT_ROOT / "Saved" / "AssetAnalysis" / "1game_battle_node_bridge_probe.json"


def _object_path(value) -> str:
    if value is None:
        return ""
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def _class_path(value) -> str:
    if value is None:
        return ""
    try:
        return value.get_class().get_path_name()
    except Exception:
        return type(value).__name__


def _class_name(value) -> str:
    if value is None:
        return ""
    try:
        return value.get_class().get_name()
    except Exception:
        return type(value).__name__


def _enum_name(value) -> str:
    try:
        return str(value.name)
    except Exception:
        return str(value)


def _xy(value) -> dict:
    if value is None:
        return {}
    result = {}
    for name in ("x", "X"):
        try:
            result["x"] = float(getattr(value, name))
            break
        except Exception:
            pass
    for name in ("y", "Y"):
        try:
            result["y"] = float(getattr(value, name))
            break
        except Exception:
            pass
    return result


def _load_inspector() -> dict:
    try:
        return json.loads(INSPECTOR_REPORT.read_text(encoding="utf-8"))
    except Exception:
        return {}


def _vars_for(report: dict, key: str) -> list[dict]:
    values = report.get("reports", {}).get(key, {}).get("variables", [])
    return [value for value in values if isinstance(value, dict)]


def _find_var_name(report: dict, key: str, type_fragment: str, index: int = 0) -> str:
    names = []
    for variable in _vars_for(report, key):
        if type_fragment in str(variable.get("type", "")):
            name = variable.get("name")
            if name:
                names.append(str(name))
    return names[index] if 0 <= index < len(names) else ""


def _target_class_name(report: dict, key: str) -> str:
    path = str(report.get("reports", {}).get(key, {}).get("generated_class", ""))
    if "." in path:
        return path.rsplit(".", 1)[-1]
    return path.rsplit("/", 1)[-1]


def _class_matches(widget, class_name: str) -> bool:
    if not class_name:
        return False
    return _class_name(widget) == class_name or _class_path(widget).endswith("." + class_name)


def _read_property(obj, name: str):
    if obj is None or not name:
        return None
    try:
        return obj.get_editor_property(name)
    except Exception:
        return None


def _read_attr_or_call(obj, name: str):
    if obj is None:
        return None
    try:
        value = getattr(obj, name)
        return value() if callable(value) else value
    except Exception:
        return None


def _as_list(value) -> list:
    if value is None or isinstance(value, (str, bytes, bytearray)):
        return []
    if isinstance(value, (list, tuple)):
        return list(value)
    try:
        return list(value)
    except Exception:
        return []


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


def _all_widgets(world) -> list:
    if hasattr(unreal, "WidgetBlueprintLibrary"):
        try:
            return list(unreal.WidgetBlueprintLibrary.get_all_widgets_of_class(world, unreal.UserWidget, False))
        except Exception:
            return []
    if hasattr(unreal, "get_objects_of_class"):
        try:
            return list(unreal.get_objects_of_class(unreal.UserWidget))
        except Exception:
            return []
    return []


def _visibility(widget) -> str:
    try:
        return _enum_name(widget.get_visibility())
    except Exception:
        return ""


def _is_visible(widget) -> bool:
    visibility = _visibility(widget).upper()
    return "VISIBLE" in visibility and "COLLAPSED" not in visibility and "HIDDEN" not in visibility


def _is_in_viewport(widget) -> bool:
    try:
        return bool(widget.is_in_viewport())
    except Exception:
        return False


def _summarize_widget(widget) -> dict:
    return {
        "path": _object_path(widget),
        "class": _class_path(widget),
        "class_name": _class_name(widget),
        "visibility": _visibility(widget),
        "is_in_viewport": _is_in_viewport(widget),
        "is_visible": _is_visible(widget),
    }


def _collect_matching_objects(value, class_name: str) -> list:
    result = []
    values = _as_list(value)
    if values:
        for item in values:
            result.extend(_collect_matching_objects(item, class_name))
        return result
    if value is not None and _class_matches(value, class_name):
        result.append(value)
    return result


def _unique_objects(values: list) -> list:
    seen = set()
    result = []
    for value in values:
        key = _object_path(value)
        if key and key not in seen:
            seen.add(key)
            result.append(value)
    return result


def _parent_by_class(widget, class_name: str):
    current = widget
    for _ in range(10):
        parent = _read_attr_or_call(current, "get_parent")
        if parent is None:
            return None
        if _class_name(parent) == class_name:
            return parent
        current = parent
    return None


def _slot_position(widget) -> dict:
    slot = _read_property(widget, "slot") or _read_property(widget, "Slot") or _read_attr_or_call(widget, "slot")
    position = _read_attr_or_call(slot, "get_position")
    size = _read_attr_or_call(slot, "get_size")
    return {
        "position": _xy(position),
        "size": _xy(size),
    }


def _geometry_center(widget) -> tuple[dict, str]:
    geometry = _read_attr_or_call(widget, "get_cached_geometry")
    if geometry is None:
        return {}, "no_geometry"

    local_size = _read_attr_or_call(geometry, "get_local_size")
    if local_size is None:
        local_size = _read_attr_or_call(widget, "get_desired_size")
    local = _xy(local_size)
    local_center = None
    if local:
        try:
            local_center = unreal.Vector2D(local.get("x", 0.0) * 0.5, local.get("y", 0.0) * 0.5)
        except Exception:
            local_center = None

    local_to_absolute = getattr(geometry, "local_to_absolute", None)
    if callable(local_to_absolute) and local_center is not None:
        try:
            return _xy(local_to_absolute(local_center)), "local_to_absolute"
        except Exception as exc:
            last_error = str(exc)
    else:
        last_error = "local_to_absolute_unavailable"

    slate_library = getattr(unreal, "SlateBlueprintLibrary", None)
    local_to_viewport = getattr(slate_library, "local_to_viewport", None) if slate_library else None
    if callable(local_to_viewport) and local_center is not None:
        try:
            value = local_to_viewport(_get_game_world(), geometry, local_center)
            if isinstance(value, (list, tuple)) and value:
                return _xy(value[0]), "slate_local_to_viewport_pixel"
            return _xy(value), "slate_local_to_viewport_pixel"
        except Exception as exc:
            last_error = str(exc)

    absolute_position = _read_attr_or_call(geometry, "get_absolute_position")
    absolute = _xy(absolute_position)
    if absolute and local:
        return {
            "x": absolute["x"] + local.get("x", 0.0) * 0.5,
            "y": absolute["y"] + local.get("y", 0.0) * 0.5,
        }, "absolute_position_plus_size"

    return {}, last_error


def _scroll_to_clickable(clickable_widgets: list) -> dict:
    if not clickable_widgets:
        return {"attempted": False, "reason": "no_clickable_widgets"}

    first_clickable = clickable_widgets[0]
    scroll_box = _parent_by_class(first_clickable, "ScrollBox")
    slot = _slot_position(first_clickable)
    position = slot.get("position", {})
    target_offset = max(0.0, float(position.get("y", 0.0)) - 280.0)
    result = {
        "attempted": True,
        "target_node": _object_path(first_clickable),
        "scroll_box": _summarize_widget(scroll_box) if scroll_box else None,
        "target_offset": target_offset,
    }
    if scroll_box is None:
        return result

    result["before_offset"] = _read_attr_or_call(scroll_box, "get_scroll_offset")
    try:
        scroll_box.set_scroll_offset(target_offset)
        result["set_scroll_offset"] = True
    except Exception as exc:
        result["set_scroll_offset_error"] = str(exc)
    try:
        scroll_box.invalidate_layout_and_volatility()
    except Exception:
        pass
    result["after_offset"] = _read_attr_or_call(scroll_box, "get_scroll_offset")
    return result


def main() -> None:
    should_scroll = "--scroll-to-clickable" in sys.argv
    inspector = _load_inspector()
    pc_current_level_var = _find_var_name(inspector, "player_controller", "int", 1)
    pc_map_widget_var = _find_var_name(inspector, "player_controller", "/Game/1Game/UI/", 0)
    node_can_click_var = _find_var_name(inspector, "node_widget", "bool", 0)
    node_index_var = _find_var_name(inspector, "node_widget", "int", 0)
    map_widget_class = _target_class_name(inspector, "map_widget")
    node_widget_class = _target_class_name(inspector, "node_widget")
    map_vars = [str(variable.get("name")) for variable in _vars_for(inspector, "map_widget") if variable.get("name")]

    world = _get_game_world()
    player_controller = _first_player_controller(world)
    widgets = _all_widgets(world)
    map_widget_from_pc = _read_property(player_controller, pc_map_widget_var)
    map_widgets = [widget for widget in widgets if _class_matches(widget, map_widget_class)]
    if map_widget_from_pc:
        map_widgets.append(map_widget_from_pc)
    map_widgets = _unique_objects(map_widgets)
    node_widgets = [widget for widget in widgets if _class_matches(widget, node_widget_class)]
    if map_widget_from_pc:
        for var_name in map_vars:
            node_widgets.extend(_collect_matching_objects(_read_property(map_widget_from_pc, var_name), node_widget_class))
    node_widgets = _unique_objects(node_widgets)
    battle_widgets = [
        widget for widget in widgets
        if "GameXXKBattleBoardWidget" in _class_name(widget) or "GameXXKBattleBoardWidget" in _class_path(widget)
    ]

    clickable_widgets = []
    for widget in node_widgets:
        if _read_property(widget, node_can_click_var) is True:
            clickable_widgets.append(widget)

    scroll_action = _scroll_to_clickable(clickable_widgets) if should_scroll else {"attempted": False}

    clickable_nodes = []
    for widget in clickable_widgets:
        center, center_source = _geometry_center(widget)
        clickable_nodes.append({
            **_summarize_widget(widget),
            "index": _read_property(widget, node_index_var),
            "slot": _slot_position(widget),
            "screen_center": center,
            "screen_center_source": center_source,
            "can_click": True,
        })

    result = {
        "ok": True,
        "has_world": world is not None,
        "world": _object_path(world),
        "player_controller": {
            **_summarize_widget(player_controller),
            "current_level_var": pc_current_level_var,
            "current_level": _read_property(player_controller, pc_current_level_var),
            "map_widget_var": pc_map_widget_var,
            "map_widget": _summarize_widget(map_widget_from_pc) if map_widget_from_pc else None,
        },
        "route_map_widgets": [_summarize_widget(widget) for widget in map_widgets],
        "battle_widgets": [_summarize_widget(widget) for widget in battle_widgets],
        "battle_widget_visible": any(_is_in_viewport(widget) and _is_visible(widget) for widget in battle_widgets),
        "clickable_node_count": len(clickable_nodes),
        "clickable_nodes": clickable_nodes,
        "scroll_action": scroll_action,
    }

    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    compact = {
        "ok": result["ok"],
        "current_level": result["player_controller"]["current_level"],
        "route_map_widgets": result["route_map_widgets"],
        "battle_widgets": result["battle_widgets"],
        "battle_widget_visible": result["battle_widget_visible"],
        "clickable_node_count": result["clickable_node_count"],
        "clickable_nodes": [
            {
                "path": node["path"],
                "index": node["index"],
                "screen_center": node["screen_center"],
                "screen_center_source": node["screen_center_source"],
                "slot": node["slot"],
            }
            for node in clickable_nodes[:8]
        ],
        "scroll_action": scroll_action,
        "report": str(REPORT_PATH),
    }
    print(json.dumps(compact, ensure_ascii=False))


if __name__ == "__main__":
    main()
