import json
from pathlib import Path
import unreal
def walk(widget):
    if isinstance(widget,unreal.TextBlock):yield str(widget.get_text())
    if isinstance(widget,unreal.PanelWidget):
        for child in widget.get_all_children():yield from walk(child)
rows=[]
for tooltip in unreal.ObjectIterator(unreal.GameXXKCardTooltipWidget):
    tree=unreal.find_object(tooltip,'WidgetTree')
    title=unreal.find_object(tree,'CardTooltipTitle') if tree else None
    if not title or str(title.get_text()) not in ('碎岩击','Rockbreaker','横剑守势','Blade Guard'):continue
    body=unreal.find_object(tree,'CardTooltipBody')
    rows.append({'title':str(title.get_text()),'path':tooltip.get_path_name(),'body':list(walk(body))})
out=Path(__file__).resolve().parents[2]/'Saved/Codex/UIGuidanceLocalization-20260910/sample-card-tooltips.json'
out.write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(rows,ensure_ascii=True))
