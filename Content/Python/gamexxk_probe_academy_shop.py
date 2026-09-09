import json
import sys
import unreal
import gamexxk_probe_training_visual_mvp as base

world,pc,workbench=base._controller_and_widget()
if not world: raise RuntimeError('Canonical desktop PIE is required')
instance=unreal.GameplayStatics.get_game_instance(world)
academy=next((o for o in unreal.ObjectIterator(unreal.GameXXKAcademySubsystem) if o.get_outer()==instance),None)
mode=sys.argv[1] if len(sys.argv)>1 else 'state'
result={'mode':mode}
if mode=='begin': result['accepted']=academy.begin_course(sys.argv[2])
elif mode=='cancel': academy.cancel_course()
elif mode=='open': workbench.open_backpack()
elif mode=='action': workbench.handle_desktop_action_for_test(int(sys.argv[2]))
elif mode=='click':
    tree=unreal.find_object(workbench,'WidgetTree') or unreal.find_object(workbench,'DesktopTrainingWorkbenchWidgetTree')
    root=unreal.find_object(tree,'DesktopTrainingOverlayRoot')
    pending=[root];found=None
    while pending:
        widget=pending.pop()
        if widget.get_name()==sys.argv[2]:found=widget;break
        if isinstance(widget,unreal.PanelWidget):pending.extend(widget.get_all_children())
    if not found:raise RuntimeError('Visible button missing: '+sys.argv[2])
    found.call_method('HandleClicked')
result['academy_present']=academy is not None
result['texts']=[]
result['shop_buttons']=[]
tree=unreal.find_object(workbench,'WidgetTree') or unreal.find_object(workbench,'DesktopTrainingWorkbenchWidgetTree')
if tree:
    pending=[unreal.find_object(tree,'DesktopTrainingOverlayRoot')]
    while pending:
        widget=pending.pop()
        if isinstance(widget,unreal.TextBlock):result['texts'].append(str(widget.get_text()))
        if isinstance(widget,unreal.Button) and widget.get_name().startswith('Shop'):
            result['shop_buttons'].append({'name':widget.get_name(),'enabled':widget.get_is_enabled()})
        if isinstance(widget,unreal.PanelWidget):pending.extend(widget.get_all_children())
print(json.dumps(result,ensure_ascii=False))
