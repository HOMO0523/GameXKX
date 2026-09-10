import json
import unreal
import gamexxk_probe_training_visual_mvp as base

world, controller, workbench = base._controller_and_widget()
assert world, "Expected the user's active desktop PIE"
instance = unreal.GameplayStatics.get_game_instance(world)
dev = next(obj for obj in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if obj.get_outer() == instance)
mvp = next(obj for obj in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if obj.get_outer() == instance)
assert not dev.is_session_active(), "Do not overwrite real progress with an experimental session"
snapshot = json.loads(dev.execute_json(json.dumps({"command": "snapshot.save", "args": {"name": "user-before-hunt-build-20260910"}})))
assert snapshot.get("ok"), snapshot
saved = mvp.save_current_game()
assert saved, "Player save failed; editor must remain open"
print(json.dumps({"player_saved": bool(saved), "snapshot": snapshot, "map": world.get_name()}, ensure_ascii=False))
