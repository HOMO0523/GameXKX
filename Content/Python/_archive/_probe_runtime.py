import unreal
sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
# Find the MVP subsystem via the game instance / world
from unreal import GameplayStatics
world = sub.get_game_world() or sub.get_editor_world()
out = {}
if world:
    gi = world.get_game_instance()
    out['world'] = str(world.get_name())
    # Try to find runtime state via a Python-side accessor is not directly available;
    # instead log the PIE state.
out['pie'] = 'game_world' if sub.get_game_world() else 'no_pie'
import json
print(json.dumps(out, ensure_ascii=False))
