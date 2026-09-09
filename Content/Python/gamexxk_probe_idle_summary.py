"""Observe the live idle summary; waits and screenshots belong to the MCP host."""
import json
import sys
import unreal

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world, 'Start PIE on L_DesktopTrainingHUD first'
pc = unreal.GameplayStatics.get_player_controller(world, 0)
pc.set_desktop_training_workbench_enabled_for_test(True)
widget = pc.get_desktop_training_workbench_widget_for_test()
subsystem = widget.get_mvp_subsystem()
mode = sys.argv[1] if len(sys.argv) > 1 else 'observe'
if mode == 'start':
    subsystem.start_new_game()
    pc.refresh_player_flow_widgets_for_test()
    widget.open_workbench()
    subsystem.start_training_travel('Training.Normal.1-1')
elif mode == 'load':
    assert subsystem.load_game_from_slot('GameXXK_MVP_SaveSlot_1', 0)
    pc.refresh_player_flow_widgets_for_test()
    widget.open_workbench()
elif mode == 'open':
    widget.open_workbench()
elif mode == 'fold':
    widget.handle_desktop_action_for_test(653)
elif mode == 'tab':
    widget.handle_desktop_action_for_test(60)
elif mode == 'advance':
    for _ in range(min(25, int(sys.argv[2]) if len(sys.argv) > 2 else 1)):
        widget.advance_travel_for_test(1)
elif mode == 'scale':
    widget.handle_desktop_action_for_test({'50': 651, '75': 656, '100': 650}[sys.argv[2]])
elif mode == 'chest':
    widget.handle_desktop_action_for_test(601 if sys.argv[2] == 'advanced' else 600)
elif mode == 'purchase':
    product = unreal.GameXXKMetaShopProductId.ADVANCED_CHEST if sys.argv[2] == 'advanced' else unreal.GameXXKMetaShopProductId.NORMAL_CHEST
    for _ in range(min(5, int(sys.argv[3]))):
        assert subsystem.purchase_meta_shop_product(product) is not None

tree = unreal.find_object(widget, 'DesktopTrainingWorkbenchWidgetTree') or unreal.find_object(widget, 'WidgetTree')
active = {}
pending = [unreal.find_object(tree, 'DesktopTrainingOverlayRoot')]
while pending:
    item = pending.pop()
    if not item:
        continue
    active[item.get_name()] = item
    if isinstance(item, unreal.PanelWidget):
        pending.extend(item.get_all_children())
runtime = subsystem.get_training_travel_runtime_copy()
if mode == 'diagnose':
    print(json.dumps({'objects': [{'path': item.get_path_name(), 'text': str(item.get_text()),
        'active': item in active.values(), 'parent': str(item.get_parent())}
        for item in unreal.ObjectIterator(unreal.TextBlock)
        if 'TrainingWaveIndexText' in item.get_name()]}))
report = {'mode': mode, 'world': world.get_name(), 'runtime_encounter': runtime.encounter_index,
          'stage': str(runtime.stage_id), 'tick': widget.get_travel_visual_native_tick_count_for_test(),
          'screen': str(subsystem.get_runtime_state_copy().screen),
          'visible': widget.is_workbench_visible_for_test(),
          'backpack_expanded': widget.is_backpack_expanded_for_test(),
          'folded': 'TrainingFoldedNormalChestButton' in active,
          'builds': widget.get_programmatic_layout_build_count_for_test_blueprint(),
          'widgets': []}
for name in ['TrainingWaveIndexText', 'TrainingWaveStageText', 'TrainingWaveProgressFill',
             'TrainingNormalChestButton', 'TrainingAdvancedChestButton',
             'TrainingNormalChestCountText', 'TrainingAdvancedChestCountText',
             'TrainingFoldedNormalChestButton', 'TrainingFoldedAdvancedChestButton',
             'TrainingFoldedNormalChestIcon', 'TrainingFoldedAdvancedChestIcon',
             'TrainingFoldedNormalChestText', 'TrainingFoldedAdvancedChestText',
             'DesktopNoticeLine_0', 'DesktopNoticeLinesCanvas', 'IdleStripFoldButton']:
    item = active.get(name)
    if not item:
        continue
    geometry = item.get_cached_geometry()
    size = unreal.SlateLibrary.get_local_size(geometry)
    position = unreal.SlateLibrary.local_to_absolute(geometry, unreal.Vector2D(0, 0))
    record = {'name': name, 'size': [size.x, size.y], 'position': [position.x, position.y]}
    if isinstance(item, unreal.TextBlock):
        record['text'] = str(item.get_text())
    report['widgets'].append(record)
print(json.dumps(report, ensure_ascii=False))
