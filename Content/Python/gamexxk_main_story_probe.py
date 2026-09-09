"""Focused live-PIE observations and public story actions; invoked through project UE MCP."""
import json
from pathlib import Path
import runpy
import sys
import traceback
import unreal

ROOT=Path(__file__).resolve().parents[2]
args=sys.argv[1:]
mode=args[0] if args else 'observe'

def run():
    if mode=='import-art':
        runpy.run_path(str(ROOT/'Content/Python/gamexxk_import_main_story_art.py'),run_name='__main__')
        return {'ok':True}
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world:
        return {'ok':False,'reason':'no PIE world','saved_dir':unreal.Paths.project_saved_dir()}
    assert 'L_DesktopTrainingHUD' in world.get_name(), world.get_name()
    instance=unreal.GameplayStatics.get_game_instance(world)
    story=next((x for x in unreal.ObjectIterator(unreal.GameXXKMainStorySubsystem) if x.get_outer()==instance),None)
    mvp=next((x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer()==instance),None)
    controller=unreal.GameplayStatics.get_player_controller(world,0)
    workbench=controller.get_desktop_training_workbench_widget_for_test()
    if mode=='open-backpack':
        workbench.open_backpack()
    elif mode=='start':
        story.start_task(unreal.Name(args[1]))
    elif mode=='show-dialogue':
        workbench.open_backpack()
        workbench.handle_desktop_action_for_test(2100)
        story.start_task(unreal.Name(args[1]))
    elif mode=='dialogue-to-result':
        catalog=json.loads((ROOT/'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))
        node=next(n for c in catalog['chapters'] for n in c['nodes'] if n['id']==args[1])
        assert node['kind']=='Dialogue','This probe only advances real dialogue actions'
        assert story.start_task(unreal.Name(args[1]))
        for _ in node['lines']:
            if json.loads(story.get_progress_json())['phase']==7:break
            assert story.advance_dialogue()
    elif mode=='dialogue-to-choice':
        catalog=json.loads((ROOT/'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))
        node=next(n for c in catalog['chapters'] for n in c['nodes'] if n['id']==args[1])
        assert node['kind']=='Investigation'
        assert story.start_task(unreal.Name(args[1]))
        for _ in node['lines']:
            if json.loads(story.get_progress_json())['phase']==2:break
            assert story.advance_dialogue()
    elif mode=='next':
        story.advance_dialogue()
    elif mode=='answer':
        story.choose_answer(int(args[1]))
    elif mode=='hint':
        story.reveal_hint()
    elif mode=='claim':
        story.claim_reward(unreal.Name(args[1]))
    elif mode=='chapter':
        story.open_chapter(unreal.Name(args[1]))
    elif mode=='pause':
        story.pause_activity()
    elif mode=='tree':
        story.open_journey_tree()
    elif mode=='battle':
        story.begin_task_battle()
    elif mode=='ui-action':
        workbench.handle_desktop_action_for_test(int(args[1]))
    elif mode=='auto-battle':
        mvp.set_training_challenge_auto_battle(True)
    elif mode=='gc':
        unreal.SystemLibrary.collect_garbage()
    elif mode=='talent-purchase':
        mvp.purchase_talent_node(unreal.Name(args[1]))
    state=mvp.get_runtime_state_copy()
    output={'ok':True,'world':world.get_name(),'saved_dir':unreal.Paths.project_saved_dir(),
            'story':json.loads(story.get_progress_json()),'gold':state.player_gold,
            'screen':str(state.screen),'route_active':state.training.challenge_active,
            'card_battle':state.card_run.has_active_card_battle,
            'stage':str(state.training.active_challenge_stage_id),
            'reachable':list(state.reachable_route_node_ids),'visited':list(state.visited_route_node_ids)}
    output['line_index']=state.narrative_progress.main_story.line_index
    output['layout_builds']=workbench.get_programmatic_layout_build_count_for_test_blueprint() if workbench else None
    if workbench:
        output['desktop_page']=str(workbench.get_active_center_page_for_test())
        output['backpack_open']=workbench.is_backpack_expanded_for_test()
        output['settings_open']=workbench.is_settings_panel_open_for_test()
    output['hp']=[state.player_hp,state.player_max_hp]
    output['chest_count']=len(state.training.owned_chest_tokens)
    output['talent_ranks']={str(k):v for k,v in state.talents.node_ranks.items()}
    output['relics']=[str(r.relic_id) for r in state.card_run.relics]
    output['formation']=[str(m.member_id) for m in state.card_run.ordered_formation.members]
    if state.card_run.has_active_card_battle:
        output['battle_phase']=str(state.card_run.active_battle.phase)
        output['battle_round']=state.card_run.active_battle.round_number
        output['battle_units']=[{'id':str(u.unit_id),'hp':u.hp,'max_hp':u.max_hp} for u in state.card_run.active_battle.units]
    import gamexxk_probe_real_play_flow as geometry_probe
    widgets=[]
    for widget in unreal.ObjectIterator(unreal.Widget):
        name=widget.get_name()
        if not (name.startswith(('Story','MainStory','Travel','RouteClose','TaskAvailable','TaskReward'))):
            continue
        # Discard widgets from dead worlds or another game instance.
        owner=widget
        valid=False
        while owner:
            if owner==workbench or (isinstance(owner,unreal.UserWidget) and owner.get_owning_player()==controller):
                valid=True;break
            owner=owner.get_outer()
        if not valid:
            continue
        rect=geometry_probe._screen_rect(world,widget)
        if not rect:
            continue
        row={'name':name,'class':widget.get_class().get_name(),'rect':rect,'visibility':str(widget.get_visibility())}
        if isinstance(widget,unreal.TextBlock):row['text']=str(widget.get_text())
        if isinstance(widget,unreal.Button):row['enabled']=widget.get_is_enabled()
        widgets.append(row)
    output['widgets']=widgets
    return output

try:
    print(json.dumps(run(),ensure_ascii=False))
except Exception:
    error=traceback.format_exc()
    (ROOT/'Saved/StorySystem/mcp-probe-error.txt').write_text(error,encoding='utf-8')
    print(json.dumps({'ok':False,'error':error},ensure_ascii=False))
