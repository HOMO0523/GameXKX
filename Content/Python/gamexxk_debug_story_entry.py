"""Read-only evidence for a real task-entry failure; never imports a fixture."""
import json
from pathlib import Path
import unreal
root=Path(__file__).resolve().parents[2]
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
instance=unreal.GameplayStatics.get_game_instance(world)
dev=next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer()==instance)
result=json.loads(dev.execute_json(json.dumps({'schema':1,'command':'snapshot.export','args':{}})))
assert result['ok'],result
folder=root/'Saved/StorySystem/TaskEntryBug';folder.mkdir(parents=True,exist_ok=True)
(folder/'live-snapshot.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
state=result['data']['state']
keys=['playerLevel','playerXP','playerGold','playerHP','playerMaxHP','playerMP','playerMaxMP','enhancementMaterial','inventory','talents']
summary={key:state.get(key) for key in keys}
summary['cardRunRouteAttributeBonuses']=state['cardRun'].get('routeAttributeBonuses')
summary['refinementSand']=state['equipmentCollection'].get('refinementSand')
summary['session_active']=result.get('session_active')
summary['saved_dir']=unreal.Paths.project_saved_dir()
(folder/'resource-evidence.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(summary,ensure_ascii=False))
