import json
import unreal
lib=unreal.MaterialEditingLibrary
mat=unreal.load_asset('/Game/GameXXK/UI/Materials/CardNameFlow/M_CardNameFlow')
result={'material':mat.get_path_name() if mat else None,'property_methods':[n for n in dir(lib) if 'material_property' in n]}
if mat and hasattr(lib,'get_material_property_input_node'):
    output=lib.get_material_property_input_node(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    result['emissive_input']=output.get_path_name() if output else None
result['instances']={}
for tier in ('Rare','Epic'):
    asset=unreal.load_asset('/Game/GameXXK/UI/Materials/CardNameFlow/MI_CardName'+tier+'Outline')
    result['instances'][tier]={}
    for key in ('ColorA','ColorB','SheenColor'):
        c=lib.get_material_instance_vector_parameter_value(asset,key)
        result['instances'][tier][key]=[c.r,c.g,c.b,c.a]
print(json.dumps(result))
