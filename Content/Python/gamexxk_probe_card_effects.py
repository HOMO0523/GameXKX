"""Observe real 2D battle card effects through the project's PIE bridge."""
import json
import sys
import unreal
import gamexxk_probe_real_play_flow as base

world=base._get_game_world()
pc=base._first_player_controller(world) if world else None
board=pc.get_battle_board_widget_for_test() if pc else None
if not board:
    raise RuntimeError("Start a desktop training battle first")
mode=sys.argv[1] if len(sys.argv)>1 else "state"
result={"ok":True,"mode":mode}
if mode=="boards":
    widgets=unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.GameXXKBattleBoardWidget,False)
    print([(w.get_path_name(),w.is_in_viewport(),str(w.get_visibility()),str(w.get_editor_property('tick_frequency'))) for w in widgets])
result['board_path']=board.get_path_name()
result['debug']=board.get_battle_board_debug_state_for_test()
result['outcome_class']=board.get_card_outcome_preview_class_for_test()
result['outcome_lines']=list(board.get_card_outcome_preview_lines_for_test())
if mode=="play":
    result['accepted']=board.click_card_in_hand(sys.argv[2])
elif mode=="target":
    result['accepted']=board.confirm_targeting_unit(sys.argv[2])
elif mode=="action":
    result['return']=str(board.call_method(sys.argv[2]))
elif mode=="manual":
    result['accepted']=board.set_auto_battle_enabled(False)
result['targeting']=board.is_card_targeting_active()
result['auto_battle']=board.is_auto_battle_enabled()
def size(widget):
    value=unreal.SlateLibrary.get_local_size(widget.get_cached_geometry())
    return [value.x,value.y]

tree=unreal.find_object(board,'BattleBoardWidgetTree') or unreal.find_object(board,'WidgetTree')
result['cards']=[]
for i in range(5):
    name=f'BattleHandCard_{i:02d}'
    widget=board.get_hand_card_button_for_test(i)
    if not widget:continue
    transform=widget.get_editor_property('render_transform')
    parent=widget.get_parent()
    aura=parent.get_child_at(0) if parent and parent.get_children_count()==2 else None
    aura_transform=aura.get_editor_property('render_transform') if aura else None
    result['cards'].append({'slot':i,'cue':board.get_hand_synergy_for_test(i),
                            'enabled':widget.get_is_enabled(),
                            'opacity':widget.get_render_opacity(),'visibility':str(widget.get_visibility()),
                            'offset':[transform.translation.x,transform.translation.y],
                            'scale':[transform.scale.x,transform.scale.y],'angle':transform.angle,
                            'size':size(widget),
                            'aura_size':size(aura) if aura else None,
                            'aura_offset':[aura_transform.translation.x,aura_transform.translation.y] if aura else None,
                            'aura_opacity':aura.get_render_opacity() if aura else None})
print(json.dumps(result,ensure_ascii=False))
