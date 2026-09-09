import json
import unreal
rows=[]
for w in unreal.ObjectIterator(unreal.GameXXKMainStoryPanelWidget):
    owner=w.get_owning_player()
    instance=unreal.GameplayStatics.get_game_instance(w)
    rows.append(dict(name=w.get_path_name(),tick=str(w.get_editor_property('tick_frequency')),owner=owner.get_path_name() if owner else None,
        instance=instance.get_path_name() if instance else None,
        visible=str(w.get_visibility()),in_viewport=w.is_in_viewport()))
print(json.dumps(rows))
