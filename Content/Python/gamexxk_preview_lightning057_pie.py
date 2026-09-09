"""Temporary real-PIE VFX preview. Does not change combat resolution."""
import bisect
import builtins
import json
from pathlib import Path
import time
import unreal
import gamexxk_probe_real_play_flow as base

world = base._get_game_world()
if not world:
    raise RuntimeError('PIE required')
pc = base._first_player_controller(world)
board = pc.get_battle_board_widget_for_test()
if not board or not board.is_visible():
    workbench = pc.get_desktop_training_workbench_widget_for_test()
    if not workbench.click_challenge_for_test():
        raise RuntimeError('Open an unlocked desktop challenge first')
    board = pc.get_battle_board_widget_for_test()
if not board:
    route = pc.get_route_map_widget_for_test()
    if route:
        route.call_method('HandleNodeButton0Clicked')
    board = pc.get_battle_board_widget_for_test()
    if not board:
        raise RuntimeError('Battle is opening; run preview again after route transition')
previous = getattr(builtins, '_lightning057_preview', None)
if previous:
    previous['stop']()
was_auto = board.is_auto_battle_enabled()
board.set_auto_battle_enabled(False)
tree = unreal.find_object(board, 'BattleBoardWidgetTree') or unreal.find_object(board, 'WidgetTree')
root = unreal.find_object(tree, 'GameXXKBattleViewportRoot')
if not isinstance(root, unreal.CanvasPanel):
    raise RuntimeError('Battle viewport canvas missing')
image = unreal.new_object(unreal.Image, outer=board)
image.set_render_opacity(0.0)
image.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
slot = root.add_child_to_canvas(image)
slot.set_anchors(unreal.Anchors(minimum=unreal.Vector2D(0,0), maximum=unreal.Vector2D(1,1)))
slot.set_offsets(unreal.Margin(0,0,0,0))
slot.set_z_order(1000)
asset_root = '/Game/GameXXK/UI/Battle/VFX/UltimateLightning057/'
image.set_brush_from_material(unreal.load_asset(asset_root+'MI_UltimateLightning057_Canonical'))
mid = image.get_dynamic_material()
mid.set_scalar_parameter_value('FlipX', 1)
mid.set_scalar_parameter_value('EdgeFeather', .09)
mid.set_scalar_parameter_value('OffsetX', .12)
mid.set_scalar_parameter_value('CloudCompact', 1)
mid.set_scalar_parameter_value('FrameScale', .9)
textures = [unreal.load_asset(asset_root+f'T_UltimateLightning057_Atlas_{i:02d}') for i in range(1,8)]
for i in range(3,7):
    textures[i] = unreal.load_asset(asset_root+f'T_UltimateLightning057_Atlas_{i+1:02d}_Unified')
textures.append(unreal.load_asset(asset_root+'T_UltimateLightning057_TransitionV3'))
manifest = json.loads((Path(unreal.Paths.project_dir())/'SourceArt/UI/Battle/VFX/UltimateLightning057TransitionV3/sequence.json').read_text(encoding='utf-8'))
frames = manifest['frames']
ends = []
total = 0
for frame in frames:
    total += frame['play_seconds']
    ends.append(total)
# Give imported atlas resources time to initialize before exposing the full-screen brush.
state = {'image':image, 'mid':mid, 'textures':textures, 'start':time.monotonic()+1.0, 'last':-1, 'handle':None, 'stopped':False}
def stop():
    if state['stopped']:
        return
    state['stopped'] = True
    if state['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(state['handle'])
    try:
        image.remove_from_parent()
        board.set_auto_battle_enabled(was_auto)
    except Exception:
        pass
def tick(delta):
    elapsed = time.monotonic()-state['start']
    if elapsed < 0:
        return
    if elapsed > 600 or not base._get_game_world():
        stop()
        return
    t = elapsed % (total+1.2)
    size=unreal.SlateLibrary.get_local_size(image.get_cached_geometry())
    if size.x > 0 and size.y > 0:
        mid.set_scalar_parameter_value('ViewportAspect',size.x/size.y)
    index = min(len(frames)-1,bisect.bisect_right(ends,t))
    frame = frames[index]
    image.set_render_opacity(1.0 if t < total else max(0., 1.-(t-total)/.25))
    image.set_color_and_opacity(unreal.LinearColor(1,1,1,1))
    if index == state['last']:
        return
    state['last'] = index
    frame = frames[index]
    mid.set_texture_parameter_value('AtlasTexture', textures[frame['atlas_index']])
    for key,value in zip(('U0','V0','U1','V1'),frame['uv_min']+frame['uv_max']):
        mid.set_scalar_parameter_value(key,value)
state['stop'] = stop
state['handle'] = unreal.register_slate_post_tick_callback(tick)
builtins._lightning057_preview = state
print(json.dumps({'ok':True,'mode':'PIE visual preview only','frames':len(frames),'flip_x':1,'seconds':total,'loop_for_seconds':600}))
