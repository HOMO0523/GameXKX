"""Remove only the retired preview actors, preserving the rest of the authored town."""
import json
import unreal

MAP = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
DEFAULT = "/Game/GameXXK/Maps/L_DesktopTrainingHUD"
CLASSES = {"GameXXKPrologueCarriageRig", "GameXXKPrologueAftermathController"}

def main():
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    if not editor.load_level(MAP):
        raise RuntimeError("Cannot load the authored preview map for cleanup")
    removed = []
    for actor in actors.get_all_level_actors():
        if actor.get_class().get_name() in CLASSES:
            removed.append(actor.get_path_name())
            if not actors.destroy_actor(actor):
                raise RuntimeError("Could not remove retired actor")
    if removed and not editor.save_current_level():
        raise RuntimeError("Could not save cleaned map")
    if not editor.load_level(DEFAULT):
        raise RuntimeError("Could not return to desktop HUD map")
    print(json.dumps({"removed": removed, "returned_to": DEFAULT}, ensure_ascii=False))

main()
