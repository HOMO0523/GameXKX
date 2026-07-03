from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[1]
REPORT_PATH = PROJECT_ROOT / "Saved" / "AssetAnalysis" / "1game_ui_framework.json"
EXPORT_DIR = PROJECT_ROOT / "Saved" / "AssetAnalysis" / "1game_ui_exports"
ROOT_PATH = "/Game/1Game"


def _stringify(value):
    if value is None:
        return None
    try:
        if isinstance(value, unreal.Name):
            return str(value)
    except Exception:
        pass
    try:
        if hasattr(value, "get_name"):
            return value.get_name()
    except Exception:
        pass
    return str(value)


def _maybe_call(value):
    if callable(value):
        try:
            return value()
        except Exception:
            return value
    return value


def _class_name(obj):
    if obj is None:
        return None
    try:
        return obj.get_class().get_name()
    except Exception:
        return type(obj).__name__


def _get_prop(obj, *names):
    for name in names:
        try:
            return obj.get_editor_property(name)
        except Exception:
            pass
        try:
            return _maybe_call(getattr(obj, name))
        except Exception:
            pass
    return None


def _asset_class(asset_path):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    return _class_name(asset)


def _object_path(asset_path):
    leaf = asset_path.rsplit("/", 1)[-1]
    if "." in leaf:
        return asset_path
    return f"{asset_path}.{leaf}"


def _native_inspect(asset_path):
    library = getattr(unreal, "GameXXKBlueprintInspectorLibrary", None)
    if library is None:
        return {"available": False, "error": "GameXXKBlueprintInspectorLibrary is not exposed"}
    inspector = getattr(library, "inspect_asset_to_json", None)
    if inspector is None:
        return {"available": False, "error": "inspect_asset_to_json is not exposed"}

    attempts = []
    for candidate in dict.fromkeys((asset_path, _object_path(asset_path))):
        try:
            raw = inspector(candidate)
            parsed = json.loads(raw)
            parsed["available"] = True
            parsed["requested_path"] = asset_path
            parsed["inspected_path"] = candidate
            if parsed.get("ok"):
                return parsed
            attempts.append({"path": candidate, "parsed": parsed})
        except Exception as exc:
            attempts.append({"path": candidate, "error": str(exc)})
    return {"available": True, "ok": False, "requested_path": asset_path, "attempts": attempts}


def _asset_data(asset_path):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        data = registry.get_asset_by_object_path(asset_path)
    except Exception:
        data = None
    result = {
        "path": asset_path,
        "asset_name": asset_path.rsplit("/", 1)[-1],
        "class": None,
        "package": asset_path,
        "tags": {},
    }
    if data:
        try:
            result["asset_name"] = str(data.asset_name)
            result["package"] = str(data.package_name)
            result["class"] = str(data.asset_class_path.asset_name)
        except Exception:
            pass
        try:
            result["tags"] = {str(k): str(v) for k, v in data.tags_and_values.items()}
        except Exception:
            result["tags"] = {}
    if not result["class"]:
        result["class"] = _asset_class(asset_path)
    return result


def _dependencies(asset_path):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    package_name = asset_path
    try:
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if asset:
            package_name = asset.get_outermost().get_name()
    except Exception:
        pass
    deps = []
    try:
        options = unreal.AssetRegistryDependencyOptions(
            include_soft_package_references=True,
            include_hard_package_references=True,
            include_searchable_names=False,
            include_soft_management_references=False,
            include_hard_management_references=False,
        )
        deps = registry.get_dependencies(package_name, options)
    except Exception:
        try:
            deps = registry.get_dependencies(package_name)
        except Exception:
            deps = []
    return sorted({str(dep) for dep in deps if str(dep).startswith("/Game/1Game")})


def _widget_name(widget):
    try:
        return widget.get_name()
    except Exception:
        return str(widget)


def _children(widget):
    children = []
    try:
        count = widget.get_children_count()
    except Exception:
        count = 0
    for index in range(count):
        try:
            children.append(widget.get_child_at(index))
        except Exception:
            pass
    return children


