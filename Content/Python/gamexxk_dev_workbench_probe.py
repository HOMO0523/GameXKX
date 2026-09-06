"""Editor MCP bridge to the same runtime Dev commands used by Development builds."""
import json
import sys

import unreal
import gamexxk_probe_training_visual_mvp as base

world, controller, workbench = base._controller_and_widget()
if not world:
    raise RuntimeError("Start the canonical desktop PIE first")
instance = unreal.GameplayStatics.get_game_instance(world)
dev = next((obj for obj in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem)
            if obj.get_outer() == instance), None)
if not dev:
    raise RuntimeError("The runtime Dev subsystem is not available in this build")
mode = sys.argv[1] if len(sys.argv) > 1 else "state"
if mode == "execute":
    result = json.loads(dev.execute_json(sys.argv[2]))
elif mode == "toggle":
    dev.toggle_panel()
    result = {"ok": True, "panel_open": dev.is_panel_open()}
elif mode == "travel_visuals":
    images = []
    names = ["TravelHeroAnimatedUnit"] + [f"TravelEnemyAnimatedUnit_{i}" for i in range(3)]
    names += [f"TravelCompanionAnimatedUnit_{i}" for i in range(2)]
    for widget in unreal.ObjectIterator(unreal.Image):
        name = widget.get_name()
        if name not in names:
            continue
        owner = widget.get_outer()
        while owner and owner != workbench:
            owner = owner.get_outer()
        if owner != workbench:
            continue
        brush = widget.get_editor_property("brush")
        resource = brush.get_editor_property("resource_object")
        transform = widget.get_editor_property("render_transform")
        images.append({"name": name, "visibility": str(widget.get_visibility()),
                       "opacity": widget.get_render_opacity(),
                       "resource": resource.get_path_name() if resource else None,
                       "scale": [transform.scale.x, transform.scale.y]})
    result = {"ok": True, "images": images,
              "travel": base._travel_runtime_snapshot(workbench),
              "visual_phase": workbench.get_travel_visual_phase_name_for_test()}
else:
    result = {"ok": True, "panel_open": dev.is_panel_open(),
              "session_active": dev.is_session_active(), "world": world.get_name()}
print(json.dumps(result, ensure_ascii=False))
