"""Read-only live chest/inventory evidence. Does not open a chest or save."""
import json
from pathlib import Path
import unreal
root=Path(__file__).resolve().parents[2]
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
report={'pie':bool(world)}
if world:
    instance=unreal.GameplayStatics.get_game_instance(world)
    mvp=next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer()==instance)
    dev=next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer()==instance)
    snapshot=json.loads(dev.execute_json(json.dumps({'schema':1,'command':'snapshot.export','args':{}})))
    assert snapshot['ok']
    out=root/'Saved/StorySystem/ChestBlock';out.mkdir(parents=True,exist_ok=True)
    (out/'live-snapshot.json').write_text(json.dumps(snapshot,ensure_ascii=False,indent=2),encoding='utf-8')
    state=snapshot['data']['state']
    report.update(dev_session=dev.is_session_active(),last_save_error=str(mvp.get_last_save_load_error()),
                  gold=state.get('playerGold'),screen=state.get('screen'),
                  inventory=state.get('inventory'),desktop_inventory=state.get('desktopInventory'),
                  chests=state.get('training',{}).get('ownedChestTokens'))
    report['notices']=[{'name':w.get_name(),'text':str(w.get_text())} for w in unreal.ObjectIterator(unreal.TextBlock)
                       if 'Notice' in w.get_name() and str(w.get_text())]
    (out/'live-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False))
