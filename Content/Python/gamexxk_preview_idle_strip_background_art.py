"""Preview an RGBA plate on the actual idle widgets, without changing packages."""
import json
import sys
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base

root = Path(__file__).resolve().parents[2]
out = root / 'Saved/Codex/IdleStripBackground-20260907'
out.mkdir(parents=True, exist_ok=True)
world, controller, workbench = base._controller_and_widget()
assert world and workbench, 'Canonical desktop PIE is required'
assert 'L_DesktopTrainingHUD' in world.get_path_name(), world.get_path_name()
restore_asset = len(sys.argv) > 1 and sys.argv[1] == '--runtime'
source = Path(sys.argv[1]) if len(sys.argv) > 1 and not restore_asset else None
texture = (unreal.load_asset('/Game/GameXXK/UI/ImageTruth/Training/T_TrainingIdleStrip_Background')
           if restore_asset else unreal.RenderingLibrary.import_file_as_texture2d(world, str(source)) if source else None)
if source or restore_asset:
    assert texture, str(source)
    # Hold the transient image alive for the preview. Runtime assets stay untouched.
    unreal._gamexxk_idle_background_art_preview = texture

records = []
for widget in unreal.ObjectIterator(unreal.Image):
    name = widget.get_name()
    if not (name.startswith('TravelBackgroundTile_') or name.startswith('TravelHeroAnimatedUnit')
            or name.startswith('TravelCompanionAnimatedUnit') or name.startswith('TravelEnemyAnimatedUnit')):
        continue
    owner = widget.get_outer()
    while owner and owner != workbench:
        owner = owner.get_outer()
    if owner != workbench:
        continue
    if texture and name.startswith('TravelBackgroundTile_'):
        widget.set_brush_from_texture(texture, False)
    brush = widget.get_editor_property('brush')
    resource = brush.get_editor_property('resource_object')
    record = {'name': name, 'resource': resource.get_path_name() if resource else None,
              'opacity': widget.get_render_opacity(), 'visibility': str(widget.get_visibility())}
    if isinstance(widget.slot, unreal.CanvasPanelSlot):
        pos, size = widget.slot.get_position(), widget.slot.get_size()
        record['position'] = [pos.x, pos.y]
        record['size'] = [size.x, size.y]
    records.append(record)
report = {'world': world.get_path_name(), 'preview_source': str(source) if source else None,
          'scroll_offset': workbench.get_travel_visual_scroll_offset_for_test(), 'images': records}
(out / 'runtime-preview.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False))
