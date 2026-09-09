"""Import only the requested warehouse/training contour art revisions."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
folder = root / 'SourceArt/UI/ImageTruth/revisions/20260909-navigation-contour-v2'
manifest_path = folder / 'manifest.json'
manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
out = root / 'Saved/Diagnostics/BottomNavDiscs'
properties = ['compression_settings', 'mip_gen_settings', 'lod_group', 'filter',
              'address_x', 'address_y', 'srgb', 'never_stream', 'compression_no_alpha',
              'max_texture_size', 'lod_bias', 'resize_during_build_x', 'resize_during_build_y']
reports = []
for entry in manifest['images']:
    source = root / entry['final']
    assert hashlib.sha256(source.read_bytes()).hexdigest() == entry['sha256']
    asset = entry['asset']
    texture = unreal.load_asset(asset)
    assert isinstance(texture, unreal.Texture2D), asset
    already = source.resolve() in [Path(p).resolve() for p in texture.get_editor_property('asset_import_data').extract_filenames()]
    assert already or not any(p.get_name() == asset for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()), 'Target has unrelated unsaved edits'
    settings = {name: texture.get_editor_property(name) for name in properties}
    backup = out / ('T_TrainingNav' + entry['kind'] + '-before-contour-v2.uasset')
    if not backup.exists():
        backup.write_bytes((root / ('Content' + asset.removeprefix('/Game') + '.uasset')).read_bytes())
    if not already:
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = '/Game/GameXXK/UI/ImageTruth/Training'
        task.destination_name = 'T_TrainingNav' + entry['kind']
        task.automated = True
        task.replace_existing = True
        task.replace_existing_settings = False
        task.save = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(asset)
    for name, value in settings.items():
        assert texture.get_editor_property(name) == value, f'{asset}: changed {name}'
    assert not texture.get_editor_property('compression_no_alpha')
    assert unreal.EditorLoadingAndSavingUtils.save_packages([texture.get_outermost()], only_dirty=False)
    export = unreal.AssetExportTask()
    export.object = texture
    export.filename = str(out / (entry['kind'] + '-contour-v2-runtime-export.png'))
    export.automated = True
    export.prompt = False
    export.replace_identical = True
    export.exporter = unreal.TextureExporterPNG()
    assert unreal.Exporter.run_asset_export_task(export)
    reports.append({'asset': asset, 'source_sha256': entry['sha256'], 'backup': str(backup),
                    'export': export.filename, 'runtime_size': [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()],
                    'settings_preserved': {key: str(value) for key, value in settings.items()}})
(out / 'contour-v2-import-report.json').write_text(json.dumps(reports, ensure_ascii=False, indent=2), encoding='utf-8')
manifest['runtime_imported'] = True
manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print(json.dumps(reports, ensure_ascii=False))
