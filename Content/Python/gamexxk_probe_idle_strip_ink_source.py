"""Read the current artist-edited idle texture without changing packages."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
out = root / 'Saved/Codex/IdleStripInkOutline-20260908'
out.mkdir(parents=True, exist_ok=True)
asset_path = '/Game/GameXXK/UI/ImageTruth/Training/T_TrainingIdleStrip_Background'
texture = unreal.load_asset(asset_path)
assert isinstance(texture, unreal.Texture2D)
report = {'asset': texture.get_path_name(),
          'size': [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()],
          'source': list(texture.get_editor_property('asset_import_data').extract_filenames()),
          'dirty_content_packages': [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]}
task = unreal.AssetExportTask()
task.object = texture
task.filename = str(out / 'current-runtime.png')
task.automated = True
task.prompt = False
task.replace_identical = True
task.exporter = unreal.TextureExporterPNG()
assert unreal.Exporter.run_asset_export_task(task)
report['export_sha256'] = hashlib.sha256((out / 'current-runtime.png').read_bytes()).hexdigest()
(out / 'before.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False))
