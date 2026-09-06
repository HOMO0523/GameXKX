"""Read current workbench presentation and authoritative state; optional real UI actions only."""
import json
import sys
from pathlib import Path
from datetime import datetime
import unreal
import gamexxk_probe_training_visual_mvp as travel_probe

def get(obj, name, default=None):
    try:
        value=obj.get_editor_property(name)
        return default if value is None else value
    except Exception:
        try:return getattr(obj,name,default)
        except Exception:return default

def main():
    world,pc,wb=travel_probe._controller_and_widget()
    if not world or not wb:raise RuntimeError('A real desktop PIE world is required')
    mode=sys.argv[1] if len(sys.argv)>1 else 'observe'
    result=None
    if mode=='action':result=wb.handle_desktop_action_for_test(int(sys.argv[2]))
    elif mode=='open':result=wb.open_backpack()
    elif mode=='select_view':result=wb.select_backpack_character_for_test(unreal.Name(sys.argv[2]))
    elif mode=='equip_first':
        slot_index=wb.find_first_backpack_equipment_slot_for_test()
        result={'slot':slot_index,'equipped':wb.right_click_backpack_slot_for_test(slot_index) if slot_index>=0 else False}
    elif mode=='cancel_carry':result=wb.cancel_carried_item_for_test()
    elif mode=='tool_confirm':result=wb.confirm_tool_for_test()
    elif mode=='start_challenge':
        result=wb.select_stage_for_test(unreal.Name(sys.argv[2] if len(sys.argv)>2 else 'Training.Normal.1-2'))
        result=wb.click_challenge_for_test() if result else result
    elif mode=='start_travel':
        result=wb.select_stage_for_test(unreal.Name(sys.argv[2] if len(sys.argv)>2 else 'Training.Normal.1-1'))
        result=wb.click_travel_for_test() if result else result
    elif mode=='advance':result=wb.advance_travel_for_test(int(sys.argv[2]) if len(sys.argv)>2 else 1)
    sub=wb.get_mvp_subsystem();state=sub.get_runtime_state_copy();training=get(state,'training');runtime=sub.get_training_travel_runtime_copy()
    payload=travel_probe._snapshot(mode)
    payload.update(time=datetime.now().astimezone().isoformat(),action_result=str(result),widget_path=wb.get_path_name(),widget_visibility=str(wb.get_visibility()),encounter_kind=str(get(runtime,'encounter_kind')),stage_id=str(get(runtime,'stage_id')))
    payload['party_runtime']=[{k:str(get(u,k)) if k=='unit_id' else get(u,k) for k in ('unit_id','hp','max_hp','attack')} for u in list(get(runtime,'party_units',[]) or [])]
    payload['persistent_player']={k:get(state,k) for k in ('player_hp','player_max_hp','player_mp','player_max_mp','player_level','player_xp','player_gold')}
    payload['equipment_state']=str(get(state,'equipment_collection'))
    payload['talents']=str(get(state,'talents'))
    payload['backpack_items']=[str(x) for x in wb.get_visible_backpack_item_ids_for_test()]
    payload['held_chests']={'normal':sub.get_training_chest_count(unreal.GameXXKTrainingRewardTier.NORMAL_CHEST),'advanced':sub.get_training_chest_count(unreal.GameXXKTrainingRewardTier.ADVANCED_CHEST)}
    tree=get(wb,'widget_tree') or get(wb,'WidgetTree') or unreal.find_object(wb,'WidgetTree') or unreal.find_object(wb,'DesktopTrainingWorkbenchWidgetTree');payload['tree']=tree.get_path_name() if tree else None
    names=['DesktopInventoryNoticeText','BackpackGoldText','ToolProgressText','TrainingWaveStageText','TrainingWaveIndexText','TrainingWaveProgressFill','TrainingFoldedNormalChestText','TrainingFoldedAdvancedChestText','TrainingNormalChestCountText','TrainingAdvancedChestCountText','TrainingNormalChestButton','TrainingAdvancedChestButton','IdleStripFoldButton','TravelHeroAnimatedUnit','TravelHeroHealth','TravelEnemyHealth_0','TravelEnemyHealth_1','TravelEnemyHealth_2','TravelEnemyAnimatedUnit_0','TravelEnemyAnimatedUnit_1','TravelEnemyAnimatedUnit_2','TravelCompanionAnimatedUnit_0','TravelCompanionAnimatedUnit_1']
    embedded=unreal.find_object(tree,'EmbeddedApprovedBackpack') if tree else None
    payload['pending_deck']=[str(x) for x in (get(embedded,'pending_hero_deck_ids',[]) or [])]
    payload['hero_loadout']=[str(x) for x in (get(get(state,'card_run'),'hero_selected_card_ids',[]) or [])]
    widgets={}
    for name in names:
        obj=unreal.find_object(tree,name) if tree else None
        if not obj:continue
        row={'class':obj.get_class().get_name(),'visible':str(obj.get_visibility()),'opacity':obj.get_render_opacity(),'enabled':obj.get_is_enabled()}
        if isinstance(obj,unreal.TextBlock):row['text']=str(obj.get_text())
        if isinstance(obj,unreal.ProgressBar):row['percent']=get(obj,'percent')
        slot=get(obj,'slot')
        if slot and isinstance(slot,unreal.CanvasPanelSlot):row.update(position=str(slot.get_position()),size=str(slot.get_size()))
        row['transform']=str(get(obj,'render_transform'))
        if isinstance(obj,unreal.Image):
            try:row['properties']=json.loads(unreal.ToolsetLibrary.get_object_properties(obj,['Brush','RenderTransform']))
            except Exception as exc:row['properties_error']=str(exc)
        widgets[name]=row
    payload['widgets']=widgets
    out=Path(unreal.Paths.get_project_file_path()).resolve().parent/'Saved/Codex/FullPlayAudit-20260906'
    out.mkdir(exist_ok=True);(out/'last-state.json').write_text(json.dumps(payload,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(payload,ensure_ascii=False))
main()
