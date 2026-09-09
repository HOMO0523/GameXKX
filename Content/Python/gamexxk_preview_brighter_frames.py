"""Preview brighter rarity frames only; never change cards, text or game rules."""
import json
import unreal
import gamexxk_probe_real_play_flow as base
from gamexxk_author_card_name_flow import rgb

world = base._get_game_world()
pc = base._first_player_controller(world)
board = pc.get_battle_board_widget_for_test()
tree = board.get_hand_card_button_for_test(0).get_outer()
updated = []
for index in range(5):
    frame = unreal.find_object(tree, f'BattleHandCard_{index:02d}LabelQualityFrame')
    if not frame:
        continue
    material = frame.get_editor_property('brush').get_editor_property('resource_object')
    if not isinstance(material, unreal.MaterialInstanceDynamic):
        continue
    tier = 'Epic' if 'Epic' in material.get_name() else 'Rare'
    colors = ('7e3217','c08221') if tier == 'Epic' else ('243b91','572786')
    for key, value in zip(('ColorA','ColorB'), colors):
        c = rgb(value)
        material.set_vector_parameter_value(key, unreal.LinearColor(c.r*1.6,c.g*1.6,c.b*1.6,1))
    updated.append({'index': index, 'tier': tier})
print(json.dumps(updated))
