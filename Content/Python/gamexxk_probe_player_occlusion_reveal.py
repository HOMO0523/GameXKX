"""Report the live PIE state of the full-color player occlusion reveal."""

from __future__ import annotations

import json
import sys
from pathlib import Path
import unreal


def _path(value):
    return "" if value is None else str(value.get_path_name())


def main():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = subsystem.get_game_world() if subsystem else None
    if world is None:
        raise RuntimeError("no active PIE world")
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    camera = controller.get_editor_property("player_camera_manager") if controller else None
    if pawn is None or camera is None:
        raise RuntimeError("PIE player or camera is unavailable")

    subsystem_mvp = None
    for getter_name in ("get_town_overlay_widget_for_test", "get_main_menu_widget_for_test"):
        try:
            widget = getattr(controller, getter_name)()
            subsystem_mvp = widget.get_mvp_subsystem() if widget else None
        except Exception:
            subsystem_mvp = None
        if subsystem_mvp:
            break
    if "--ensure-town" in sys.argv[1:] and subsystem_mvp:
        subsystem_mvp.ensure_qingshan_town_runtime_for_direct_map()

    components = pawn.get_components_by_class(unreal.PaperFlipbookComponent)
    visuals = []
    for component in components:
        flipbook = component.get_flipbook()
        material = component.get_material(0)
        visuals.append({
            "name": str(component.get_name()),
            "visible": bool(component.is_visible()),
            "flipbook": _path(flipbook),
            "frame": int(component.get_playback_position_in_frames()),
            "material": _path(material),
            "sort_priority": int(component.get_editor_property("translucency_sort_priority")),
        })

    captures = []
    viewport_size = controller.get_viewport_size() if controller else (0, 0)
    pawn_cameras = []
    for pawn_camera in pawn.get_components_by_class(unreal.CameraComponent):
        pawn_cameras.append({
            "name": str(pawn_camera.get_name()),
            "fov": round(float(pawn_camera.get_editor_property("field_of_view")), 3),
            "aspect_ratio": round(float(pawn_camera.get_editor_property("aspect_ratio")), 5),
            "projection_mode": str(pawn_camera.get_editor_property("projection_mode")),
        })
    for capture in pawn.get_components_by_class(unreal.SceneCaptureComponent2D):
        if "--force-scene-color" in sys.argv[1:]:
            capture.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR)
        if "--force-final-color" in sys.argv[1:]:
            capture.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        if "--force-render-scene" in sys.argv[1:]:
            capture.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_RENDER_SCENE_PRIMITIVES)
        if "--clear-capture-hidden" in sys.argv[1:]:
            capture.clear_hidden_components()
            capture.capture_scene()
        elif "--force-final-color" in sys.argv[1:]:
            capture.capture_scene()
        target = capture.get_editor_property("texture_target")
        if "--export-capture" in sys.argv[1:] and target is not None:
            export_dir = Path(unreal.Paths.project_saved_dir()) / "Screenshots" / "OcclusionProbe"
            export_dir.mkdir(parents=True, exist_ok=True)
            unreal.RenderingLibrary.export_render_target(world, target, str(export_dir), "RevealCapture")
        pixel_samples = {}
        if target is not None:
            width = int(target.get_editor_property("size_x"))
            height = int(target.get_editor_property("size_y"))
            for label, x, y in (
                ("center", width // 2, height // 2),
                ("center_left", width * 2 // 5, height // 2),
                ("center_right", width * 3 // 5, height // 2),
                ("center_up", width // 2, height * 2 // 5),
                ("center_down", width // 2, height * 3 // 5),
                ("upper_left", width // 4, height // 4),
                ("lower_right", width * 3 // 4, height * 3 // 4),
            ):
                color = unreal.RenderingLibrary.read_render_target_raw_pixel(world, target, x, y)
                pixel_samples[label] = [round(float(color.r), 6), round(float(color.g), 6), round(float(color.b), 6), round(float(color.a), 6)]
        hidden_components = []
        try:
            for component in capture.get_editor_property("hidden_components"):
                owner = component.get_owner() if component else None
                hidden_components.append({
                    "component": "" if component is None else str(component.get_name()),
                    "actor": "" if owner is None else str(owner.get_actor_label()),
                })
        except Exception as exc:
            hidden_components = [{"error": str(exc)}]
        captures.append({
            "name": str(capture.get_name()),
            "visible": bool(capture.is_visible()),
            "capture_every_frame": bool(capture.get_editor_property("capture_every_frame")),
            "capture_on_movement": bool(capture.get_editor_property("capture_on_movement")),
            "capture_source": str(capture.get_editor_property("capture_source")),
            "primitive_render_mode": str(capture.get_editor_property("primitive_render_mode")),
            "texture_target": _path(target),
            "target_size": [] if target is None else [int(target.get_editor_property("size_x")), int(target.get_editor_property("size_y"))],
            "show_only_actor_count": len(capture.get_editor_property("show_only_actors")),
            "hidden_actor_count": len(capture.get_editor_property("hidden_actors")),
            "location": [round(float(v), 2) for v in capture.get_world_location().to_tuple()],
            "rotation": [round(float(v), 2) for v in capture.get_world_rotation().to_tuple()],
            "fov": round(float(capture.get_editor_property("fov_angle")), 2),
            "pixel_samples": pixel_samples,
            "hidden_components": hidden_components,
        })

    start = camera.get_camera_location()
    end = pawn.get_actor_location() + unreal.Vector(0.0, 0.0, 40.0)
    hit = unreal.SystemLibrary.sphere_trace_single(
        world, start, end, 36.0,
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [pawn],
        unreal.DrawDebugTrace.NONE, True,
    )
    payload = {} if hit is None else hit.to_dict()
    hit_actor = payload.get("actor") or payload.get("hit_actor")
    hit_component = payload.get("component") or payload.get("hit_component")
    report = {
        "world": _path(world.get_outermost()),
        "pawn": _path(pawn.get_class()),
        "pawn_location": [round(float(v), 2) for v in pawn.get_actor_location().to_tuple()],
        "camera_location": [round(float(v), 2) for v in start.to_tuple()],
        "camera_manager_fov": round(float(camera.get_fov_angle()), 3),
        "viewport_size": [int(viewport_size[0]), int(viewport_size[1])],
        "pawn_cameras": pawn_cameras,
        "trace_blocked": bool(payload),
        "blocking_actor": "" if hit_actor is None else str(hit_actor.get_actor_label()),
        "blocking_component": "" if hit_component is None else str(hit_component.get_name()),
        "town_runtime_ensured": "--ensure-town" in sys.argv[1:],
        "visuals": visuals,
        "captures": captures,
    }
    print(json.dumps(report, ensure_ascii=False))
    return report


if __name__ == "__main__":
    main()
