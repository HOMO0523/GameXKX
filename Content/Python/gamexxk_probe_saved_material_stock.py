"""Read named player save objects without applying them to a game or writing a slot."""
import hashlib
import json
import os
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve()
source_dir=(root/'Saved/SaveGames').resolve()
engine_save_dir=(Path(unreal.Paths.project_saved_dir()).resolve()/'SaveGames').resolve()
records=[]
for name in ('GameXXK_MVP_SaveSlot_1','GameXXK_DesktopTraining_Checkpoint'):
    path=(source_dir/(name+'.sav')).resolve()
    assert path.parent==source_dir and path.is_file()
    before=hashlib.sha256(path.read_bytes()).hexdigest()
    # UE's raw SaveGame loader accepts a path relative to its current SaveGames
    # directory. This only deserializes an object; it never calls the game load flow.
    relative=os.path.relpath(path.with_suffix(''),engine_save_dir).replace('\\','/')
    obj=unreal.GameplayStatics.load_game_from_slot(relative,0)
    assert obj and isinstance(obj,unreal.GameXXKSaveGame),name
    state=obj.save_state.runtime_state
    bag={str(k):int(v) for k,v in state.inventory.items()}
    storage={str(k):int(v) for k,v in state.desktop_inventory.warehouse_items.items()}
    records.append({'file':str(path),'sha256':before,'readOnly':True,
        'materials':[{'id':item,'bag':bag.get(item,0),'storage':storage.get(item,0)} for item in ('Item.EnhancementStone','Item.RefinementSand')],
        'chests':len(state.training.owned_chest_tokens),'gold':state.player_gold,'level':state.player_level})
    assert hashlib.sha256(path.read_bytes()).hexdigest()==before
(root/'Saved/Codex/UIGuidanceLocalization-20260910/material-stock-on-disk.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(records,ensure_ascii=True))
