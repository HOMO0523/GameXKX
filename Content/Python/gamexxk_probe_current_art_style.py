"""Export a bounded set of currently referenced 2D textures without saving assets."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
out = root / 'Saved/Codex/CardEffects-20260907/art-style'
out.mkdir(parents=True, exist_ok=True)
paths = [
    '/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Plain_GeneratedV2',
    '/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Forest_GeneratedV2',
    '/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Village_GeneratedV2',
    '/Game/GameXXK/UI/ImageTruth/Training/T_TrainingIdleStrip_Background',
]
records = []
for path in paths:
    texture = unreal.load_asset(path)
    record = {'asset': path, 'loaded': isinstance(texture, unreal.Texture2D)}
    if record['loaded']:
        destination = out / (path.rsplit('/', 1)[1] + '.png')
        task = unreal.AssetExportTask()
        task.object = texture
        task.filename = str(destination)
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        task.exporter = unreal.TextureExporterPNG()
        record['exported'] = unreal.Exporter.run_asset_export_task(task)
        if record['exported']:
            record['file'] = str(destination)
            record['sha256'] = hashlib.sha256(destination.read_bytes()).hexdigest()
        record['source'] = list(texture.get_editor_property('asset_import_data').extract_filenames())
    records.append(record)
(out / 'inventory.json').write_text(json.dumps(records, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(records, ensure_ascii=False))