def _widget_tree(asset):
    tree = _get_prop(asset, "widget_tree", "WidgetTree")
    if tree is None:
        return {"root": None, "widgets": []}
    root = _get_prop(tree, "root_widget", "RootWidget")
    widgets = []
    seen = set()

    def visit(widget, depth):
        if widget is None:
            return
        name = _widget_name(widget)
        key = id(widget)
        if key in seen:
            return
        seen.add(key)
        slot = _get_prop(widget, "slot", "Slot")
        text_value = _get_prop(widget, "text", "Text")
        brush = _get_prop(widget, "brush", "Brush")
        image = _get_prop(brush, "resource_object", "ResourceObject") if brush is not None else None
        widgets.append(
            {
                "name": name,
                "class": _class_name(widget),
                "depth": depth,
                "slot_class": _class_name(slot),
                "text": _stringify(text_value),
                "image": _stringify(image),
                "children": [_widget_name(child) for child in _children(widget)],
            }
        )
        for child in _children(widget):
            visit(child, depth + 1)

    visit(root, 0)
    return {"root": _widget_name(root) if root else None, "widgets": widgets}


def _pin_info(pin):
    linked_to = []
    try:
        linked_to = [_stringify(linked) for linked in pin.linked_to]
    except Exception:
        linked_to = []
    return {
        "name": _stringify(_get_prop(pin, "pin_name", "PinName")),
        "direction": _stringify(_get_prop(pin, "direction", "Direction")),
        "category": _stringify(_get_prop(_get_prop(pin, "pin_type", "PinType"), "pin_category", "PinCategory")),
        "sub_category": _stringify(_get_prop(_get_prop(pin, "pin_type", "PinType"), "pin_sub_category", "PinSubCategory")),
        "default": _stringify(_get_prop(pin, "default_value", "DefaultValue")),
        "linked_to": linked_to,
    }


def _node_title(node):
    try:
        return str(node.get_node_title(unreal.NodeTitleType.FULL_TITLE))
    except Exception:
        pass
    try:
        return str(node.get_node_title(unreal.NodeTitleType.LIST_VIEW))
    except Exception:
        pass
    return _widget_name(node)


def _graph_info(graph):
    nodes = []
    graph_nodes = _get_prop(graph, "nodes", "Nodes") or []
    if not graph_nodes:
        for method_name in ("get_nodes", "get_graph_nodes", "get_all_nodes"):
            try:
                graph_nodes = list(getattr(graph, method_name)())
                if graph_nodes:
                    break
            except Exception:
                pass
    for node in graph_nodes:
        pins = []
        try:
            pins = [_pin_info(pin) for pin in node.pins]
        except Exception:
            pins = []
        nodes.append({
            "name": _widget_name(node),
            "class": _class_name(node),
            "title": _node_title(node),
            "function_name": _stringify(_get_prop(node, "function_name", "FunctionName")),
            "custom_function_name": _stringify(_get_prop(node, "custom_function_name", "CustomFunctionName")),
            "event_reference": _stringify(_get_prop(node, "event_reference", "EventReference")),
            "pins": pins,
        })
    return {
        "name": _widget_name(graph),
        "class": _class_name(graph),
        "node_count": len(nodes),
        "nodes": nodes,
        "diagnostics": sorted([name for name in dir(graph) if "node" in name.lower() or "pin" in name.lower() or "graph" in name.lower()])[:120],
    }


def _blueprint_variables(asset):
    variables = []
    names = []
    for function_name in ("list_member_variable_names", "get_blueprint_variable_list"):
        try:
            names = list(getattr(unreal.BlueprintEditorLibrary, function_name)(asset))
            if names:
                break
        except Exception:
            pass
    for name in names:
        entry = {"name": str(name)}
        try:
            entry["category"] = str(unreal.BlueprintEditorLibrary.get_blueprint_variable_category(asset, name))
        except Exception:
            pass
        try:
            entry["type"] = str(unreal.BlueprintEditorLibrary.get_member_variable_type(asset, name))
        except Exception:
            pass
        try:
            entry["type_legacy"] = str(unreal.BlueprintEditorLibrary.get_blueprint_variable_type(asset, name))
        except Exception:
            pass
        try:
            entry["default"] = str(unreal.BlueprintEditorLibrary.get_blueprint_variable_default_value(asset, name))
        except Exception:
            pass
        variables.append(entry)
    return variables


