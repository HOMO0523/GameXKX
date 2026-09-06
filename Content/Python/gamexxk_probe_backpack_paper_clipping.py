"""Inspect backpack paper bounds; optionally release the old live outer clip."""
import json
import sys
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as probe

world, controller, workbench = probe._controller_and_widget()
if not world or not workbench:
    raise RuntimeError('A real desktop PIE world is required')
tree = unreal.find_object(workbench, 'DesktopTrainingWorkbenchWidgetTree') or unreal.find_object(workbench, 'WidgetTree')
if not tree:
    raise RuntimeError('Workbench widget tree not found')
clip = unreal.find_object(tree, 'EmbeddedBackpackContentClip')
mode = sys.argv[1] if len(sys.argv) > 1 else 'inspect'
if mode == 'unclip':
    if not clip:
        raise RuntimeError('The old outer clip is not present')
    clip.set_clipping(unreal.WidgetClipping.INHERIT)
elif mode not in ('inspect', 'scroll-deck'):
    raise ValueError(mode)
embedded = unreal.find_object(tree, 'EmbeddedApprovedBackpack')
embedded_tree = unreal.find_object(embedded, 'InventoryWindowWidgetTree') or unreal.find_object(embedded, 'WidgetTree')
if mode == 'scroll-deck':
    unreal.find_object(embedded_tree, 'InventoryHeroDeckScrollBox').set_scroll_offset(480.0)
paper = unreal.find_object(embedded_tree, 'InventoryWindowFrame')
paper_slot = paper.get_editor_property('slot')
embedded_slot = embedded.get_editor_property('slot')
pos, size, offset = paper_slot.get_position(), paper_slot.get_size(), embedded_slot.get_position()
result = {
    'mode': mode,
    'map': world.get_name(),
    'outer_clip_present': bool(clip),
    'outer_clipping': str(clip.get_editor_property('clipping')) if clip else None,
    'paper_bounds_in_reference': [pos.x + offset.x, pos.y + offset.y, pos.x + offset.x + size.x, pos.y + offset.y + size.y],
    'embedded_offset': [offset.x, offset.y],
    'scroll_regions': {},
}
for name in ('InventoryBackpackScrollBox', 'HeroDeckScrollBox', 'InventoryHeroDeckScrollBox', 'CharacterDeckScrollBox'):
    widget = unreal.find_object(embedded_tree, name)
    if widget:
        result['scroll_regions'][name] = {'class': widget.get_class().get_name(), 'clipping': str(widget.get_editor_property('clipping')), 'scroll_offset': widget.get_scroll_offset()}
out = Path(unreal.Paths.get_project_file_path()).resolve().parent / 'Saved/Codex/BackpackPaperEdges-20260906'
out.mkdir(exist_ok=True)
(out / (mode + '-paper-bounds.json')).write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(result, ensure_ascii=False))
