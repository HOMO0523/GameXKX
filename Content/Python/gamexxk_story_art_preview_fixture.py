"""Isolated, restorable art preview through the project's public development API."""
import json
from pathlib import Path
import sys
import unreal

ROOT = Path(__file__).resolve().parents[2]
saved = unreal.Paths.project_saved_dir().replace('\\', '/')
assert any(folder in saved for folder in ('StorySystem/ArtEdgePreviewUser/','Saved/InteractiveEditorUser/')), 'Never prepare the art fixture in a player save'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and 'L_DesktopTrainingHUD' in world.get_name()
instance = unreal.GameplayStatics.get_game_instance(world)
dev = next(x for x in unreal.ObjectIterator(unreal.GameXXKDevToolsSubsystem) if x.get_outer() == instance)
mode = sys.argv[1] if len(sys.argv) > 1 else 'export'
def execute(command, args=None):
    result = json.loads(dev.execute_json(json.dumps({'schema': 1, 'command': command, 'args': args or {}})))
    assert result['ok'], result
    return result
if mode == 'export':
    result = execute('snapshot.export')
    (ROOT / 'Saved/StorySystem/art-preview-scene.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'exported': True, 'keys': list(result)}))
elif mode == 'restore':
    print(json.dumps(execute('session.restore'), ensure_ascii=False))
elif mode == 'show-chapter':
    chapter_id=sys.argv[2]
    campaign=json.loads((ROOT/'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))
    assert chapter_id in [c['id'] for c in campaign['chapters']]
    scene=execute('snapshot.export')['data']
    state=scene['state'];assert not state['training']['bChallengeActive']
    narrative=state['narrativeProgress'];assert narrative['mainStory']['activeNodeId']=='None'
    stage_ids=['Training.Normal.1-1','Training.Normal.1-2','Training.Normal.1-3','Training.Normal.2-1','Training.Normal.2-2','Training.Normal.2-3']
    state['training']['clearedStageIds']=sorted(set(state['training']['clearedStageIds']+stage_ids))
    for chapter in campaign['chapters']:
        ids=[node['id'] for node in chapter['nodes']]
        narrative['storyProgressById']['Story.Main.Cartography.'+chapter['id']]={'version':1,'state':'Completed','activeTaskIds':[],'completedTaskIds':ids}
        for node_id in ids:
            narrative['taskProgressById'][node_id]={'state':'Rewarded','currentStepId':node_id+'.Goal','objectiveCounts':{},'bRewardCommitted':True}
    narrative['mainStory']['mainlineCompletedChapters']=[c['id'] for c in campaign['chapters']]
    imported=execute('snapshot.import',{'scene':scene});assert imported['session_active']
    story=next(x for x in unreal.ObjectIterator(unreal.GameXXKMainStorySubsystem) if x.get_outer()==instance)
    assert story.open_chapter(unreal.Name(chapter_id))
    wb=unreal.GameplayStatics.get_player_controller(world,0).get_desktop_training_workbench_widget_for_test()
    wb.open_backpack();wb.handle_desktop_action_for_test(2100+int(chapter_id[1:]))
    print(json.dumps({'art_fixture':True,'chapter':chapter_id,'writes_suppressed':True,'scope':'Art/dialogue replay only; not progression acceptance'}))
elif mode == 'show-s00':
    scene=execute('snapshot.export')['data']
    assert not scene['state']['training']['bChallengeActive']
    narrative=scene['state']['narrativeProgress']
    assert narrative['mainStory']['activeNodeId']=='None'
    chapter=json.loads((ROOT/'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))['chapters'][0]
    ids=[node['id'] for node in chapter['nodes']]
    narrative['storyProgressById']['Story.Main.Cartography.S00']={'version':1,'state':'Completed','activeTaskIds':[],'completedTaskIds':ids}
    for node_id in ids:
        narrative['taskProgressById'][node_id]={'state':'Rewarded','currentStepId':node_id+'.Goal','objectiveCounts':{},'bRewardCommitted':True}
    narrative['mainStory']['mainlineCompletedChapters']=list(set(narrative['mainStory']['mainlineCompletedChapters']+['S00']))
    imported=execute('snapshot.import',{'scene':scene})
    assert imported['session_active']
    story=next(x for x in unreal.ObjectIterator(unreal.GameXXKMainStorySubsystem) if x.get_outer()==instance)
    assert story.open_chapter(unreal.Name('S00'))
    wb=unreal.GameplayStatics.get_player_controller(world,0).get_desktop_training_workbench_widget_for_test()
    wb.open_backpack();wb.handle_desktop_action_for_test(2100)
    print(json.dumps({'art_fixture':True,'chapter':'S00','writes_suppressed':True,'scope':'Art and dialogue replay preview, not progression acceptance'}))
elif mode == 'scroll':
    panels=[p for p in unreal.ObjectIterator(unreal.GameXXKMainStoryPanelWidget) if p.get_parent() and p.get_world()==world]
    moved=[]
    for scroll in unreal.ObjectIterator(unreal.ScrollBox):
        if scroll.get_name().startswith('MainStoryTreeScroll') and scroll.get_parent() and scroll.get_outer().get_outer() in panels:
            scroll.set_scroll_offset(float(sys.argv[2]));moved.append(scroll.get_path_name())
    assert moved
    print(json.dumps({'scrolled':moved,'offset':float(sys.argv[2])}))
elif mode == 'show':
    scene = execute('snapshot.export')['data']
    state = scene['state']
    # Visual fixtures use the same validated, write-suppressed development
    # import as other project art checks; restore the session after screenshots.
    state['training']['clearedStageIds'] = sorted(set(state['training']['clearedStageIds'] + ['Training.Normal.1-1', 'Training.Normal.1-2']))
    narrative = state['narrativeProgress']
    narrative['mainStory']['mainlineCompletedChapters'] = ['S00', 'S01']
    campaign = json.loads((ROOT / 'SourceAssets/Narrative/MainStory/campaign.json').read_text(encoding='utf-8'))
    for chapter in campaign['chapters'][:2]:
        narrative['storyProgressById']['Story.Main.Cartography.' + chapter['id']] = {
            'version': 1, 'state': 'Completed', 'activeTaskIds': [], 'completedTaskIds': [n['id'] for n in chapter['nodes']]}
        for node in chapter['nodes']:
            narrative['taskProgressById'][node['id']] = {'state': 'Rewarded', 'currentStepId': node['id'] + '.Goal', 'objectiveCounts': {}, 'bRewardCommitted': True}
    narrative['storyProgressById']['Story.Main.Cartography.S02'] = {'version': 1, 'state': 'Active', 'activeTaskIds': [], 'completedTaskIds': ['S02-01']}
    narrative['taskProgressById']['S02-01'] = {'state': 'Rewarded', 'currentStepId': 'S02-01.Goal', 'objectiveCounts': {}, 'bRewardCommitted': True}
    imported = execute('snapshot.import', {'scene': scene})
    assert imported['session_active']
    story = next(x for x in unreal.ObjectIterator(unreal.GameXXKMainStorySubsystem) if x.get_outer() == instance)
    assert story.open_chapter(unreal.Name('S02'))
    workbench = unreal.GameplayStatics.get_player_controller(world, 0).get_desktop_training_workbench_widget_for_test()
    workbench.open_backpack()
    workbench.handle_desktop_action_for_test(2102)
    print(json.dumps({'art_fixture': True, 'writes_suppressed': True, 'saved_dir': saved, 'chapter': 'S02'}))
