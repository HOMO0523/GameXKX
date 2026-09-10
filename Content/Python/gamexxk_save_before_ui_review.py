"""Preserve the current player session before the coordinated UI cold build."""
import json
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base

world, controller, workbench = base._controller_and_widget()
assert world, 'Expected the live desktop player session'
instance = unreal.GameplayStatics.get_game_instance(world)
dev = next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer() == instance)
mvp = next(o for o in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if o.get_outer() == instance)
assert not dev.is_session_active(), 'Do not save a temporary experiment over the player'
assert mvp.save_current_game(), 'Player save failed; keep the editor open'
result = {'player_saved': True, 'dev_session': False, 'map': world.get_name()}
output = Path(unreal.Paths.project_saved_dir()) / 'Codex/UIGuidanceLocalization-20260910'
(output / 'player-before-central-settings.json').write_text(json.dumps(result), encoding='utf-8')
print(json.dumps(result))
