import json

import unreal

out = {"play_names": sorted([n for n in dir(unreal) if "play" in n.lower() and "editor" in n.lower()])}
out["level_editor_names"] = sorted([n for n in dir(unreal) if "leveleditor" in n.lower()])
print(json.dumps(out, ensure_ascii=False))
