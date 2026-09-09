import json
import unreal
paths=['/Game/GameXXK/UI/Materials/EquipmentQuality/M_EquipmentNameFlow','/Game/GameXXK/UI/Materials/EquipmentQuality/M_EquipmentSurface']
out={}
for path in paths:
    asset=unreal.load_asset(path)
    if not asset:raise RuntimeError('Missing quality material: '+path)
    out[path]=str(unreal.MaterialEditingLibrary.get_statistics(asset))
print(json.dumps(out))
