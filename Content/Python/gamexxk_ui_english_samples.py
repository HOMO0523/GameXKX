import json
import sys
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
instance=unreal.GameplayStatics.get_game_instance(world)
dev=next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer()==instance)
assert dev.is_session_active()
def run(command,args):
    result=json.loads(dev.execute_json(json.dumps({'schema':1,'command':command,'args':args})))
    assert result['ok'],result
    return result
mode=sys.argv[1]
if mode=='items':
    run('item.give',{'id':'Item.Gem.FireDamage.Common','quantity':1})
    run('equipment.create',{'id':'Equipment.PoJun.Shoes','level':5,'quality':1,'quantity':1,'equip':False})
elif mode=='cards':
    run('cards.set',{'character':'Player','cards':sys.argv[2:]})
elif mode=='boots':
    print(json.dumps(run('equipment.create',{'id':'Equipment.PoJun.Shoes','level':5,'quality':1,'quantity':1,'equip':True}),ensure_ascii=False))
print(json.dumps({'ok':True,'mode':mode}))
