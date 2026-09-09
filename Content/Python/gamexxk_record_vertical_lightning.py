"""Observe the actual impact widget during a real card play, without driving it."""
import builtins,json,time
from pathlib import Path
import unreal
import gamexxk_probe_real_play_flow as base
world=base._get_game_world();pc=base._first_player_controller(world);board=pc.get_battle_board_widget_for_test()
tree=unreal.find_object(board,'BattleBoardWidgetTree') or unreal.find_object(board,'WidgetTree')
impact=unreal.find_object(tree,'BattleCinematicImpact')
itree=unreal.find_object(impact,'BattleUnitVisualWidgetTree') or unreal.find_object(impact,'WidgetTree')
image=unreal.find_object(itree,'BattleUnitAtlasImage')
mid=image.get_dynamic_material()
print([n for n in dir(mid) if 'parameter_value' in n])
out=Path(unreal.Paths.project_dir())/'Saved/Codex/vertical-lightning-live.json'
state={'start':time.monotonic(),'rows':[],'handle':None,'last':None}
def tick(delta):
    elapsed=time.monotonic()-state['start']
    if elapsed>20:
        unreal.unregister_slate_post_tick_callback(state['handle'])
        out.write_text(json.dumps(state['rows'],indent=2),encoding='utf-8')
        return
    texture=mid.get_texture_parameter_value('AtlasTexture')
    row={'texture':texture.get_path_name() if texture else '', 'visible':str(impact.get_visibility()),'column':mid.get_scalar_parameter_value('FrameColumn'),'row':mid.get_scalar_parameter_value('FrameRow')}
    if row!=state['last']:
        state['rows'].append(dict(time=elapsed,**row));state['last']=row
state['handle']=unreal.register_slate_post_tick_callback(tick)
builtins._vertical_lightning_record=state
