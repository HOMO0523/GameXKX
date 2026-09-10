"""Short teaching review operations inside an already protected Dev session."""
import json
import sys
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base

world, pc, wb = base._controller_and_widget()
assert world and wb and 'L_DesktopTrainingHUD' in world.get_path_name()
instance = unreal.GameplayStatics.get_game_instance(world)
dev = next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer() == instance)
academy = next(o for o in unreal.ObjectIterator(unreal.GameXXKAcademySubsystem) if o.get_outer() == instance)
assert dev.is_session_active(), 'The normal UI review must protect player state first'
mode = sys.argv[1] if len(sys.argv) > 1 else 'inspect'
if mode == 'course':
    academy.cancel_course()
    assert academy.begin_course(unreal.Name(sys.argv[2])), 'Course could not start'
elif mode == 'cancel':
    academy.cancel_course()
elif mode == 'ui':
    academy.cancel_course()
    wb.open_workbench(); wb.open_backpack()
    wb.handle_desktop_action_for_test(652); wb.handle_desktop_action_for_test(662)

records = []
for widget in unreal.ObjectIterator(unreal.TextBlock):
    if widget.get_name() not in ('AcademyLessonTitle', 'AcademyMechanism', 'AcademyAction', 'AcademyObjective', 'ExitOfflineHint'):
        continue
    owner = widget.get_outer()
    if isinstance(owner, unreal.GameXXKBattleBoardWidget) and owner.get_owning_player() != pc:
        continue
    if not widget.get_parent():
        continue
    geo = widget.get_cached_geometry(); size = unreal.SlateLibrary.get_local_size(geo)
    pos = unreal.SlateLibrary.local_to_absolute(geo, unreal.Vector2D(0, 0))
    records.append({'name': widget.get_name(), 'text': str(widget.get_text()), 'visible': widget.is_visible(),
                    'position': [pos.x, pos.y], 'size': [size.x, size.y]})
out = Path(unreal.Paths.project_dir()) / 'Saved/Codex/UIGuidanceLocalization-20260910/ui-review/teaching-live.json'
out.write_text(json.dumps({'mode': mode, 'widgets': records}, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'mode': mode, 'widgets': records}, ensure_ascii=True))
