"""Read the active desktop tab's actual paper layers without changing assets."""
import json
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as probe

world, controller, workbench = probe._controller_and_widget()
if not world or not workbench:
    raise RuntimeError('A real desktop PIE world is required')
tree = unreal.find_object(workbench, 'DesktopTrainingWorkbenchWidgetTree') or unreal.find_object(workbench, 'WidgetTree')

def walk(widget):
    yield widget
    if isinstance(widget, unreal.PanelWidget):
        for i in range(widget.get_children_count()):
            yield from walk(widget.get_child_at(i))

reference_canvas = unreal.find_object(tree, 'DesktopTrainingReferenceCanvas')
if not reference_canvas:
    raise RuntimeError('The current desktop reference canvas was not found')
active = {widget.get_name():widget for widget in walk(reference_canvas)}
result = {'world':world.get_name(), 'panels':{}}
for name in ('WarehousePanel','FormationPanel','TalentsPanel','ToolsPanel','TrainingMapPanel','CharacterPickerPage','ToolsTalentLockedPanel','ToolInputGridFrame','DesktopHudSettingsPanel'):
    panel = active.get(name)
    if not panel:
        continue
    row = {'clipping':str(panel.get_editor_property('clipping')), 'layers':[]}
    for widget in walk(panel):
        if isinstance(widget, unreal.ScaleBox):
            row['layers'].append({'type':'ScaleBox','scale':widget.get_editor_property('user_specified_scale'), 'offsets':str(widget.get_editor_property('slot').get_editor_property('layout_data'))})
        elif isinstance(widget, unreal.Border):
            row['layers'].append({'type':'Border','background':json.loads(unreal.ToolsetLibrary.get_object_properties(widget,['Background']))})
    result['panels'][name] = row
result['nodes'] = []
for index in range(1, 10):
    icon = active.get('TrainingNode_' + str(index))
    label = active.get('TrainingNodeLabel_' + str(index))
    state = active.get('TrainingNodeState_' + str(index))
    if not icon or not label or not state:
        continue
    icon_slot, label_slot, state_slot = [x.get_editor_property('slot') for x in (icon,label,state)]
    icon_pos, icon_size = icon_slot.get_position(), icon_slot.get_size()
    label_pos, state_pos = label_slot.get_position(), state_slot.get_position()
    separated = min(label_pos.x, state_pos.x) >= icon_pos.x + icon_size.x + 13.9
    result['nodes'].append({'stage':index, 'label':str(label.get_text()), 'state':str(state.get_text()),
        'icon_position':[icon_pos.x,icon_pos.y], 'label_position':[label_pos.x,label_pos.y],
        'state_position':[state_pos.x,state_pos.y], 'text_is_right_of_icon':separated})
    assert separated, 'Training node text overlaps its icon'
result['tool_controls'] = {}
for name in ('ToolsTalentLockedPanel','ToolButton_0','ToolConfirmButton'):
    widget = active.get(name)
    if widget: result['tool_controls'][name] = str(widget.get_visibility())
out=Path(unreal.Paths.get_project_file_path()).resolve().parent/'Saved/Codex/TabPaperUnity-20260906'
out.mkdir(exist_ok=True)
(out/'last-paper-state.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False))