def _blueprint_graphs(asset):
    result = {}
    try:
        result["graph_names"] = [str(name) for name in unreal.BlueprintEditorLibrary.list_graph_names(asset)]
    except Exception:
        result["graph_names"] = []
    for function_name in ("list_graphs", "get_all_graphs", "get_graphs", "get_function_graphs"):
        try:
            func = getattr(unreal.BlueprintEditorLibrary, function_name)
            graphs = func(asset)
            if graphs:
                result["blueprint_editor_library_" + function_name] = [_graph_info(graph) for graph in graphs]
                return result
        except Exception:
            pass
    graph_props = (
        ("ubergraph_pages", "UbergraphPages"),
        ("function_graphs", "FunctionGraphs"),
        ("macro_graphs", "MacroGraphs"),
        ("delegate_signature_graphs", "DelegateSignatureGraphs"),
    )
    for prop_names in graph_props:
        graphs = _get_prop(asset, *prop_names) or []
        result[prop_names[0]] = [_graph_info(graph) for graph in graphs]
    return result


def _blueprint_extra(asset):
    return {
        "variables": _blueprint_variables(asset),
        "graphs": _blueprint_graphs(asset),
        "animations": [
            _widget_name(anim)
            for anim in (_get_prop(asset, "animations", "Animations") or [])
        ],
    }


def _struct_or_enum_info(asset_path):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path) or unreal.load_asset(asset_path)
    info = _asset_data(asset_path)
    info["dependencies"] = _dependencies(asset_path)
    info["native_inspector"] = _native_inspect(asset_path)
    info["raw_candidate_properties"] = {}
    for prop_name in (
        "display_names",
        "DisplayNames",
        "names",
        "Names",
        "variables_descriptions",
        "VariablesDescriptions",
        "editor_data",
        "EditorData",
        "guid",
        "Guid",
    ):
        value = _get_prop(asset, prop_name)
        if value is not None:
            if isinstance(value, (list, tuple)):
                info["raw_candidate_properties"][prop_name] = [_stringify(item) for item in value]
            else:
                info["raw_candidate_properties"][prop_name] = _stringify(value)
    return info


