"""Read-only editor scaling preference; never changes display settings."""
import json
import unreal
result={}
for name in ('EditorStyleSettings','EditorAppearanceSettings'):
    cls=getattr(unreal,name,None)
    if not cls:continue
    obj=unreal.get_default_object(cls)
    try:result[name]=obj.get_editor_property('application_scale')
    except Exception as exc:result[name]=str(exc)
print(json.dumps(result))
