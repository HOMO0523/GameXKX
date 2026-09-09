"""Read and export current UE status textures for a visual audit; never save assets."""
import json
from pathlib import Path
import re
import unreal

root = Path(__file__).resolve().parents[2]
out = root / 'Saved/Codex/CardEffects-20260907/status-assets'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp').read_text(encoding='utf-8')
ids = set(re.findall(r'MakeStyle\(TEXT\("([^"]+)"\)', source))
ids.add('ArmorShield')
ids.discard('UnknownStatus')
rows = []
for icon in sorted(ids):
    resource = f'/Game/GameXXK/UI/Battle/StatusIcons/T_BattleStatus_{icon}'
    texture = unreal.load_asset(resource)
    record = {'id': icon, 'resource': resource, 'loaded': isinstance(texture, unreal.Texture2D)}
    if record['loaded']:
        record['width'] = texture.blueprint_get_size_x()
        record['height'] = texture.blueprint_get_size_y()
        task = unreal.AssetExportTask()
        task.object = texture
        task.filename = str(out / f'{icon}-ue.png')
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        task.exporter = unreal.TextureExporterPNG()
        record['exported'] = unreal.Exporter.run_asset_export_task(task)
    rows.append(record)
(out/'inventory.json').write_text(json.dumps(rows, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'count': len(rows), 'loaded': sum(r['loaded'] for r in rows), 'exported': sum(r.get('exported',False) for r in rows), 'path': str(out)}, ensure_ascii=False))
