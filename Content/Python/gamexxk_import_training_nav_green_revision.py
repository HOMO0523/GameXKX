"""Import only the user-requested bold, green-route training navigation icon."""
import hashlib
import json
from pathlib import Path

import unreal

root = Path(__file__).resolve().parents[2]
folder = root / 'SourceArt/UI/ImageTruth/revisions/20260909-training-green'
manifest_path = folder / 'manifest.json'
manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
source = root / manifest['final']
assert hashlib.sha256(source.read_bytes()).hexdigest() == manifest['sha256']
asset = '/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTraining'
texture = unreal.load_asset(asset)
assert isinstance(texture, unreal.Texture2D)
already_imported = source.resolve() in [Path(p).resolve() for p in texture.get_editor_property('asset_import_data').extract_filenames()]
assert already_imported or not any(p.get_name() == asset for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()), 'Target has unrelated unsaved edits'
properties = ['compression_settings', 'mip_gen_settings', 'lod_group', 'filter',
              'address_x', 'address_y', 'srgb', 'never_stream', 'compression_no_alpha']
settings = {name: texture.get_editor_property(name) for name in properties}
out = root / 'Saved/Diagnostics/BottomNavDiscs'
package = root / 'Content/GameXXK/UI/ImageTruth/Training/T_TrainingNavTraining.uasset'
backup = out / 'T_TrainingNavTraining-before-green.uasset'
if not backup.exists():
    backup.write_bytes(package.read_bytes())

task = unreal.AssetImportTask()
task.filename = str(source)
task.destination_path = '/Game/GameXXK/UI/ImageTruth/Training'
task.destination_name = 'T_TrainingNavTraining'
task.automated = True
task.replace_existing = True
task.replace_existing_settings = False
task.save = False
if not already_imported:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset(asset)
assert isinstance(texture, unreal.Texture2D)
for name, value in settings.items():
    assert texture.get_editor_property(name) == value, f'Import changed {name}'
assert not texture.get_editor_property('compression_no_alpha')
assert unreal.EditorLoadingAndSavingUtils.save_packages([texture.get_outermost()], only_dirty=False)

export = unreal.AssetExportTask()
export.object = texture
export.filename = str(out / 'map-runtime-export.png')
export.automated = True
export.prompt = False
export.replace_identical = True
export.exporter = unreal.TextureExporterPNG()
assert unreal.Exporter.run_asset_export_task(export)
report = {'asset': texture.get_path_name(), 'source_sha256': manifest['sha256'],
          'runtime_size': [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()],
          'source': list(texture.get_editor_property('asset_import_data').extract_filenames()),
          'settings_preserved': {name: str(value) for name, value in settings.items()},
          'backup': str(backup), 'export': export.filename}
(out / 'map-import-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
manifest['runtime_imported'] = True
manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps(report, ensure_ascii=False))
