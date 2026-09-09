import json
import sys
import unreal
import gamexxk_verify_route_node_art as probe

world, controller, subsystem, dev = probe.context()
route = controller.get_route_map_widget_for_test()
print('libraries', [name for name in dir(unreal) if 'Slate' in name and 'Library' in name])
if hasattr(unreal, 'SlateLibrary'):
    print('root geometry', unreal.SlateLibrary.get_local_size(route.get_cached_geometry()))

def owned(obj):
    outer = obj.get_outer()
    for _ in range(6):
        if outer == route: return True
        if not outer: return False
        outer = outer.get_outer()
    return False

result = []
for cls in (unreal.TextBlock, unreal.SizeBox, unreal.Border, unreal.Overlay):
    for obj in unreal.ObjectIterator(cls):
        name = obj.get_name()
        if owned(obj) and (name.startswith('RouteLegend') or name.startswith('GameXXKRoute') or name == 'GameXXKOneGameRouteMapRoot'):
            size = obj.get_desired_size()
            data = {'name':name, 'desired':[size.x,size.y], 'type':obj.get_class().get_name()}
            if isinstance(obj,unreal.TextBlock): data.update(font=obj.get_editor_property('font').size, text=str(obj.get_text()))
            result.append(data)
print(json.dumps(result,ensure_ascii=False))
