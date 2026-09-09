import json
import unreal
result=[]
for i in (3,4,5):
    t=unreal.load_asset(f'/Game/GameXXK/UI/Battle/VFX/UltimateLightning057/T_UltimateLightning057_Atlas_{i:02d}')
    row={'atlas':i,'size':[t.blueprint_get_size_x(),t.blueprint_get_size_y()]}
    for p in ('compression_settings','compression_no_alpha','srgb','lod_group','mip_gen_settings','filter','never_stream','adjust_brightness','adjust_brightness_curve','adjust_saturation','adjust_vibrance','adjust_hue','adjust_min_alpha','adjust_max_alpha'):
        try:row[p]=str(t.get_editor_property(p))
        except Exception as e:row[p]=str(e)
    row['source']=t.get_editor_property('asset_import_data').get_first_filename()
    result.append(row)
print(json.dumps(result))
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([unreal.load_asset('/Game/GameXXK/UI/Battle/VFX/UltimateLightning057/T_UltimateLightning057_Atlas_04')])
