
import json, unreal
obj = unreal.load_asset('/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_HeroFullBody')
out = {'loaded': obj is not None}
if obj:
    out['size'] = [obj.blueprint_get_size_x(), obj.blueprint_get_size_y()]
print(json.dumps(out, ensure_ascii=True))
