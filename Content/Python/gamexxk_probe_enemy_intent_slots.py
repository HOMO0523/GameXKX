"""Check visible intent wrappers, including labels left behind after a death."""
import json
import sys
from pathlib import Path
import unreal
import gamexxk_probe_real_play_flow as base

world=base._get_game_world()
pc=base._first_player_controller(world)
board=pc.get_battle_board_widget_for_test()
widgets={}
tree=unreal.find_object(board,'WidgetTree') or unreal.find_object(board,'BattleBoardWidgetTree')
root=unreal.find_object(tree,'GameXXKBattleViewportRoot') if tree else None
assert root is not None, 'The live viewport widget tree must exist'
pending=[root]
while pending:
    widget=pending.pop()
    if not widget:continue
    widgets[widget.get_name()]=widget
    if isinstance(widget,unreal.PanelWidget):pending.extend(widget.get_all_children())
rows=[]
for index in range(3):
    prefix=f'BattleEnemyIntentSlot_{index:02d}'
    wrapper=widgets.get(prefix)
    number=widgets.get(f'BattleEnemyIntentSlotNumber_{index:02d}')
    card=widgets.get(f'BattleEnemyIntentCard_{index:02d}')
    rows.append({'slot':index,'wrapper_visible':wrapper is not None and wrapper.get_visibility()!=unreal.SlateVisibility.COLLAPSED,
                 'number':str(number.get_text()) if number else '',
                 'card_visible':card is not None and card.get_visibility()!=unreal.SlateVisibility.COLLAPSED})
label=sys.argv[1] if len(sys.argv)>1 else 'current'
result={'label':label,'intent_count':sum(r['card_visible'] for r in rows),'slots':rows}
path=Path(unreal.Paths.project_dir())/'Saved/Codex/CardEffects-20260907'/('enemy-intent-slots-'+label+'.json')
path.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False))
expected=[r for r in rows if r['card_visible']]
assert len([r for r in rows if r['wrapper_visible']])==len(expected), 'An empty intent card left a visible label wrapper'
assert len({r['number'] for r in rows if r['wrapper_visible']})==len(expected), 'Duplicate visible enemy slot numbers'
