"""Replace only the requested idle background, preserving old art and settings."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
source_dir = root / 'SourceArt/UI/Training/IdleStrip/QuietInkV24'
manifest = json.loads((source_dir / 'manifest.json').read_text(encoding='utf-8'))
source = root / manifest['final']
assert hashlib.sha256(source.read_bytes()).hexdigest() == manifest['sha256']
asset_path = '/Game/GameXXK/UI/ImageTruth/Training/T_TrainingIdleStrip_Background'
texture = unreal.load_asset(asset_path)
assert isinstance(texture, unreal.Texture2D), asset_path
current_sources = [Path(p).resolve() for p in texture.get_editor_property('asset_import_data').extract_filenames()]
already_imported = source.resolve() in current_sources
assert already_imported or not any(p.get_name() == asset_path for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()), 'Target has unsaved edits'
properties = ['compression_settings', 'mip_gen_settings', 'lod_group', 'filter',
              'address_x', 'address_y', 'srgb', 'never_stream']
settings = {name: texture.get_editor_property(name) for name in properties}
out = root / 'Saved/Codex/IdleStripBackground-20260907'
backup = out / 'original/T_TrainingIdleStrip_Background.uasset'
package = root / 'Content/GameXXK/UI/ImageTruth/Training/T_TrainingIdleStrip_Background.uasset'
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    backup.write_bytes(package.read_bytes())

task = unreal.AssetImportTask()
task.filename = str(source)
task.destination_path = '/Game/GameXXK/UI/ImageTruth/Training'
task.destination_name = 'T_TrainingIdleStrip_Background'
task.automated = True
task.replace_existing = True
task.replace_existing_settings = False
task.save = False
if not already_imported:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset(asset_path)
assert isinstance(texture, unreal.Texture2D)
assert [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()] == [1983, 793]
for name, value in settings.items():
    assert texture.get_editor_property(name) == value, f'Import changed {name}'
assert unreal.EditorLoadingAndSavingUtils.save_packages([texture.get_outermost()], only_dirty=False)

export_path = out / 'after-runtime-texture.png'
export = unreal.AssetExportTask()
export.object = texture
export.filename = str(export_path)
export.automated = True
export.prompt = False
export.replace_identical = True
export.exporter = unreal.TextureExporterPNG()
assert unreal.Exporter.run_asset_export_task(export)
report = {'status': 'import-pass', 'asset': texture.get_path_name(),
          'source': list(texture.get_editor_property('asset_import_data').extract_filenames()),
          'source_sha256': manifest['sha256'], 'old_package_backup': str(backup),
          'old_package_sha256': hashlib.sha256(backup.read_bytes()).hexdigest(),
          'new_package_sha256': hashlib.sha256(package.read_bytes()).hexdigest(),
          'settings_preserved': {k: str(v) for k,v in settings.items()},
          'export': str(export_path)}
(out / 'import-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False))
