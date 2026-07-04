from __future__ import annotations

import json
import sys
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
INSPECTOR_REPORT = PROJECT_ROOT / "Saved" / "AssetAnalysis" / "1game_blueprint_inspector_check.json"
REPORT_PATH = PROJECT_ROOT / "Saved" / "AssetAnalysis" / "1game_island_runtime_probe.json"


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


def _load_inspector() -> dict:
    try:
        return json.loads(INSPECTOR_REPORT.read_text(encoding="utf-8"))
    except Exception:
        return {}


def _vars_for(report: dict, key: str) -> list[dict]:
    values = report.get("reports", {}).get(key, {}).get("variables", [])
    return [value for value in values if isinstance(value, dict)]


def _var_names_by_type(report: dict, key: str, type_fragment: str) -> list[str]:
    result = []
    for variable in _vars_for(report, key):
        if type_fragment in str(variable.get("type", "")):
            name = variable.get("name")
            if name:
                result.append(str(name))
    return result


def _find_var_name(report: dict, key: str, type_fragment: str, index: int = 0) -> str:
    names = _var_names_by_type(report, key, type_fragment)
    if 0 <= index < len(names):
        return names[index]
    return ""


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
        if callable(value):
            return value()
        return value
    except Exception:
        return None


def _jsonable(value, depth: int = 0):
    if depth > 2:
        return str(value)
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if isinstance(value, (list, tuple)):
        return [_jsonable(item, depth + 1) for item in value[:20]]
    if isinstance(value, dict):
        return {str(key): _jsonable(item, depth + 1) for key, item in list(value.items())[:20]}

    result = {"repr": str(value), "type": type(value).__name__}
    for attr in ("x", "y", "X", "Y"):
        if hasattr(value, attr):
            try:
                result[attr] = _jsonable(getattr(value, attr), depth + 1)
            except Exception:
                pass
    for prop in ("X", "Y", "x", "y"):
        try:
            result[prop] = _jsonable(value.get_editor_property(prop), depth + 1)
        except Exception:
            pass
    try:
        result["path"] = value.get_path_name()
    except Exception:
        pass
    try:
        result["class"] = value.get_class().get_path_name()
    except Exception:
        pass
    return result


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


def _all_widgets(world):
    raw_widgets = []
    if hasattr(unreal, "WidgetBlueprintLibrary"):
        try:
            raw_widgets = unreal.WidgetBlueprintLibrary.get_all_widgets_of_class(world, unreal.UserWidget, False)
        except Exception:
            raw_widgets = []
    if not raw_widgets and hasattr(unreal, "get_objects_of_class"):
        try:
            raw_widgets = unreal.get_objects_of_class(unreal.UserWidget)
        except Exception:
            raw_widgets = []
    return list(raw_widgets)


def _target_class_name(report: dict, key: str) -> str:
    path = str(report.get("reports", {}).get(key, {}).get("generated_class", ""))
    if "." in path:
        return path.rsplit(".", 1)[-1]
    return path.rsplit("/", 1)[-1]


def _class_matches(widget, class_name: str) -> bool:
    if not class_name:
        return False
    return _class_name(widget) == class_name or _class_path(widget).endswith("." + class_name)


def _summarize_widget(widget) -> dict:
    result = {
        "path": _object_path(widget),
        "class": _class_path(widget),
        "class_name": _class_name(widget),
    }
    try:
        result["visible"] = str(widget.get_visibility())
    except Exception:
        pass
    try:
        result["in_viewport"] = bool(widget.is_in_viewport())
    except Exception:
        pass
    return result


