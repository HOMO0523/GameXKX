"""Read-only catalogue diagnostics; does not change language or player state."""
import json
from pathlib import Path
import unreal
root=Path(__file__).resolve().parents[2]
out=root/'Saved/Codex/UIGuidanceLocalization-20260910/localization-missing-current.json'
rows={'missing':list(unreal.GameXXKLocalizationLibrary.get_missing_sources()),'textBlocks':[]}
for text in unreal.ObjectIterator(unreal.TextBlock):
    value=str(text.get_text())
    if value and any('\u3400'<=c<='\u9fff' for c in value):
        rows['textBlocks'].append({'path':text.get_path_name(),'text':value})
out.write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'missing':rows['missing'],'path':str(out)},ensure_ascii=True))
