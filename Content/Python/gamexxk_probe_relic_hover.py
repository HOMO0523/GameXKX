"""Inspect the actual acquired-relic hit targets in the current 2D PIE."""
import json
import sys
import unreal
import gamexxk_probe_real_play_flow as base

world = base._get_game_world()
pc = base._first_player_controller(world) if world else None
bar = pc.get_relic_bar_widget_for_test() if pc else None
if not bar:
    raise RuntimeError('The current route/battle needs an acquired relic')
tree = unreal.find_object(bar, 'RelicBarWidgetTree') or unreal.find_object(bar, 'WidgetTree')
if not tree:
    raise RuntimeError('Relic widget tree not found')
records = []
grid = unreal.find_object(tree, 'RelicBarSixColumnGrid')
for index in range(bar.get_rendered_relic_count_for_test()):
    slot = grid.get_child_at(index)
    if not slot:
        continue
    if len(sys.argv) > 1 and sys.argv[1] == 'verify_hit_hypothesis':
        slot.set_visibility(unreal.SlateVisibility.VISIBLE)
    geometry = slot.get_cached_geometry()
    center = unreal.SlateLibrary.local_to_absolute(geometry, unreal.Vector2D(26, 26))
    records.append({'index': index, 'visibility': str(slot.get_visibility()),
                    'tooltip_text': str(slot.get_editor_property('tool_tip_text')),
                    'tooltip_widget': str(slot.get_editor_property('tool_tip_widget')),
                    'center': [center.x, center.y]})
print(json.dumps(records, ensure_ascii=False))