def _blueprint_info(asset_path):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset is None:
        asset = unreal.load_asset(asset_path)
    result = _asset_data(asset_path)
    result["dependencies"] = _dependencies(asset_path)
    result["native_inspector"] = _native_inspect(asset_path)
    result["parent_class"] = _stringify(_get_prop(asset, "parent_class", "ParentClass"))
    result["generated_class"] = _stringify(_get_prop(asset, "generated_class", "GeneratedClass"))
    result["widget_tree"] = _widget_tree(asset)
    generated_class = _get_prop(asset, "generated_class", "GeneratedClass")
    cdo = None
    try:
        cdo = generated_class.get_default_object()
    except Exception:
        pass
    result["generated_cdo_class"] = _class_name(cdo)
    result["generated_cdo_widget_tree"] = _widget_tree(cdo) if cdo is not None else {"root": None, "widgets": []}
    result.update(_blueprint_extra(asset))
    export_file = EXPORT_DIR / (asset_path.replace("/Game/1Game/", "").replace("/", "__").replace(".", "__") + ".txt")
    result["export"] = {"attempted": False, "success": False, "file": str(export_file)}
    try:
        EXPORT_DIR.mkdir(parents=True, exist_ok=True)
        task = unreal.AssetExportTask()
        task.object = asset
        task.filename = str(export_file)
        task.automated = True
        task.replace_identical = True
        task.prompt = False
        result["export"]["attempted"] = True
        result["export"]["success"] = bool(unreal.Exporter.run_asset_export_task(task))
    except Exception as exc:
        result["export"]["error"] = str(exc)
    result["diagnostics"] = {
        "asset_dir_sample": sorted([name for name in dir(asset) if "graph" in name.lower() or "widget" in name.lower() or "class" in name.lower()])[:120],
        "blueprint_editor_library_sample": sorted([name for name in dir(unreal.BlueprintEditorLibrary) if "graph" in name.lower() or "variable" in name.lower() or "class" in name.lower()])[:160] if hasattr(unreal, "BlueprintEditorLibrary") else [],
    }
    return result


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        registry.scan_paths_synchronous([ROOT_PATH], True)
    except Exception:
        pass
    directory_exists = False
    try:
        directory_exists = bool(unreal.EditorAssetLibrary.does_directory_exist(ROOT_PATH))
    except Exception:
        pass
    asset_paths = sorted(unreal.EditorAssetLibrary.list_assets(ROOT_PATH, recursive=True, include_folder=False))
    if not asset_paths:
        asset_paths = sorted(unreal.EditorAssetLibrary.list_assets(ROOT_PATH + "/", recursive=True, include_folder=False))
    disk_asset_paths = []
    disk_root = PROJECT_ROOT / "Content" / "1Game"
    if disk_root.exists():
        for file_path in sorted(disk_root.rglob("*.uasset")):
            rel = file_path.relative_to(PROJECT_ROOT / "Content").with_suffix("")
            disk_asset_paths.append("/Game/" + "/".join(rel.parts))
    direct_load_probe = []
    if not asset_paths:
        asset_paths = disk_asset_paths
    for probe_path in disk_asset_paths[:]:
        try:
            exists = bool(unreal.EditorAssetLibrary.does_asset_exist(probe_path))
        except Exception:
            exists = False
        asset = None
        try:
            asset = unreal.load_asset(probe_path)
        except Exception:
            pass
        direct_load_probe.append({
            "path": probe_path,
            "editor_asset_exists": exists,
            "load_class": _class_name(asset),
        })
    groups = {key: [] for key in ("ui", "data", "texture", "material", "control", "map", "other")}
    assets = []
    for path in asset_paths:
        info = _asset_data(path)
        info["dependencies"] = _dependencies(path)
        assets.append(info)
        lower = path.lower()
        normalized = lower.split(".", 1)[0]
        segments = [segment for segment in normalized.split("/") if segment]
        bucket = segments[2] if len(segments) >= 3 and segments[0] == "game" and segments[1] == "1game" else ""
        if bucket == "ui":
            groups["ui"].append(path)
        elif bucket == "data":
            groups["data"].append(path)
        elif bucket == "texture":
            groups["texture"].append(path)
        elif bucket == "material":
            groups["material"].append(path)
        elif bucket == "control":
            groups["control"].append(path)
        elif bucket == "map":
            groups["map"].append(path)
        else:
            groups["other"].append(path)

    widget_blueprints = []
    for path in groups["ui"]:
        try:
            widget_blueprints.append(_blueprint_info(path))
        except Exception as exc:
            widget_blueprints.append({"path": path, "error": str(exc)})

    control_blueprints = []
    for path in groups["control"]:
        try:
            control_blueprints.append(_blueprint_info(path))
        except Exception as exc:
            control_blueprints.append({"path": path, "error": str(exc)})

    data_assets = []
    for path in groups["data"]:
        try:
            data_assets.append(_struct_or_enum_info(path))
        except Exception as exc:
            data_assets.append({"path": path, "error": str(exc)})

    report = {
        "root": ROOT_PATH,
        "directory_exists": directory_exists,
        "asset_count": len(asset_paths),
        "disk_asset_count": len(disk_asset_paths),
        "direct_load_probe": direct_load_probe,
        "groups": groups,
        "assets": assets,
        "widget_blueprints": widget_blueprints,
        "control_blueprints": control_blueprints,
        "data_assets": data_assets,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({
        "report": str(REPORT_PATH),
        "asset_count": len(asset_paths),
        "group_counts": {key: len(value) for key, value in groups.items()},
        "widgets": [item.get("path") for item in widget_blueprints],
        "controls": [item.get("path") for item in control_blueprints],
    }, ensure_ascii=False))


if __name__ == "__main__":
    main()
