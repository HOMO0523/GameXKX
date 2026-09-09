"""Preserve current legitimate progress in a new recovery slot before the HP fix build."""
import datetime
import hashlib
import json
from pathlib import Path
import unreal
root=Path(__file__).resolve().parents[2]
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
instance=unreal.GameplayStatics.get_game_instance(world)
mvp=next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer()==instance)
dev=next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer()==instance)
export=json.loads(dev.execute_json(json.dumps({'schema':1,'command':'snapshot.export','args':{}})))
assert export['ok'] and not export['session_active'],export
state=mvp.get_runtime_state_copy()
assert not state.training.challenge_active and not state.card_run.has_active_card_battle
slot='GameXXK_StoryEntry_Recovery_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
assert not unreal.GameplayStatics.does_save_game_exist(slot,0)
save=unreal.GameplayStatics.create_save_game_object(unreal.GameXXKSaveGame)
payload=unreal.GameXXKMVPRules.make_save_state(state)
save.set_editor_property('save_state',payload)
assert save.get_editor_property('save_state').runtime_state.player_hp==state.player_hp
assert save.get_editor_property('save_state').runtime_state.player_gold==state.player_gold
assert unreal.GameplayStatics.save_game_to_slot(save,slot,0)
reloaded=unreal.GameplayStatics.load_game_from_slot(slot,0)
copy=reloaded.get_editor_property('save_state').runtime_state
assert copy.player_gold==state.player_gold and copy.player_hp==state.player_hp and copy.player_level==state.player_level
folder=root/'Saved/StorySystem/TaskEntryBug';folder.mkdir(parents=True,exist_ok=True)
(folder/'prebuild-live-snapshot.json').write_text(json.dumps(export,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
report={'slot':slot,'user_index':0,'world':world.get_name(),'saved_dir':unreal.Paths.project_saved_dir(),'gold':state.player_gold,'level':state.player_level,'xp':state.player_xp,'hp':state.player_hp,'base_max_hp':state.player_max_hp,'is_extra_recovery_slot':True,'original_slots_overwritten':False,'load_after_resource_fix_only':True}
(folder/'recovery-slot.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,ensure_ascii=False))
