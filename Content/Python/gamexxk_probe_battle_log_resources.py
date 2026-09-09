"""Inspect the live, rendered battle log and shared ink-resource widgets."""
import json
import sys
from pathlib import Path
import unreal
import gamexxk_probe_real_play_flow as base

world = base._get_game_world()
pc = base._first_player_controller(world)
board = pc.get_battle_board_widget_for_test()
if not board or not board.is_battle_board_visible():
    raise RuntimeError('The canonical battle board must be visible')
tree = unreal.find_object(board,'BattleBoardWidgetTree') or unreal.find_object(board,'WidgetTree')
mode = sys.argv[1] if len(sys.argv)>1 else 'compact'
if mode == 'toggle':
    board.call_method('HandleBattleSettlementLogToggle')

def record(widget):
    geometry = widget.get_cached_geometry()
    size = unreal.SlateLibrary.get_local_size(geometry)
    position = unreal.SlateLibrary.local_to_absolute(geometry,unreal.Vector2D(0,0))
    center = unreal.SlateLibrary.local_to_absolute(geometry,unreal.Vector2D(size.x*.5,size.y*.5))
    result = {'name':widget.get_name(),'size':[size.x,size.y],'position':[position.x,position.y],
              'center':[center.x,center.y],
              'visibility':str(widget.get_visibility())}
    if isinstance(widget,unreal.TextBlock):
        result['text'] = str(widget.get_text())
        font = widget.get_editor_property('font')
        result['font_size'] = font.size
        result['font'] = str(font.font_object)
    return result

result = {'mode':mode,'widgets':[],'resources':[]}
for name in ['BattleSettlementLogPanel','BattleSettlementLogText','BattleSettlementLogToggle',
             'BattleSettlementSummary','BattleTerrainFeedback','BattleEnemyIntentCardBox']:
    widget = unreal.find_object(tree,name)
    if widget: result['widgets'].append(record(widget))
for unit in unreal.WidgetLibrary.get_all_widgets_of_class(world,unreal.GameXXKBattleUnitResourceWidget,False):
    unit_tree = unreal.find_object(unit,'WidgetTree')
    values = {'name':unit.get_name(),'hp':unit.get_health_display_text_for_test(),
              'mp':unit.get_mana_display_text_for_test(),'hp_fraction':unit.get_health_percent_for_test(),
              'mp_fraction':unit.get_mana_percent_for_test(),'materials':[]}
    for name in ['HealthBarLegacy','ManaBarLegacy']:
        widget = unreal.find_object(unit_tree,name) if unit_tree else None
        material = widget.get_dynamic_material() if widget else None
        if material:
            values['materials'].append({'name':name,'material':material.get_path_name(),
                                       'percent':material.get_scalar_parameter_value('FillPercent')})
    result['resources'].append(values)
destination = Path(unreal.Paths.project_dir())/'Saved/Codex/CardEffects-20260907'
(destination/('battle-log-resources-'+mode+'.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False))
