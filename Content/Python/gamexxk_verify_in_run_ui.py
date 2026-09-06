"""Current route UI capture setup. Uses public, non-saving fixture entrypoints."""
import json
import sys
from pathlib import Path
from datetime import datetime
import unreal

ROOT=Path(unreal.Paths.get_project_file_path()).parent
OUT=ROOT/'Saved'/'Codex'/'RouteUI-20260906'/'live-after'
OUT.mkdir(parents=True,exist_ok=True)
def prop(obj,name):
    try:return obj.get_editor_property(name)
    except Exception:
        try:return getattr(obj,name,None)
        except Exception:return None
def main():
    mode=sys.argv[1] if len(sys.argv)>1 else 'state'
    editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world=editor.get_game_world()
    ew=editor.get_editor_world()
    report={'time':datetime.now().astimezone().isoformat(),'mode':mode,'saved_dir':unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),'editor_map':ew.get_path_name() if ew else None,'game_map':world.get_path_name() if world else None}
    if mode=='assets':
        assets=['/Game/GameXXK/UI/MasterV2/Approved/'+n for n in ('T_MasterV2_PanelLarge','T_MasterV2_CardFrame','T_MasterV2_ItemSlot','T_MasterV2_CloseInk','T_MasterV2_Ingot','T_MasterV2_SquareSelected')]
        assets+=['/Game/GameXXK/UI/Relics/Icons/T_Relic_'+n for n in ('JadeBell','CraneFeather','IronKnot','RainCape','DrumCharm','CandleStub','MedicineGourd','AncientCoin')]
        assets+=['/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Hero','/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Blade','/Game/GameXXK/UI/MainMenu/Textures/T_InkButtonBase','/Game/GameXXK/UI/Items/T_Item_TrainingNormalChest']
        assets+=['/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Plain_GeneratedV2','/Game/GameXXK/UI/Battle/Textures/T_BattleArena_WaterShore_GeneratedV2']
        dest=OUT/'assets';dest.mkdir(exist_ok=True)
        rows=[]
        for path in assets:
            asset=unreal.load_asset(path)
            r={'asset':path,'loaded':bool(asset)}
            if asset:
                task=unreal.AssetExportTask();task.object=asset;task.filename=str(dest/(asset.get_name()+'.png'));task.automated=True;task.prompt=False;task.replace_identical=True
                r['exported']=bool(unreal.Exporter.run_asset_export_task(task));r['file']=task.filename
            rows.append(r)
        report['assets']=rows
        (OUT/'assets-state.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps(report,ensure_ascii=False));return
    if world:
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        report['viewport_size']=str(unreal.WidgetLayoutLibrary.get_viewport_size(world))
        wb=pc.get_desktop_training_workbench_widget_for_test()
        sub=wb.get_mvp_subsystem() if wb else None
        if not sub:
            for getter in ('get_route_merchant_widget_for_test','get_route_encounter_panel_widget_for_test','get_battle_board_widget_for_test','get_route_map_widget_for_test'):
                w=getattr(pc,getter)()
                if w:sub=w.get_mvp_subsystem()
                if sub:break
        if mode=='challenge':
            wb.handle_desktop_action_for_test(4)
            wb.handle_desktop_action_for_test(6)
        elif mode=='scale': wb.handle_desktop_action_for_test({50:651,75:656,100:650}[int(sys.argv[2])])
        elif mode=='save_receipt':report['result']=bool(sub.save_current_game('GameXXK_UISettlement_Verification',0))
        elif mode=='load_receipt':report['result']=bool(sub.load_game_from_slot('GameXXK_UISettlement_Verification',0))
        elif mode=='confirm_settlement':report['result']=bool(pc.get_training_settlement_widget_for_test().confirm_for_test())
        elif mode=='buy_relic_stock':
            stock=prop(prop(sub.get_runtime_state_copy(),'card_run'),'route_merchant')
            offers=list(prop(stock,'offers') or [])
            report['bought']=[bool(pc.get_route_merchant_widget_for_test().purchase_offer(prop(x,'offer_id'))) for x in offers if 'RELIC' in str(prop(x,'kind')) and not prop(x,'sold') and not prop(x,'unavailable')]
        elif mode=='refresh_merchant':report['result']=bool(pc.get_route_merchant_widget_for_test().refresh_stock())
        elif mode=='merchant':report['result']=str(sub.apply_route_merchant_acceptance_fixture_for_test(True))
        elif mode=='event':report['result']=str(sub.apply_route_encounter_acceptance_fixture_for_test(False))
        elif mode=='camp':report['result']=str(sub.apply_route_encounter_acceptance_fixture_for_test(True))
        elif mode=='clear':report['result']=str(sub.clear_route_encounter_acceptance_fixture_for_test())
        elif mode=='victory':report['result']=str(sub.resolve_battle_victory(False))
        elif mode=='finish_battle':
            for step in range(160):
                snapshot=sub.get_runtime_state_copy()
                pending=prop(prop(snapshot,'card_run'),'pending_reward')
                if list(prop(pending,'options') or []):break
                if str(prop(snapshot,'screen')).find('BATTLE')<0:break
                if not sub.resolve_battle_victory(False):break
            report['steps']=step
        elif mode=='skip_reward':report['result']=bool(pc.get_battle_board_widget_for_test().skip_pending_route_reward())
        elif mode=='leave_merchant':report['result']=bool(pc.get_route_merchant_widget_for_test().leave_merchant())
        elif mode=='choose_first':
            panel=pc.get_route_encounter_panel_widget_for_test()
            report['selected']=bool(panel.select_choice_for_test(0))
            report['result']=bool(panel.confirm_selected_choice_for_test())
        elif mode=='take_relic':
            opts=list(prop(prop(prop(sub.get_runtime_state_copy(),'card_run'),'pending_reward'),'options') or [])
            idx=next(i for i,x in enumerate(opts) if 'RELIC' in str(prop(x,'kind')))
            report['result']=bool(pc.get_battle_board_widget_for_test().choose_pending_battle_reward_option(idx,unreal.Name('')))
        elif mode=='camp_heal':report['result']=bool(pc.get_route_encounter_panel_widget_for_test().trigger_primary_action_for_test())
        elif mode=='select_node':report['result']=bool(sub.select_route_node_by_id(int(sys.argv[2])))
        elif mode=='battle':
            state=sub.get_runtime_state_copy()
            reachable=list(prop(state,'reachable_route_node_ids') or [])
            report['reachable']=[int(x) for x in reachable]
            if reachable:report['result']=str(sub.select_route_node_by_id(int(reachable[0])))
        pc.refresh_player_flow_widgets_for_test()
        state=sub.get_runtime_state_copy()
        run=prop(state,'card_run')
        reward=prop(run,'pending_reward')
        battle=prop(run,'active_battle');training=prop(state,'training')
        report['route_progress']=str(prop(run,'route_progress'));report['battle_phase']=str(prop(battle,'phase'));report['pending_choice']=str(prop(battle,'pending_choice'));report['challenge_active']=bool(prop(training,'challenge_active'));report['training_stage']=str(prop(training,'active_challenge_stage_id'));report['last_error']=str(sub.get_last_save_load_error())
        report['units']=[{'id':str(prop(u,'unit_id')),'hp':prop(u,'hp'),'max':prop(u,'max_hp')} for u in list(prop(battle,'units') or [])]
        report.update({'screen':str(prop(state,'screen')),'quest_state':str(prop(state,'quest_state')),'pending_node':str(prop(state,'pending_route_node_id')),'reward_options':str(prop(reward,'options'))})
        receipt=sub.get_pending_training_settlement_copy()
        bar=pc.get_relic_bar_widget_for_test()
        training_widget=pc.get_training_settlement_widget_for_test()
        report['settlement_widget_visible']=str(training_widget.get_visibility()) if training_widget else None
        report['travel_active']=bool(prop(training,'travel_active'))
        report['members']=[{'name':str(prop(m,'display_name')),'xp_gained':prop(m,'experience_gained'),'level_before':prop(m,'level_before'),'level_after':prop(m,'level_after')} for m in list(prop(receipt,'members') or [])]
        report.update({'medicine':int(unreal.GameXXKMVPRules.get_item_count(state,unreal.Name('Item.HealingPowder'))),'gold':int(prop(state,'player_gold')),'hp':int(prop(state,'player_hp')),'pending_settlement':bool(sub.has_pending_training_settlement()),'receipt_id':prop(receipt,'receipt_id').to_string(),'settlement_gold':int(prop(receipt,'gold') or 0),'relic_count':int(bar.get_rendered_relic_count_for_test()) if bar else 0,'relic_visible':str(bar.get_visibility()) if bar else None})
        if mode in ('route_nodes','skip_reward','finish_battle','state'):
            reachable=list(prop(state,'reachable_route_node_ids') or [])
            report['nodes']=[{'id':int(prop(n,'node_id')),'kind':str(prop(n,'node_kind')),'next':[int(x) for x in list(prop(n,'outgoing_node_ids') or [])],'reachable':prop(n,'node_id') in reachable} for n in list(prop(state,'route_map_nodes') or [])]
    report['dirty_maps']=[x.get_name() for x in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    report['dirty_content']=[x.get_name() for x in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    (OUT/(mode+'-state.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False))
main()
