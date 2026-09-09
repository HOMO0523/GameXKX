"""Import the cut/matted 057 atlases and a reusable UI playback material."""

def save_asset_with_texture_budget(*args, **kwargs):
    from gamexxk_texture_budget import save_loaded_asset
    return save_loaded_asset(*args, unreal_module=unreal, **kwargs)

from pathlib import Path
import json,unreal
from gamexxk_author_card_name_flow import node,connect,scalar
root=Path(unreal.Paths.project_dir());source=root/'SourceArt/UI/Battle/VFX/UltimateLightning057'
sequence=json.loads((source/'sequence.json').read_text(encoding='utf-8'))
dest='/Game/GameXXK/UI/Battle/VFX/UltimateLightning057';lib=unreal.MaterialEditingLibrary;assets=unreal.AssetToolsHelpers.get_asset_tools()
unreal.EditorAssetLibrary.make_directory(dest);textures=[];report=[]
for entry in sequence['atlases']:
 file=source/entry['file'];name=file.stem;task=unreal.AssetImportTask()
 task.filename=str(file);task.destination_path=dest;task.destination_name=name;task.automated=True;task.replace_existing=True;task.save=True
 assets.import_asset_tasks([task]);texture=unreal.load_asset(dest+'/'+name)
 if not isinstance(texture,unreal.Texture2D):raise RuntimeError('Failed texture import: '+name)
 for prop,value in [('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI),('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON),('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),('srgb',True),('never_stream',True),('address_x',unreal.TextureAddress.TA_CLAMP),('address_y',unreal.TextureAddress.TA_CLAMP),('filter',unreal.TextureFilter.TF_BILINEAR),('compression_no_alpha',False)]:texture.set_editor_property(prop,value)
 save_asset_with_texture_budget(texture);textures.append(texture)
 report.append({'texture':texture.get_path_name(),'source':str(file),'source_size':entry['size']})
name='M_UltimateLightning057';path=dest+'/'+name
material=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(name,dest,unreal.Material,unreal.MaterialFactoryNew())
material.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
for expression in lib.get_material_expressions(material):lib.delete_material_expression(material,expression)
uv=node(material,unreal.MaterialExpressionTextureCoordinate);rect=node(material,unreal.MaterialExpressionCustom)
rect.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT2)
inputs=[]
for input_name in ['UV','U0','V0','U1','V1','FlipX']:
 item=unreal.CustomInput();item.set_editor_property('input_name',input_name);inputs.append(item)
rect.set_editor_property('inputs',inputs);rect.set_editor_property('code','float2 local=UV; local.x=lerp(local.x,1.0-local.x,saturate(FlipX)); return lerp(float2(U0,V0),float2(U1,V1),local);')
connect(uv,rect,'UV');first=sequence['frames'][0]
for p,v in [('U0',first['uv_min'][0]),('V0',first['uv_min'][1]),('U1',first['uv_max'][0]),('V1',first['uv_max'][1]),('FlipX',0)]:connect(scalar(material,p,v),rect,p)
sample=node(material,unreal.MaterialExpressionTextureSampleParameter2D);sample.set_editor_property('parameter_name','AtlasTexture');sample.set_editor_property('texture',textures[0]);connect(rect,sample,'Coordinates')
vertex=node(material,unreal.MaterialExpressionVertexColor);color=node(material,unreal.MaterialExpressionMultiply);alpha=node(material,unreal.MaterialExpressionMultiply)
lib.connect_material_expressions(sample,'RGB',color,'A');connect(vertex,color,'B');lib.connect_material_expressions(sample,'A',alpha,'A');lib.connect_material_expressions(vertex,'A',alpha,'B')
lib.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
lib.layout_material_expressions(material);lib.recompile_material(material);save_asset_with_texture_budget(material)
from gamexxk_author_lightning057_edge_mask import apply_edge_mask
apply_edge_mask(material)
instances=[]
for suffix,flip in [('Friendly',1),('Canonical',0)]:
 iname='MI_UltimateLightning057_'+suffix;ipath=dest+'/'+iname
 instance=unreal.load_asset(ipath) if unreal.EditorAssetLibrary.does_asset_exist(ipath) else assets.create_asset(iname,dest,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
 lib.set_material_instance_parent(instance,material);lib.set_material_instance_scalar_parameter_value(instance,'FlipX',flip);lib.set_material_instance_texture_parameter_value(instance,'AtlasTexture',textures[0]);lib.update_material_instance(instance);save_asset_with_texture_budget(instance);instances.append(instance.get_path_name())
result={'ok':True,'textures':report,'material':material.get_path_name(),'instances':instances,'frame_count':56,'friendly_flip_x':1,'hooked_to_battle':False}
(source/'ue-import.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
(root/'Deliverables/Reviews/2026-09-08-vfx/ultimates/redraw-057/ue-import.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False))
