"""Read-only audit of the currently mounted workbench/battle UI."""
import json
import re
import sys
from pathlib import Path
import unreal

world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'UEDPIE_' in world.get_path_name(), 'Requires the existing player PIE; never starts it'
pc=unreal.GameplayStatics.get_player_controller(world,0)
roots=[pc.get_desktop_training_workbench_widget_for_test(),pc.get_battle_board_widget_for_test()]
records=[]
for widget in roots:
    if widget and widget.is_visible():
        value=json.loads(unreal.GameXXKLocalizationLibrary.audit_text_layout(widget))
        value['root']=widget.get_name();records.append(value)
stem=sys.argv[1] if len(sys.argv)>1 else 'current-layout-audit'
assert re.fullmatch(r'[a-z0-9-]+',stem)
out=Path(unreal.Paths.project_dir())/'Saved/Codex/UIGuidanceLocalization-20260910/ui-review'
(out/(stem+'.json')).write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps([{'root':r['root'],'language':r.get('language'),'visibleTexts':len(r.get('texts',[])),
    'unarranged':r.get('unarrangedTextCount'),'cjk':sum(t['chinese'] for t in r.get('texts',[])),
    'overflow':sum(t['potentialOverflow'] for t in r.get('texts',[]))} for r in records]))
