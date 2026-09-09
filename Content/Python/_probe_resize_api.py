import json

import unreal

out = {}
out["unreal_texture_names"] = sorted([n for n in dir(unreal) if "texture" in n.lower() and not n.startswith("_")])
out["unreal_resize_names"] = sorted([n for n in dir(unreal) if "resize" in n.lower()])
try:
    editor_subsystem = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
    out["asset_editor_methods"] = sorted([n for n in dir(editor_subsystem) if not n.startswith("_")])
except Exception as exc:
    out["asset_editor"] = f"ERR:{exc}"
print(json.dumps(out, ensure_ascii=False))
