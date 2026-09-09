import json

import unreal

out = {}
try:
    settings = unreal.get_default_object(unreal.LevelEditorPlaySettings)
    props = [p for p in dir(settings) if not p.startswith("_") and ("window" in p.lower() or "view" in p.lower() or "play" in p.lower() or "size" in p.lower())]
    out["play_settings_props"] = sorted(props)
    for name in props:
        try:
            out[name] = str(settings.get_editor_property(name))
        except Exception:
            pass
except Exception as exc:
    out["error"] = str(exc)
print(json.dumps(out, ensure_ascii=False))
