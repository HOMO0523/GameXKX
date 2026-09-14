"""Capture existing PIE UI into native 1920x1080 frames in an isolated user profile.

Uses real game state and widgets, never synthetic UI or image generation.
The transparent desktop UI is rendered on a neutral matte to exclude the user's desktop.
"""
import argparse
import json
from pathlib import Path
import unreal

parser = argparse.ArgumentParser()
parser.add_argument('--phase', choices=['prepare', 'open', 'capture', 'battle', 'status', 'save'], required=True)
parser.add_argument('--action', type=int)
parser.add_argument('--label', default='12_Desktop_1920x1080_schinese')
args = parser.parse_args()

saved = str(unreal.Paths.project_saved_dir()).replace('\\', '/')
if 'SaveHardening/PreviewUser' not in saved:
    raise RuntimeError('Steam capture requires the isolated SaveHardening/PreviewUser directory')

if args.phase == 'prepare':
    unreal.EditorLoadingAndSavingUtils.load_map('/Game/GameXXK/Maps/L_DesktopTrainingHUD')
    print(json.dumps({'ok': True, 'phase': 'prepare'}))
else:
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    if not world:
        raise RuntimeError('PIE must be running')
    controller = unreal.GameplayStatics.get_player_controller(world, 0)
    instance = unreal.GameplayStatics.get_game_instance(world)
    mvp = next(x for x in unreal.ObjectIterator(unreal.GameXXKMVPSubsystem) if x.get_outer() == instance)
    widget = controller.get_desktop_training_workbench_widget_for_test()
    if args.phase == 'save':
        print(json.dumps({'ok': bool(mvp.save_current_game()), 'gold': mvp.get_runtime_state_copy().player_gold}))
    elif args.phase == 'status':
        save = unreal.GameplayStatics.load_game_from_slot('GameXXK_MVP_SaveSlot_1', 0)
        print(json.dumps({'ok': save is not None, 'version': save.save_state.save_version if save else None, 'error': str(mvp.get_last_save_load_error()), 'gold': mvp.get_runtime_state_copy().player_gold, 'user_dir': saved}))
    elif args.phase == 'open':
        widget.close_workbench()
        widget.open_workbench()
        widget.open_backpack()
        if args.action is not None:
            widget.handle_desktop_action_for_test(args.action)
        print(json.dumps({'ok': True, 'phase': 'open', 'action': args.action}))
    elif args.phase == 'battle':
        widget.handle_desktop_action_for_test(4)
        widget.handle_desktop_action_for_test(11)
        widget.handle_desktop_action_for_test(6)
        if mvp.get_training_progress_copy().challenge_active:
            mvp.select_training_challenge_route_node(1)
            controller.refresh_player_flow_widgets_for_test()
        print(json.dumps({'ok': bool(controller.get_battle_board_widget_for_test()), 'phase': 'battle', 'error': str(mvp.get_last_save_load_error())}))
    else:
        if 'Battle' in args.label:
            widget = controller.get_battle_board_widget_for_test()
        if not widget:
            raise RuntimeError('Requested live widget does not exist')
        folder = Path(unreal.Paths.project_dir()).resolve() / 'Deliverables/Steam/CloudFarer/05_screenshots'
        folder.mkdir(parents=True, exist_ok=True)
        path = folder / (args.label + '.png')
        ok = unreal.GameXXKEditorCaptureAutomationLibrary.capture_live_game_widget(widget, str(path), 1920, 1080)
        payload = {'ok': ok, 'path': str(path), 'world': world.get_path_name(),
                   'widget': widget.get_path_name(), 'capture': 'live PIE widget render on neutral matte',
                   'size': [1920, 1080], 'user_dir': saved}
        (folder / (args.label + '.json')).write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps(payload, ensure_ascii=False))
