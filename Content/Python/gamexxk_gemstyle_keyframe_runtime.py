"""Enter an isolated Dev training fight and inspect the actual UMG atlas players."""
import json
import sys
import time
from pathlib import Path
import unreal

root=Path(str(unreal.Paths.project_dir())).resolve()
out=root/'Saved/Diagnostics/GemstyleKeyframePilot-20260910'
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world=editor.get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_path_name(),'Use the canonical pure-2D desktop PIE'
pc=unreal.GameplayStatics.get_player_controller(world,0)
instance=unreal.GameplayStatics.get_game_instance(world)
dev=next(obj for obj in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if obj.get_outer()==instance)
mode=sys.argv[1] if len(sys.argv)>1 else 'observe'

def command(name,args=None):
    result=json.loads(dev.execute_json(json.dumps({'command':name,'args':args or {}},ensure_ascii=False)))
    if not result.get('ok'):raise RuntimeError(json.dumps(result,ensure_ascii=False))
    return result

def observe(include_runtime=True):
    board=pc.get_battle_board_widget_for_test()
    library=unreal.get_default_object(unreal.load_class(None,'/Script/UMG.WidgetBlueprintLibrary'))
    cls=unreal.load_class(None,'/Script/GameXXK.GameXXKBattleUnitVisualWidget')
    rows=[]
    images=[im for im in unreal.ObjectIterator(unreal.Image) if im.get_name()=='BattleUnitAtlasImage']
    for widget in library.get_all_widgets_of_class(world,cls,False):
        image=next((im for im in images if im.get_outer() and im.get_outer().get_outer()==widget),None)
        if not image:continue
        mat=image.get_editor_property('brush').get_editor_property('resource_object')
        tex=mat.get_texture_parameter_value('AtlasTexture') if mat else None
        if not tex or not mat:continue
        column=float(mat.get_scalar_parameter_value('FrameColumn'));row=float(mat.get_scalar_parameter_value('FrameRow'))
        rows.append({'widget':widget.get_path_name(),'texture':tex.get_path_name(),'frameColumn':column,'frameRow':row,'frameIndex':int(row)*8+int(column),'visibility':str(widget.get_visibility()),'opacity':widget.get_render_opacity()})
    if not include_runtime:return {'visuals':rows}
    mvp=next(obj for obj in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if obj.get_outer()==instance)
    battle=mvp.get_runtime_state_copy().card_run.active_battle
    from gamexxk_probe_party_deck_runtime import _instance_summary
    units=[{'id':str(u.unit_id),'definition':str(u.enemy_definition_id),'hp':u.hp} for u in battle.units]
    hand=[_instance_summary(c) for c in battle.deck.hand]
    return {'world':world.get_path_name(),'board':board.get_path_name() if board else None,'boardVisible':bool(pc.has_battle_board_widget_in_viewport_for_test()),'devSession':bool(dev.is_session_active()),'visuals':rows,'units':units,'hand':hand}

report={'mode':mode}
if mode=='activate':
    if dev.is_session_active():raise RuntimeError('An existing Dev session owns the editor; coordinate before replacing it')
    report['begin']=command('session.begin')
    unreal.SystemLibrary.execute_console_command(world,'GameXXK.BattleAnimation.GemstyleKeyframes 1')
    report['battle']=command('battle.start',{'stage':'Training.Normal.1-1','encounter':1,'seed':20260910})
    report['auto']=command('battle.auto',{'enabled':False})
elif mode=='end-turn':
    board=pc.get_battle_board_widget_for_test()
    report['endTurnMethods']=[n for n in dir(board) if 'end_turn' in n]
elif mode=='hero-attack':
    snapshot=observe()
    target=next(u['id'] for u in snapshot['units'] if 'Rooster' in u['definition'])
    card=next(c for c in snapshot['hand'] if c['card_id']=='Hero.Generic.HeYuZhan')
    board=pc.get_battle_board_widget_for_test()
    clicked=bool(board.click_card_in_hand(unreal.Name(card['instance_id'])))
    targeting=bool(board.is_card_targeting_active())
    confirmed=bool(board.confirm_targeting_unit(unreal.Name(target))) if targeting else clicked
    report['attack']={'clicked':clicked,'targeting':targeting,'confirmed':confirmed,'card':card,'target':target}
elif mode=='capture':
    start=time.monotonic();samples=[];handle=[None];last=[-1.0]
    def tick(delta):
        elapsed=time.monotonic()-start
        if elapsed-last[0]<.04:return
        last[0]=elapsed
        try:
            state=observe(False)
            samples.append({'elapsed':elapsed,'visuals':state['visuals']})
        except Exception as error:
            samples.append({'elapsed':elapsed,'error':str(error)})
        if elapsed>=8:
            unreal.unregister_slate_post_tick_callback(handle[0])
            (out/'playback-samples.json').write_text(json.dumps(samples,ensure_ascii=False,indent=2),encoding='utf-8')
    handle[0]=unreal.register_slate_post_tick_callback(tick)
    report['capture']='8 second non-blocking frame/material capture started'
elif mode=='restore':
    report['restore']=command('session.restore')
    unreal.SystemLibrary.execute_console_command(world,'GameXXK.BattleAnimation.GemstyleKeyframes 0')
    mvp=next(obj for obj in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if obj.get_outer()==instance)
    report['playerSaved']=bool(mvp.save_current_game())
report.update(observe());out.mkdir(parents=True,exist_ok=True)
(out/(mode+'.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False))
