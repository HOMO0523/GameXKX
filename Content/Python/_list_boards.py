import json
import unreal as u

found = []
for obj in u.ObjectIterator():
    name = type(obj).__name__
    if 'BattleBoard' in name:
        found.append(str(obj))
print(json.dumps(found, ensure_ascii=False))
