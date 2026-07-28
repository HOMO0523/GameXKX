"""Read-only UE 5.8 API inventory for the guarded town-terrain transfer.

The probe never changes maps, assets, actors, or packages.  It reports the
available Geometry, Landscape, trace, and StaticMesh editor bindings so the
baking phase can use only APIs present in the running GameXXK editor.
"""

from __future__ import annotations

import json

import unreal


def _names(value: object, contains: str = "") -> list[str]:
    try:
        names = [name for name in dir(value) if not name.startswith("_")]
    except Exception:
        return []
    token = contains.lower()
    if token:
        names = [name for name in names if token in name.lower()]
    return sorted(names)


def _exists(name: str) -> bool:
    return getattr(unreal, name, None) is not None


def _doc(value: object) -> str:
    return str(getattr(value, "__doc__", ""))


def main() -> dict[str, object]:
    system_library = getattr(unreal, "SystemLibrary", None)
    static_mesh_editor_subsystem = getattr(unreal, "StaticMeshEditorSubsystem", None)
    landscape_class = getattr(unreal, "Landscape", None)
    geometry_types = sorted(
        name
        for name in dir(unreal)
        if name.startswith("GeometryScript") or name.startswith("DynamicMesh")
    )
    report = {
        "system_library_line_trace_bindings": _names(system_library, "line_trace"),
        "static_mesh_editor_subsystem": _exists("StaticMeshEditorSubsystem"),
        "static_mesh_editor_methods": _names(static_mesh_editor_subsystem, "mesh"),
        "landscape_class": _exists("Landscape"),
        "landscape_methods": _names(landscape_class, "height"),
        "editor_level_library": _exists("EditorLevelLibrary"),
        "geometry_bindings": geometry_types,
        "asset_tools_helpers": _exists("AssetToolsHelpers"),
        "mesh_description": _exists("MeshDescription"),
    }
    line_trace_single = getattr(system_library, "line_trace_single", None)
    line_trace_single_new = getattr(system_library, "line_trace_single_new", None)
    report["line_trace_single_doc"] = str(getattr(line_trace_single, "__doc__", ""))
    report["line_trace_single_new_doc"] = str(getattr(line_trace_single_new, "__doc__", ""))
    report["trace_type_members"] = _names(getattr(unreal, "TraceTypeQuery", None))
    report["draw_debug_trace_members"] = _names(getattr(unreal, "DrawDebugTrace", None))
    report["bake_resolution_members"] = _names(getattr(unreal, "GeometryScriptBakeResolution", None))
    report["bake_sample_members"] = _names(getattr(unreal, "GeometryScriptBakeSamplesPerPixel", None))
    editor_level_library = getattr(unreal, "EditorLevelLibrary", None)
    asset_utils = getattr(unreal, "GeometryScript_AssetUtils", None)
    scene_utils = getattr(unreal, "GeometryScript_SceneUtility", None)
    report["landscape_export_methods"] = _names(landscape_class, "export")
    report["editor_level_conversion_methods"] = _names(editor_level_library, "convert")
    report["asset_utils_methods"] = _names(asset_utils)
    report["scene_utils_methods"] = _names(scene_utils)
    report["editor_level_convert_actors_doc"] = _doc(getattr(editor_level_library, "convert_actors", None))
    report["asset_utils_create_static_mesh_doc"] = _doc(
        getattr(asset_utils, "create_new_static_mesh_asset_from_object_path", None)
    )
    report["asset_utils_copy_to_static_mesh_doc"] = _doc(
        getattr(asset_utils, "copy_mesh_to_static_mesh", None)
    )
    report["scene_utils_copy_from_component_doc"] = _doc(
        getattr(scene_utils, "copy_mesh_from_component", None)
    )
    primitives = getattr(unreal, "GeometryScript_Primitives", None)
    deforms = getattr(unreal, "GeometryScript_DeformMesh", None)
    editor_asset_utils = getattr(unreal, "GeometryScript_NewAssetUtils", None)
    editor_texture_utils = getattr(unreal, "GeometryScript_EditorTextureUtils", None)
    mesh_queries = getattr(unreal, "GeometryScript_MeshQueries", None)
    mesh_edits = getattr(unreal, "GeometryScript_MeshEdits", None)
    bake_utils = getattr(unreal, "GeometryScript_Bake", None)
    report["dynamic_mesh_class"] = _exists("DynamicMesh")
    report["primitive_rectangle_doc"] = _doc(getattr(primitives, "append_rectangle_xy", None))
    report["deform_texture_doc"] = _doc(
        getattr(deforms, "apply_displace_from_texture_map", None)
    )
    report["create_static_mesh_from_dynamic_doc"] = _doc(
        getattr(editor_asset_utils, "create_new_static_mesh_asset_from_mesh", None)
    )
    report["copy_dynamic_mesh_to_static_doc"] = _doc(
        getattr(asset_utils, "copy_mesh_to_static_mesh", None)
    )
    report["new_asset_create_static_mesh_doc"] = _doc(
        getattr(editor_asset_utils, "create_new_static_mesh_asset_from_mesh", None)
    )
    report["mesh_vertex_positions_doc"] = _doc(
        getattr(mesh_queries, "get_all_vertex_positions", None)
    )
    report["mesh_set_vertex_positions_doc"] = _doc(
        getattr(mesh_edits, "set_all_mesh_vertex_positions", None)
    )
    report["bake_render_captures_doc"] = _doc(
        getattr(bake_utils, "bake_texture_from_render_captures", None)
    )
    report["new_asset_create_texture_doc"] = _doc(
        getattr(editor_asset_utils, "create_new_texture_2d_asset", None)
    )
    report["new_asset_create_texture2d_doc"] = _doc(
        getattr(editor_asset_utils, "create_new_texture2d_asset", None)
    )
    report["new_asset_utils_methods"] = _names(editor_asset_utils)
    report["editor_texture_utils_methods"] = _names(editor_texture_utils)
    mesh_probe = unreal.DynamicMesh()
    primitive_options = unreal.GeometryScriptPrimitiveOptions()
    unreal.GeometryScript_Primitives.append_rectangle_xy(
        mesh_probe,
        primitive_options,
        unreal.Transform(),
        200.0,
        100.0,
        2,
        1,
    )
    _, position_list, has_gaps = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(
        mesh_probe,
        False,
    )
    probe_positions = unreal.GeometryScript_List.convert_vector_list_to_array(position_list)
    report["transient_grid_probe"] = {
        "vertex_count": len(probe_positions),
        "has_gaps": bool(has_gaps),
        "first_position": list(probe_positions[0].to_tuple()),
        "last_position": list(probe_positions[-1].to_tuple()),
    }
    print(json.dumps(report, ensure_ascii=False, sort_keys=True))
    return report


if __name__ == "__main__":
    main()
