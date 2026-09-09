"""Preserve approved art and author shared UI cutout materials for mechanic icons."""

def save_asset_with_texture_budget(*args, **kwargs):
    from gamexxk_texture_budget import save_loaded_asset
    return save_loaded_asset(*args, unreal_module=unreal, **kwargs)

import hashlib
import json
from pathlib import Path
import shutil
import unreal
from gamexxk_author_card_name_flow import node,connect,scalar

root=Path(unreal.Paths.project_dir())
source=root/'SourceArt/UI/Battle/MechanicIcons/Draft_20260907'
approved=root/'SourceArt/UI/Battle/MechanicIcons/Approved_20260908'
approved.mkdir(parents=True,exist_ok=True)
dest='/Game/GameXXK/UI/Battle/MechanicIcons'
unreal.EditorAssetLibrary.make_directory(dest)
specs=[('Lightning','mage_lightning-draft-v4.png',0),('Fire','mage_fire-draft-v4.png',1),
       ('Ice','mage_ice_snowflake-draft-v7.png',1),('Universal','mage_universal-draft-v4.png',1),
       ('Formula','healer_formula-draft-v4.png',1)]
textures={}
report=[]
for key,filename,remove in specs:
 original=source/filename
 copied=approved/(key.lower()+'.png')
 shutil.copyfile(original,copied)
 assert original.read_bytes()==copied.read_bytes()
 name='T_Mechanic_'+key
 task=unreal.AssetImportTask();task.filename=str(copied);task.destination_path=dest;task.destination_name=name
 task.automated=True;task.replace_existing=True;task.save=True
 unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
 texture=unreal.load_asset(dest+'/'+name)
 texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
 texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
 texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
 texture.set_editor_property('srgb',True);texture.set_editor_property('never_stream',True)
 save_asset_with_texture_budget(texture)
 textures[key]=texture
 report.append({'key':key,'source':str(original),'preserved_copy':str(copied),
                'sha256':hashlib.sha256(original.read_bytes()).hexdigest(),'remove_neutral_checker_in_material':bool(remove)})

lib=unreal.MaterialEditingLibrary
path=dest+'/M_MechanicIconCutout'
material=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_MechanicIconCutout',dest,unreal.Material,unreal.MaterialFactoryNew())
material.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI)
material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
for expression in lib.get_material_expressions(material):lib.delete_material_expression(material,expression)
uv=node(material,unreal.MaterialExpressionTextureCoordinate)
texture=node(material,unreal.MaterialExpressionTextureObjectParameter)
texture.set_editor_property('parameter_name','IconTexture');texture.set_editor_property('texture',textures['Fire'])
cutout=node(material,unreal.MaterialExpressionCustom)
cutout.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
inputs=[]
for name in ['UV','IconTexture','RemoveNeutralBackdrop']:
 item=unreal.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
cutout.set_editor_property('inputs',inputs)
cutout.set_editor_property('description','Keep approved colored ink/paper; make only the neutral proof backdrop transparent')
cutout.set_editor_property('code','''
float4 pixel=Texture2DSample(IconTexture,IconTextureSampler,UV);
float highest=max(pixel.r,max(pixel.g,pixel.b));
float lowest=min(pixel.r,min(pixel.g,pixel.b));
float colored=smoothstep(0.010,0.035,highest-lowest);
float darkInk=1.0-smoothstep(0.18,0.42,highest);
float shape=max(colored,darkInk);
return float4(pixel.rgb,pixel.a*lerp(1.0,shape,saturate(RemoveNeutralBackdrop)));
''')
connect(uv,cutout,'UV');connect(texture,cutout,'IconTexture')
connect(scalar(material,'RemoveNeutralBackdrop',1),cutout,'RemoveNeutralBackdrop')
color=node(material,unreal.MaterialExpressionComponentMask)
alpha=node(material,unreal.MaterialExpressionComponentMask)
for channel in ['r','g','b','a']:
 color.set_editor_property(channel,channel!='a');alpha.set_editor_property(channel,channel=='a')
connect(cutout,color,'Input');connect(cutout,alpha,'Input')
vertex=node(material,unreal.MaterialExpressionVertexColor)
final_color=node(material,unreal.MaterialExpressionMultiply);connect(color,final_color,'A');connect(vertex,final_color,'B')
final_alpha=node(material,unreal.MaterialExpressionMultiply);connect(alpha,final_alpha,'A');lib.connect_material_expressions(vertex,'A',final_alpha,'B')
lib.connect_material_property(final_color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.connect_material_property(final_alpha,'',unreal.MaterialProperty.MP_OPACITY)
lib.layout_material_expressions(material);lib.recompile_material(material);save_asset_with_texture_budget(material)
for key,filename,remove in specs:
 name='MI_Mechanic_'+key
 asset=dest+'/'+name
 instance=unreal.load_asset(asset) if unreal.EditorAssetLibrary.does_asset_exist(asset) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,dest,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
 lib.set_material_instance_parent(instance,material)
 lib.set_material_instance_texture_parameter_value(instance,'IconTexture',textures[key])
 lib.set_material_instance_scalar_parameter_value(instance,'RemoveNeutralBackdrop',remove)
 lib.update_material_instance(instance);save_asset_with_texture_budget(instance)
report_path=root/'Saved/Codex/CardEffects-20260907/mechanic-icon-materials.json'
report_path.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
(approved/'manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'ok':True,'material':path,'icons':[r['key'] for r in report],'source_pixels_unchanged':True}))
