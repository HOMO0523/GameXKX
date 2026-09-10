"""Restorable English story presentation review; never awards real player progress."""
import json
from pathlib import Path
import sys
import unreal

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Saved/StorySystem/Localization';OUT.mkdir(parents=True,exist_ok=True)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
instance=unreal.GameplayStatics.get_game_instance(world)
pc=unreal.GameplayStatics.get_player_controller(world,0)
dev=next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer()==instance)
story=next(x for x in unreal.ObjectIterator(unreal.GameXXKMainStorySubsystem) if x.get_outer()==instance)

def command(name,args=None):
    result=json.loads(dev.execute_json(json.dumps({'schema':1,'command':name,'args':args or {}})))
    assert result['ok'],result
    return result

mode=sys.argv[1] if len(sys.argv)>1 else 'audit'
if mode=='prepare':
    assert not dev.is_session_active()
    mvp=next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer()==instance)
    assert mvp.save_current_game('',0), str(mvp.get_last_save_load_error())
    original=command('snapshot.export')
    (OUT/'ui-original.json').write_text(json.dumps(original,ensure_ascii=False,indent=2),encoding='utf-8')
    language=unreal.GameXXKLocalizationLibrary.get_language()
    (OUT/'ui-language.json').write_text(json.dumps({'language':language}),encoding='utf-8')
    command('session.begin')
    scene=command('snapshot.export')['data'];state=scene['state']
    assert not state['training']['bChallengeActive'], 'Requires workbench; preserve an active player battle'
    campaign=json.loads((ROOT/'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))
    stage_ids=['Training.Normal.1-1','Training.Normal.1-2','Training.Normal.1-3','Training.Normal.2-1','Training.Normal.2-2','Training.Normal.2-3']
    state['training']['clearedStageIds']=sorted(set(state['training']['clearedStageIds']+stage_ids))
    narrative=state['narrativeProgress'];narrative['trackedTaskId']='None'
    main_ids={n['id'].upper() for c in campaign['chapters'] for n in c['nodes']}
    narrative['taskProgressById']={k:v for k,v in narrative['taskProgressById'].items() if k.upper() not in main_ids}
    narrative['storyProgressById']={k:v for k,v in narrative['storyProgressById'].items() if not k.lower().startswith('story.main.cartography.')}
    for chapter in campaign['chapters']:
        ids=[n['id'] for n in chapter['nodes']]
        narrative['storyProgressById']['Story.Main.Cartography.'+chapter['id']]={'version':1,'state':'Completed','activeTaskIds':[],'completedTaskIds':ids}
        for node_id in ids:
            narrative['taskProgressById'][node_id]={'state':'Rewarded','currentStepId':node_id+'.Goal','objectiveCounts':{},'bRewardCommitted':True}
    narrative['mainStory']['activeNodeId']='None';narrative['mainStory']['phase']='None';narrative['mainStory']['lineIndex']=0
    narrative['mainStory']['mainlineCompletedChapters']=[c['id'] for c in campaign['chapters']]
    command('snapshot.import',{'scene':scene})
    assert unreal.GameXXKLocalizationLibrary.set_language('en',False)
    pc.refresh_player_flow_widgets_from_state()
elif mode in ('chapter','node'):
    assert dev.is_session_active()
    chapter=sys.argv[2][:3]
    wb=pc.get_desktop_training_workbench_widget_for_test()
    wb.open_backpack();wb.handle_desktop_action_for_test(2100+int(chapter[1:]))
    if mode=='node':assert story.start_task(unreal.Name(sys.argv[2]))
elif mode=='restore':
    command('session.restore')
    original=json.loads((OUT/'ui-original.json').read_text(encoding='utf-8'))
    after=command('snapshot.export')
    assert original['data']['state']==after['data']['state']
    language=json.loads((OUT/'ui-language.json').read_text(encoding='utf-8'))['language']
    assert unreal.GameXXKLocalizationLibrary.set_language(language,False)
    assert not dev.is_session_active()
    pc.refresh_player_flow_widgets_from_state()

panels=[p for p in unreal.ObjectIterator(unreal.GameXXKMainStoryPanelWidget) if p.get_parent()]
report={'mode':mode,'dev_session':dev.is_session_active(),'language':unreal.GameXXKLocalizationLibrary.get_language(),
        'panels':[json.loads(unreal.GameXXKLocalizationLibrary.audit_text_layout(p)) for p in panels]}
if mode=='audit':
    for p in unreal.ObjectIterator(unreal.GameXXKDialoguePanelWidget):
        if p.get_owning_player()==pc and p.get_visibility()!=unreal.SlateVisibility.COLLAPSED:
            report['dialogue']=json.loads(unreal.GameXXKLocalizationLibrary.audit_text_layout(p))
(OUT/('ui-'+mode+'.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False))
