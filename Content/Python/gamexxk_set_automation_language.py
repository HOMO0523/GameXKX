"""Non-persistent language baseline for an isolated automation editor."""
import json
import sys
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert not world or 'UEDPIE_' not in world.get_path_name(), 'Never change a player PIE for automation'
language=sys.argv[1] if len(sys.argv)>1 else 'zh-Hans'
assert unreal.GameXXKLocalizationLibrary.set_language(language,False)
print(json.dumps({'language':unreal.GameXXKLocalizationLibrary.get_language(),'persisted':False}))
