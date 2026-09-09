"""Read real Slate-frame card transforms without advancing or changing gameplay."""
import builtins
import json
from pathlib import Path
import re
import sys
import time
import unreal
import gamexxk_probe_real_play_flow as base

name=sys.argv[1] if len(sys.argv)>1 else 'motion'
if not re.fullmatch(r'[A-Za-z0-9_-]+',name):raise ValueError('Use a simple evidence name')
prior=getattr(builtins,'_gamexxk_motion_trace',None)
if prior and prior.get('handle') is not None:unreal.unregister_slate_post_tick_callback(prior['handle'])
output=Path(unreal.Paths.project_dir())/'Saved/Codex/CardEffects-20260907'/f'{name}-trace.json'
state={'start':time.perf_counter(),'frames':[],'handle':None}
builtins._gamexxk_motion_trace=state

def sample(widget):
    t=widget.get_editor_property('render_transform')
    return {'name':widget.get_name(),'visibility':str(widget.get_visibility()),'opacity':widget.get_render_opacity(),
            'offset':[t.translation.x,t.translation.y],'scale':[t.scale.x,t.scale.y],'angle':t.angle}

def tick(dt):
    age=time.perf_counter()-state['start']
    frame={'time':age,'dt':dt,'hand':[],'reward':[]}
    world=base._get_game_world();pc=base._first_player_controller(world) if world else None
    board=pc.get_battle_board_widget_for_test() if pc else None
    try:
        if board:
            first=board.get_hand_card_button_for_test(0)
            tree=first.get_outer() if first else None
            for index in range(5):
                w=board.get_hand_card_button_for_test(index)
                if w:frame['hand'].append(sample(w))
            for index in range(3):
                w=unreal.find_object(tree,f'BattleRewardCard_{index:02d}')
                if w:frame['reward'].append(sample(w))
            w=unreal.find_object(tree,'BattleEnemyIntentShowcaseCard')
            if w:frame['intent']=sample(w)
        state['frames'].append(frame)
    except Exception as error:
        state['error']=str(error);age=7
    if age>=6:
        unreal.unregister_slate_post_tick_callback(state['handle']);state['handle']=None
        output.parent.mkdir(parents=True,exist_ok=True)
        output.write_text(json.dumps({'frames':state['frames'],'error':state.get('error')},ensure_ascii=False,indent=2),encoding='utf-8')

state['handle']=unreal.register_slate_post_tick_callback(tick)
print(json.dumps({'started':name,'path':str(output)}))
