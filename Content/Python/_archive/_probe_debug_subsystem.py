
import unreal, json
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = editor.get_game_world()
out = {"world": str(world)}
try:
    gi = world.get_game_instance()
    out["gi_attr"] = str(gi)
except Exception as e:
    out["gi_attr_err"] = str(e)[:200]
try:
    gi2 = unreal.GameplayStatics.get_game_instance(world)
    out["gi_static"] = str(gi2)
    out["has_cls"] = hasattr(unreal, "GameXXKMVPSubsystem")
    sub = gi2.get_subsystem(unreal.GameXXKMVPSubsystem)
    out["sub"] = str(sub)
except Exception as e:
    out["gi_static_err"] = str(e)[:200]
print(json.dumps(out, ensure_ascii=False))
