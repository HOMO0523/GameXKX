"""Read the active idle-strip asset and map; export evidence without saving packages."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
out = root / 'Saved/Codex/IdleStripBackground-20260907'
out.mkdir(parents=True, exist_ok=True)
asset_path = '/Game/GameXXK/UI/ImageTruth/Training/T_TrainingIdleStrip_Background'
texture = unreal.load_asset(asset_path)
assert isinstance(texture, unreal.Texture2D), asset_path
report = {'asset': texture.get_path_name(), 'source': list(texture.get_editor_property('asset_import_data').extract_filenames())}
for prop in ['compression_settings', 'mip_gen_settings', 'lod_group', 'filter', 'address_x', 'address_y', 'srgb', 'never_stream']:
    report[prop] = str(texture.get_editor_property(prop))
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
report['editor_world'] = world.get_path_name() if world else None
report['dirty_content_packages'] = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
destination = out / 'before-runtime-texture.png'
if not destination.exists():
    task = unreal.AssetExportTask()
    task.object = texture
    task.filename = str(destination)
    task.automated = True
    task.prompt = False
    task.replace_identical = False
    task.exporter = unreal.TextureExporterPNG()
    assert unreal.Exporter.run_asset_export_task(task), 'export failed'
report['export_sha256'] = hashlib.sha256(destination.read_bytes()).hexdigest()
(out / 'before-inventory.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False))
