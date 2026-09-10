"""Save current player progress and editor packages before a packaging restart."""
from __future__ import annotations

import datetime
import json

import unreal
import gamexxk_probe_training_visual_mvp as base


world, controller, workbench = base._controller_and_widget()
result = {"player_saved": None, "saved_directory": unreal.Paths.project_saved_dir()}
if world:
    instance = unreal.GameplayStatics.get_game_instance(world)
    dev = next(obj for obj in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem)
               if obj.get_outer() == instance)
    mvp = next(obj for obj in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem)
               if obj.get_outer() == instance)
    assert not dev.is_session_active(), "An active Dev experiment must be restored by its owner first"
    name = "before-packaging-" + datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    snapshot = json.loads(dev.execute_json(json.dumps({"command": "snapshot.save", "args": {"name": name}})))
    assert snapshot.get("ok"), snapshot
    assert mvp.save_current_game(), "Player save failed; leave the editor open"
    result.update(player_saved=True, map=world.get_name(), snapshot=name)

assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True), "Editor package save failed"
dirty = (list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
         + list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()))
assert not dirty, "Dirty packages remain; leave the editor open"
result["dirty_packages"] = []
print(json.dumps(result, ensure_ascii=False))