def _summarize_slot(widget) -> dict:
    slot = _read_property(widget, "slot") or _read_property(widget, "Slot") or _read_attr_or_call(widget, "slot")
    result = {
        "slot_path": _object_path(slot),
        "slot_class": _class_path(slot),
        "slot_class_name": _class_name(slot),
    }
    for prop in (
        "layout_data",
        "position",
        "size",
        "alignment",
        "anchors",
        "offsets",
        "padding",
        "horizontal_alignment",
        "vertical_alignment",
    ):
        value = _read_property(slot, prop)
        if value is not None:
            result[prop] = _jsonable(value)
    for method_name in ("get_position", "get_size", "get_padding", "get_alignment", "get_anchors", "get_offsets"):
        value = _read_attr_or_call(slot, method_name)
        if value is not None:
            result[method_name] = _jsonable(value)
    layout_library = getattr(unreal, "WidgetLayoutLibrary", None)
    if layout_library is not None:
        for method_name in (
            "slot_as_canvas_slot",
            "slot_as_overlay_slot",
            "slot_as_scroll_box_slot",
            "slot_as_size_box_slot",
            "slot_as_uniform_grid_slot",
        ):
            method = getattr(layout_library, method_name, None)
            if method is None:
                continue
            try:
                typed_slot = method(widget)
            except Exception:
                typed_slot = None
            if typed_slot:
                result[method_name] = {
                    "slot_path": _object_path(typed_slot),
                    "slot_class": _class_path(typed_slot),
                    "slot_class_name": _class_name(typed_slot),
                }
                for typed_method_name in ("get_position", "get_size", "get_padding", "get_alignment", "get_anchors", "get_offsets"):
                    value = _read_attr_or_call(typed_slot, typed_method_name)
                    if value is not None:
                        result[method_name][typed_method_name] = _jsonable(value)
    return result


def _summarize_geometry(widget) -> dict:
    result = {}
    for method_name in ("get_desired_size", "get_cached_geometry"):
        value = _read_attr_or_call(widget, method_name)
        if value is not None:
            result[method_name] = _jsonable(value)
    return result


def _parent_chain(widget) -> list[dict]:
    result = []
    current = widget
    for _ in range(6):
        parent = _read_attr_or_call(current, "get_parent")
        if parent is None:
            break
        result.append(
            {
                "path": _object_path(parent),
                "class": _class_path(parent),
                "class_name": _class_name(parent),
            }
        )
        current = parent
    return result


def _first_parent_by_class(widget, class_name: str):
    current = widget
    for _ in range(8):
        parent = _read_attr_or_call(current, "get_parent")
        if parent is None:
            return None
        if _class_name(parent) == class_name:
            return parent
        current = parent
    return None


def _summarize_value(value) -> dict:
    values = _as_list(value)
    if values:
        return {
            "kind": "array",
            "count": len(values),
            "items": [_jsonable(item) for item in values[:10]],
        }
    if value is not None and hasattr(value, "get_class"):
        return {
            "kind": "object",
            "path": _object_path(value),
            "class": _class_path(value),
            "class_name": _class_name(value),
        }
    return {"kind": type(value).__name__, "value": _jsonable(value)}


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


