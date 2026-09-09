"""Observe production ultimate/impact ordering during a task completion."""
import builtins,json,time
from pathlib import Path
import unreal
import gamexxk_probe_real_play_flow as base
world=base._get_game_world();pc=base._first_player_controller(world);board=pc.get_battle_board_widget_for_test()
tree=unreal.find_object(board,'BattleBoardWidgetTree') or unreal.find_object(board,'WidgetTree')
impact=unreal.find_object(tree,'BattleCinematicImpact')
itree=unreal.find_object(impact,'BattleUnitVisualWidgetTree') or unreal.find_object(impact,'WidgetTree')
image=unreal.find_object(itree,'BattleUnitAtlasImage');mid=image.get_dynamic_material()
out=Path(unreal.Paths.project_dir())/'Saved/Codex/partner-task-lightning-live.json'
state={'start':time.monotonic(),'rows':[],'handle':None,'last':None}
def tick(delta):
    elapsed=time.monotonic()-state['start']
    if elapsed>45:
        unreal.unregister_slate_post_tick_callback(state['handle'])
        out.write_text(json.dumps(state['rows'],indent=2),encoding='utf-8')
        return
    texture=mid.get_texture_parameter_value('AtlasTexture')
    ultimate=unreal.find_object(tree,'BattleLightningUltimate')
    ultimate_mid=ultimate.get_dynamic_material() if ultimate else None
    ultimate_texture=ultimate_mid.get_texture_parameter_value('AtlasTexture') if ultimate_mid else None
    row={'impact':texture.get_path_name() if texture else '', 'visible':str(impact.get_visibility()),'frame':mid.get_scalar_parameter_value('FrameColumn'),'ultimate_visible':str(ultimate.get_visibility()) if ultimate else 'absent','ultimate_texture':ultimate_texture.get_path_name() if ultimate_texture else ''}
    if row!=state['last']:
        state['rows'].append(dict(time=elapsed,**row));state['last']=row
state['handle']=unreal.register_slate_post_tick_callback(tick)
builtins._partner_task_lightning_record=state
