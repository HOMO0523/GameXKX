"""Temporarily keep the editor responsive during route animation verification."""
import json
import sys
import unreal

settings = unreal.load_object(None, "/Script/UnrealEd.Default__EditorPerformanceSettings")
assert settings, "Editor performance settings were not found"
key = "bThrottleCPUWhenNotForeground"
mode = sys.argv[1] if len(sys.argv) > 1 else "begin"
if mode == "begin":
    if not hasattr(unreal, "_gamexxk_route_art_throttle_before"):
        unreal._gamexxk_route_art_throttle_before = settings.get_editor_property(key)
    settings.set_editor_property(key, False)
else:
    if hasattr(unreal, "_gamexxk_route_art_throttle_before"):
        settings.set_editor_property(key, unreal._gamexxk_route_art_throttle_before)
        del unreal._gamexxk_route_art_throttle_before
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    allow_slate = bool(settings.get_editor_property("bAllowSlateThrottling"))
    unreal.SystemLibrary.execute_console_command(world, "Slate.bAllowThrottling " + str(int(allow_slate)))
print(json.dumps({"mode": mode, "throttle": settings.get_editor_property(key)}))