def main() -> None:
    should_scroll_to_clickable = "--scroll-to-clickable" in sys.argv
    inspector = _load_inspector()
    pc_current_chapter_var = _find_var_name(inspector, "player_controller", "int", 0)
    pc_current_level_var = _find_var_name(inspector, "player_controller", "int", 1)
    pc_map_widget_var = _find_var_name(inspector, "player_controller", "/Game/1Game/UI/", 0)
    node_struct_var = _find_var_name(inspector, "node_widget", "ST_", 0)
    node_index_var = _find_var_name(inspector, "node_widget", "int", 0)
    node_can_click_var = _find_var_name(inspector, "node_widget", "bool", 0)
    map_vars = [str(variable.get("name")) for variable in _vars_for(inspector, "map_widget") if variable.get("name")]
    map_widget_class = _target_class_name(inspector, "map_widget")
    node_widget_class = _target_class_name(inspector, "node_widget")

    world = _get_game_world()
    player_controller = _first_player_controller(world)
    widgets = _all_widgets(world)
    map_widget_from_pc = _read_property(player_controller, pc_map_widget_var)
    map_widgets = [widget for widget in widgets if _class_matches(widget, map_widget_class)]
    node_widgets = [widget for widget in widgets if _class_matches(widget, node_widget_class)]
    map_widget_properties = {}
    if map_widget_from_pc:
        for var_name in map_vars:
            value = _read_property(map_widget_from_pc, var_name)
            map_widget_properties[var_name] = _summarize_value(value)
            node_widgets.extend(_collect_matching_objects(value, node_widget_class))
    node_widgets = _unique_objects(node_widgets)

    nodes = []
    clickable_widgets = []
    for widget in node_widgets:
        can_click = _read_property(widget, node_can_click_var)
        if can_click is True:
            clickable_widgets.append(widget)
        node_info = _read_property(widget, node_struct_var)
        nodes.append(
            {
                **_summarize_widget(widget),
                "slot": _summarize_slot(widget),
                "geometry": _summarize_geometry(widget),
                "parents": _parent_chain(widget),
                "can_click": _jsonable(can_click),
                "index": _jsonable(_read_property(widget, node_index_var)),
                "map_info": _jsonable(node_info),
            }
        )

    scroll_action = {"attempted": False}
    if should_scroll_to_clickable and clickable_widgets:
        first_clickable = clickable_widgets[0]
        scroll_box = _first_parent_by_class(first_clickable, "ScrollBox")
        slot = _read_property(first_clickable, "slot") or _read_property(first_clickable, "Slot") or _read_attr_or_call(first_clickable, "slot")
        position = _read_attr_or_call(slot, "get_position")
        target_offset = 0.0
        try:
            target_offset = max(0.0, float(position.y) - 280.0)
        except Exception:
            target_offset = 3000.0
        scroll_action = {
            "attempted": True,
            "target_node": _object_path(first_clickable),
            "scroll_box": _summarize_widget(scroll_box) if scroll_box else None,
            "target_offset": target_offset,
        }
        if scroll_box is not None:
            for before_method in ("get_scroll_offset",):
                value = _read_attr_or_call(scroll_box, before_method)
                if value is not None:
                    scroll_action["before_" + before_method] = _jsonable(value)
            try:
                scroll_box.set_scroll_offset(target_offset)
                scroll_action["set_scroll_offset"] = True
            except Exception as exc:
                scroll_action["set_scroll_offset_error"] = str(exc)
            try:
                scroll_box.scroll_widget_into_view(first_clickable, True, unreal.ScrollWhenFocusChanges.INSTANT_SCROLL, 0.0)
                scroll_action["scroll_widget_into_view"] = True
            except Exception as exc:
                scroll_action["scroll_widget_into_view_error"] = str(exc)
            try:
                scroll_box.invalidate_layout_and_volatility()
            except Exception:
                pass
            for after_method in ("get_scroll_offset",):
                value = _read_attr_or_call(scroll_box, after_method)
                if value is not None:
                    scroll_action["after_" + after_method] = _jsonable(value)

    result = {
        "ok": True,
        "has_world": world is not None,
        "world": _object_path(world),
        "player_controller": {
            "path": _object_path(player_controller),
            "class": _class_path(player_controller),
            "class_name": _class_name(player_controller),
            "current_chapter_var": pc_current_chapter_var,
            "current_chapter": _jsonable(_read_property(player_controller, pc_current_chapter_var)),
            "current_level_var": pc_current_level_var,
            "current_level": _jsonable(_read_property(player_controller, pc_current_level_var)),
            "map_widget_var": pc_map_widget_var,
            "map_widget": _summarize_widget(map_widget_from_pc) if map_widget_from_pc else None,
        },
        "class_filters": {
            "map_widget_class": map_widget_class,
            "node_widget_class": node_widget_class,
            "active_widget_count": len(widgets),
        },
        "map_widgets": [_summarize_widget(widget) for widget in map_widgets],
        "map_widget_properties": map_widget_properties,
        "node_count": len(nodes),
        "clickable_node_count": sum(1 for node in nodes if node.get("can_click") is True),
        "clickable_nodes": [node for node in nodes if node.get("can_click") is True],
        "scroll_action": scroll_action,
        "nodes": nodes,
    }

    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    compact = {
        "ok": result["ok"],
        "pc": result["player_controller"],
        "node_count": result["node_count"],
        "clickable_node_count": result["clickable_node_count"],
        "clickable_nodes": [
            {
                "path": node.get("path"),
                "index": node.get("index"),
                "slot_position": node.get("slot", {}).get("get_position"),
                "can_click": node.get("can_click"),
            }
            for node in result["clickable_nodes"][:5]
        ],
        "scroll_action": scroll_action,
        "report": str(REPORT_PATH),
    }
    print(json.dumps(compact, ensure_ascii=False))


if __name__ == "__main__":
    main()
