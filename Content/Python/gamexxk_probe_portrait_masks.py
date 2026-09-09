"""Observe the actual card-local mask geometry after a real Slate paint."""
import json
from pathlib import Path
import sys
import unreal
import gamexxk_probe_real_play_flow as base

world=base._get_game_world()
pc=base._first_player_controller(world)
board=pc.get_battle_board_widget_for_test()
rows=[]
def walk(widget):
    if isinstance(widget, unreal.GameXXKCardPortraitImage):
        size=widget.get_mask_card_size_for_test()
        rows.append({'widget':widget.get_name(),'card_geometry':widget.uses_card_geometry_for_test(),
                     'card_size':[size.x,size.y],'visibility':str(widget.get_visibility())})
    if isinstance(widget,unreal.PanelWidget):
        for child in widget.get_all_children():walk(child)

for index in range(5):
    button=board.get_hand_card_button_for_test(index)
    if button:walk(button)
name=sys.argv[1] if len(sys.argv)>1 else 'current'
out=Path(__file__).resolve().parents[2]/'Saved/Codex/CardEffects-20260907'
(out/f'portrait-masks-{name}.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(rows,ensure_ascii=False))
