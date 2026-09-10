"""Small UI review phases; temporary gameplay state and recorded preferences."""
import json
import sys
import re
import hashlib
from pathlib import Path
import unreal
import gamexxk_probe_training_visual_mvp as base

world, pc, wb = base._controller_and_widget()
assert world and wb and 'L_DesktopTrainingHUD' in world.get_path_name()
instance = unreal.GameplayStatics.get_game_instance(world)
dev = next(o for o in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if o.get_outer() == instance)
out = Path(unreal.Paths.project_dir()) / 'Saved/Codex/UIGuidanceLocalization-20260910/ui-review'
out.mkdir(parents=True, exist_ok=True)
mode = sys.argv[1] if len(sys.argv) > 1 else 'observe'

def hud_scale():
    config = (Path(unreal.Paths.project_dir()) / 'Saved/Config/GameXXKDesktopHudSettings.ini').read_text(encoding='utf-8-sig')
    return int(re.search(r'(?m)^HudScalePercent=(\d+)', config).group(1))

def command(name):
    result = json.loads(dev.execute_json(json.dumps({'command': name, 'args': {}})))
    assert result.get('ok'), result
    return result

if mode == 'begin':
    assert not dev.is_session_active(), 'Another temporary test owns the player'
    assert wb.get_mvp_subsystem().save_current_game(), 'Save the real player before opening a temporary review session'
    original = {'language': unreal.GameXXKLocalizationLibrary.get_language(),
                'scale': hud_scale(),
                'runtime': command('snapshot.export')}
    (out / 'review-original.json').write_text(json.dumps(original, ensure_ascii=False, indent=2), encoding='utf-8')
    command('session.begin')
    wb.open_workbench(); wb.open_backpack()
elif mode == 'restore':
    assert dev.is_session_active()
    original = json.loads((out / 'review-original.json').read_text(encoding='utf-8'))
    command('session.restore')
    assert command('snapshot.export')['data']['state'] == original['runtime']['data']['state'], 'Original player state was not restored exactly'
    if 'saveHashes' in original:
        assert {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(unreal.Paths.project_dir())/'Saved/SaveGames').glob('*.sav')} == original['saveHashes'], 'A real save changed during the protected review'
    unreal.GameXXKLocalizationLibrary.set_language(original['language'], True)
    wb.handle_desktop_action_for_test({50: 651, 75: 656, 100: 650}[original['scale']])
    wb.open_backpack()
elif mode == 'language':
    assert dev.is_session_active()
    assert unreal.GameXXKLocalizationLibrary.set_language(sys.argv[2], False)
elif mode == 'pause':
    assert dev.is_session_active()
    original=json.loads((out/'review-original.json').read_text(encoding='utf-8'))
    original.setdefault('saveHashes',{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(unreal.Paths.project_dir())/'Saved/SaveGames').glob('*.sav')})
    (out/'review-original.json').write_text(json.dumps(original,ensure_ascii=False,indent=2),encoding='utf-8')
    scene=command('snapshot.export')['data']
    scene['state']['training']['bTravelActive']=False
    scene['state']['training']['activeTravelEncounterIndex']=-1
    scene['travel']={}
    result=json.loads(dev.execute_json(json.dumps({'command':'snapshot.import','args':{'scene':scene}})))
    assert result.get('ok'),result
elif mode == 'reload':
    assert unreal.GameXXKLocalizationLibrary.reload_text_catalog()
elif mode == 'action':
    assert dev.is_session_active()
    wb.handle_desktop_action_for_test(int(sys.argv[2]))
elif mode == 'backpack':
    assert dev.is_session_active()
    wb.open_backpack()

tree = unreal.find_object(wb, 'DesktopTrainingWorkbenchWidgetTree') or unreal.find_object(wb, 'WidgetTree')
root = unreal.find_object(tree, 'DesktopTrainingReferenceCanvas') if tree else None
active = {}
pending = [root]
while pending:
    item = pending.pop()
    if not item: continue
    active[item.get_name()] = item
    if isinstance(item, unreal.PanelWidget): pending.extend(item.get_all_children())

records = []
for item in active.values():
    if not isinstance(item, unreal.TextBlock) and item.get_name() not in (
        'DesktopHudSettingsPanel', 'BackpackPanel', 'HudSettingsCloseButton',
        'HudScale50Button', 'HudScale75Button', 'HudScale100Button',
        'HudLanguageChineseButton', 'HudLanguageEnglishButton', 'TrainingTravelStrip',
        'MainStoryCentralPanel', 'MainStoryDialoguePanel'):
        continue
    geometry = item.get_cached_geometry()
    position = unreal.SlateLibrary.local_to_absolute(geometry, unreal.Vector2D(0, 0))
    size = unreal.SlateLibrary.get_local_size(geometry)
    scale = unreal.SlateLibrary.local_to_absolute(geometry, unreal.Vector2D(size.x, size.y))
    transform = item.get_editor_property('render_transform')
    translation = transform.get_editor_property('translation')
    record = {'name': item.get_name(), 'position': [position.x, position.y],
              'size': [size.x, size.y], 'rendered_size': [scale.x-position.x, scale.y-position.y],
              'translation': [translation.x, translation.y], 'visible': item.is_visible()}
    if isinstance(item, unreal.TextBlock):
        font = item.get_editor_property('font'); resource = font.get_editor_property('font_object')
        record.update(text=str(item.get_text()), font=resource.get_path_name() if resource else '',
                      face=str(font.get_editor_property('typeface_font_name')), font_size=font.get_editor_property('size'))
    records.append(record)
body = wb.get_desktop_body_offset_for_test()
report = {'mode': mode, 'language': unreal.GameXXKLocalizationLibrary.get_language(),
          'scale': hud_scale(), 'dev_session': dev.is_session_active(),
          'body': [body.x, body.y], 'widgets': records}
(out / 'review-live.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'mode': mode, 'language': report['language'], 'scale': report['scale'],
                  'dev_session': report['dev_session'], 'widgets': len(records)}, ensure_ascii=True))
