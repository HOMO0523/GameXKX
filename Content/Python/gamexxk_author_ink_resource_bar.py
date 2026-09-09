"""Import the approved ink atlas unchanged and render one shared HP/MP silhouette."""

def save_asset_with_texture_budget(*args, **kwargs):
    from gamexxk_texture_budget import save_loaded_asset
    return save_loaded_asset(*args, unreal_module=unreal, **kwargs)

import json
from pathlib import Path
import unreal
from gamexxk_author_card_name_flow import node,connect,scalar,vector,rgb

root=Path(__file__).resolve().parents[2]
dest='/Game/GameXXK/UI/Battle/ResourceBars/InkV3'
source=root/'SourceArt/UI/Battle/ResourceBars/Draft_20260907/ink_resource_bars_v3.png'
unreal.EditorAssetLibrary.make_directory(dest)
texture_path=dest+'/T_InkResourceBarMaster'
texture=unreal.load_asset(texture_path) if unreal.EditorAssetLibrary.does_asset_exist(texture_path) else None
if not texture:
    task=unreal.AssetImportTask();task.filename=str(source);task.destination_path=dest;task.destination_name='T_InkResourceBarMaster';task.automated=True;task.save=True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);texture=unreal.load_asset(texture_path)
texture.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('srgb',True);texture.set_editor_property('never_stream',True)
save_asset_with_texture_budget(texture)
path=dest+'/M_InkResourceBar'
lib=unreal.MaterialEditingLibrary
m=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_InkResourceBar',dest,unreal.Material,unreal.MaterialFactoryNew())
m.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
for expression in lib.get_material_expressions(m):lib.delete_material_expression(m,expression)
uv=node(m,unreal.MaterialExpressionTextureCoordinate)
tex=node(m,unreal.MaterialExpressionTextureObjectParameter);tex.set_editor_property('parameter_name','Master');tex.set_editor_property('texture',texture)
custom=node(m,unreal.MaterialExpressionCustom);custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
names=('UV','Master','FillPercent','BarAspect','FillColor','EmptyColor')
inputs=[]
for name in names:
    item=unreal.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
custom.set_editor_property('inputs',inputs)
custom.set_editor_property('description','One ink master, fixed endcaps, recolored fill, transparent paper removal')
custom.set_editor_property('code','''
// The approved red row is the only shape source for every resource.
float sourceCap=72.0/1383.0;
float cap=min(0.25,(72.0/143.0)/max(BarAspect,2.0));
float u=UV.x;
float su=u<cap ? u/cap*sourceCap : (u>1.0-cap ? 1.0-(1.0-u)/cap*sourceCap : lerp(sourceCap,1.0-sourceCap,(u-cap)/(1.0-2.0*cap)));
float2 atlasUV=(float2(76.0,445.0)+float2(su,UV.y)*float2(1383.0,143.0))/float2(1536.0,1024.0);
float3 sample=Texture2DSample(Master,MasterSampler,atlasUV).rgb;
float light=dot(sample,float3(0.299,0.587,0.114));
float opacity=1.0-smoothstep(0.72,0.88,light);
float interior=smoothstep(0.04,0.10,sample.r-sample.g);
float innerLeft=0.027/sourceCap*cap;
float innerRight=1.0-0.027/sourceCap*cap;
float boundary=lerp(innerLeft,innerRight,saturate(FillPercent));
float aa=max(fwidth(u),0.0001);
float filled=FillPercent<=0.0 ? 0.0 : (FillPercent>=1.0 ? 1.0 : 1.0-smoothstep(boundary-aa,boundary+aa,u));
float shade=clamp(light/0.24,0.82,1.06);
float3 channel=lerp(EmptyColor.rgb,FillColor.rgb,filled)*shade;
return float4(lerp(sample,channel,interior),opacity);
''')
connect(uv,custom,'UV');connect(tex,custom,'Master')
connect(scalar(m,'FillPercent',1),custom,'FillPercent');connect(scalar(m,'BarAspect',124/18),custom,'BarAspect')
connect(vector(m,'FillColor',rgb('c45135')),custom,'FillColor');connect(vector(m,'EmptyColor',rgb('9d8b70')),custom,'EmptyColor')
color=node(m,unreal.MaterialExpressionComponentMask)
for c in ('r','g','b','a'):color.set_editor_property(c,c!='a')
connect(custom,color,'Input')
alpha=node(m,unreal.MaterialExpressionComponentMask)
for c in ('r','g','b','a'):alpha.set_editor_property(c,c=='a')
connect(custom,alpha,'Input')
vertex=node(m,unreal.MaterialExpressionVertexColor)
tint=node(m,unreal.MaterialExpressionMultiply);connect(color,tint,'A');connect(vertex,tint,'B')
opacity=node(m,unreal.MaterialExpressionMultiply);connect(alpha,opacity,'A');assert lib.connect_material_expressions(vertex,'A',opacity,'B')
assert lib.connect_material_property(tint,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
lib.layout_material_expressions(m);lib.recompile_material(m);save_asset_with_texture_budget(m)
print(json.dumps({'material':path,'texture':texture_path,'source_unchanged':True,'shared_shape_row':[76,445,1459,588]}))
