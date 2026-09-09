"""Small phases for observing real talent clicks under a reversible Dev session."""
import json
import sys
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base

world, controller, workbench = base._controller_and_widget()
if not world or not workbench or 'L_DesktopTrainingHUD' not in world.get_path_name():
    raise RuntimeError('Canonical 2D PIE is required')
instance = unreal.GameplayStatics.get_game_instance(world)
dev = next((obj for obj in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem)
            if obj.get_outer() == instance), None)
if not dev:
    raise RuntimeError('Development session guard is unavailable')
subsystem = workbench.get_mvp_subsystem()
output = Path(unreal.Paths.project_dir()) / 'Saved/Diagnostics/TalentUpgradeUsability/PIE'
output.mkdir(parents=True, exist_ok=True)

def command(name, args=None):
    result = json.loads(dev.execute_json(json.dumps({'command': name, 'args': args or {}})))
    if not result.get('ok'):
        raise RuntimeError(name + ': ' + result.get('message', 'failed'))
    return result

def walk(node):
    if not node:
        return
    yield node
    if isinstance(node, unreal.PanelWidget):
        for index in range(node.get_children_count()):
            yield from walk(node.get_child_at(index))

phase = sys.argv[1] if len(sys.argv) > 1 else 'observe'
if phase == 'backup':
    (output / 'prebuild-shared-runtime.json').write_text(json.dumps(command('snapshot.export')['data'], ensure_ascii=False, indent=2), encoding='utf-8')
elif phase == 'fixture':
    if dev.is_session_active():
        raise RuntimeError('Restore the existing Dev session first')
    command('session.begin')
    (output / 'original-runtime.json').write_text(json.dumps(command('snapshot.export')['data'], ensure_ascii=False, indent=2), encoding='utf-8')
    assert subsystem.start_game()
    subsystem.purchase_talent_node('Talent.Root')
    subsystem.purchase_talent_node('Talent.Entry.Combat')
    workbench.open_backpack()
    workbench.handle_desktop_action_for_test(2)
elif phase == 'restore':
    command('session.restore')
elif phase == 'finish':
    if dev.is_session_active():
        raise RuntimeError('Restore the temporary test session before saving')
    assert subsystem.save_current_game()
    (output / 'restored-runtime-saved.json').write_text(json.dumps(command('snapshot.export')['data'], ensure_ascii=False, indent=2), encoding='utf-8')

tree = unreal.find_object(workbench, 'DesktopTrainingWorkbenchWidgetTree') or unreal.find_object(workbench, 'WidgetTree')
canvas = unreal.find_object(tree, 'DesktopTrainingReferenceCanvas') if tree else None
talent = next((w for w in walk(canvas) if w.get_name() == 'PermanentTalentTreeWidget'), None)
controls = {}
if talent:
    inner = unreal.find_object(talent, 'TalentWidgetTree') or unreal.find_object(talent, 'WidgetTree')
    root = unreal.find_object(inner, 'TalentTreeRoot') if inner else None
    for node in walk(root):
        name = node.get_name()
        if not name.startswith(('TalentPurchase', 'TalentUpgrade', 'TalentDetail', 'TalentNode_', 'TalentGraphFrame')):
            continue
        geom = node.get_cached_geometry()
        position = unreal.SlateLibrary.local_to_absolute(geom, unreal.Vector2D(0, 0))
        size = unreal.SlateLibrary.get_absolute_size(geom)
        row = {'path': node.get_path_name(), 'enabled': node.get_is_enabled(), 'visibility': str(node.get_visibility()),
               'position': [position.x, position.y], 'absolute_size': [size.x, size.y]}
        if isinstance(node, unreal.TextBlock):
            row['text'] = str(node.get_text())
        controls[name] = row
state = subsystem.get_runtime_state_copy()
views = subsystem.get_talent_node_views()
result = {'phase': phase, 'world': world.get_path_name(), 'session_active': dev.is_session_active(),
          'gold': state.player_gold, 'hp': state.player_hp, 'max_hp': state.player_max_hp,
          'controls': controls,
          'nodes': [{'id': str(v.definition.id), 'rank': v.rank, 'price': v.next_price, 'state': str(v.state),
                     'position': [v.definition.graph_position.x, v.definition.graph_position.y]}
                    for v in views if str(v.definition.id) in ['Talent.Root', 'Talent.Entry.Combat', 'Talent.Combat.FlatAttack.01', 'Talent.Combat.FlatHealth.01']]}
(output / (phase + '.json')).write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(result, ensure_ascii=False))
