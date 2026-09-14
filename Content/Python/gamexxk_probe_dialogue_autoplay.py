"""Exercise the actual story dialogue widget in an isolated PIE profile."""
import argparse
import json
from pathlib import Path
import unreal

parser=argparse.ArgumentParser()
parser.add_argument('--phase', choices=['prepare','start','choice','enable','status','disable','capture'],required=True)
args=parser.parse_args()
saved=str(unreal.Paths.project_saved_dir()).replace('\\','/')
if 'DialogueAutoPlay/PreviewUser' not in saved:
    raise RuntimeError('This probe requires the isolated DialogueAutoPlay/PreviewUser profile')
if args.phase=='prepare':
    unreal.EditorLoadingAndSavingUtils.load_map('/Game/GameXXK/Maps/L_DesktopTrainingHUD')
    print(json.dumps({'ok':True}))
else:
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world:raise RuntimeError('PIE is not running')
    instance=unreal.GameplayStatics.get_game_instance(world)
    mvp=next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer()==instance)
    story=next(x for x in unreal.ObjectIterator(unreal.GameXXKMainStorySubsystem) if x.get_outer()==instance)
    controller=unreal.GameplayStatics.get_player_controller(world,0)
    if args.phase in ['start','choice']:
        mvp.start_new_game()
        workbench=controller.get_desktop_training_workbench_widget_for_test()
        workbench.open_workbench();workbench.open_backpack()
        workbench.handle_desktop_action_for_test(654)
        workbench.handle_desktop_action_for_test(2100)
        if not story.start_task(unreal.Name('S00-01')):raise RuntimeError('Cannot start initial story')
        if args.phase=='choice':
            for node in ['S00-01','S00-02']:
                if node!='S00-01' and not story.start_task(unreal.Name(node)):raise RuntimeError('Cannot start '+node)
                for _ in range(30):
                    if not story.advance_dialogue():break
                if not story.claim_reward(unreal.Name(node)):raise RuntimeError('Cannot claim '+node)
            if not story.start_task(unreal.Name('S00-03')):raise RuntimeError('Cannot start choice task')
            for _ in range(30):
                if not story.advance_dialogue():break
        print(json.dumps({'ok':True}))
    else:
        panels=[x for x in unreal.ObjectIterator(unreal.GameXXKDialoguePanelWidget) if x.get_world()==world]
        if not panels:
            wb=controller.get_desktop_training_workbench_widget_for_test()
            Path(unreal.Paths.project_dir(),'Saved/DialogueAutoPlay/diagnostic.txt').write_text(str([(x.get_path_name(),str(x.get_visibility()),str(x.get_world())) for x in unreal.ObjectIterator(unreal.GameXXKDialoguePanelWidget)])+'\nTicks '+str(wb.get_travel_visual_native_tick_count_for_test())+' Visible '+str(wb.is_visible()),encoding='utf-8')
            raise RuntimeError('No visible live dialogue panel; see diagnostic.txt')
        panel=panels[0]
        if args.phase in ['enable','disable']:panel.set_auto_play_enabled(args.phase=='enable')
        state=mvp.get_runtime_state_copy().narrative_progress.main_story
        result={'ok':True,'enabled':panel.is_auto_play_enabled(),'delay':panel.get_auto_play_delay_seconds(),
                'node':str(state.active_node_id),'line':state.line_index,'phase':str(state.phase),
                'visible':panel.is_visible(),'world':world.get_path_name(),
                'diagnostic':panel.get_auto_play_status_for_test(),
                'ticks':controller.get_desktop_training_workbench_widget_for_test().get_travel_visual_native_tick_count_for_test()}
        if args.phase=='capture':
            root=Path(unreal.Paths.project_dir()).resolve()
            result['capture']=unreal.GameXXKEditorCaptureAutomationLibrary.capture_live_game_widget(
                controller.get_desktop_training_workbench_widget_for_test(),str(root/'Saved/DialogueAutoPlay/live-auto.png'),1920,1080)
        print(json.dumps(result,ensure_ascii=False))
