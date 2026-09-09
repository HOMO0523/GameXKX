from pathlib import Path
import json
import unreal
rows=[]
for widget in unreal.ObjectIterator(unreal.GameXXKAsyncStoryImage):
    if not widget.get_parent():continue
    brush=widget.get_editor_property('brush')
    resource=brush.get_editor_property('resource_object')
    material=resource if isinstance(resource,unreal.MaterialInstanceDynamic) else None
    texture=None
    if isinstance(material,unreal.MaterialInstanceDynamic):
        texture=material.get_texture_parameter_value(unreal.Name('Illustration'))
    if not texture and isinstance(resource,unreal.Texture2D):texture=resource
    rows.append(dict(widget=widget.get_path_name(),texture=texture.get_path_name() if texture else None,
        soft_edges=isinstance(material,unreal.MaterialInstanceDynamic),
        material_parent=material.get_editor_property('parent').get_path_name() if material else None,
        memory=texture.blueprint_get_memory_size() if texture else 0))
root=Path(__file__).resolve().parents[2]
(root/'Saved/ImageOptimization/live-story-images.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
print(json.dumps(rows))
