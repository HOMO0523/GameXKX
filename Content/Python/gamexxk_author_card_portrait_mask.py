"""Author a shared, antialiased card-local art mask; no render targets or source edits."""
import json
import unreal
from gamexxk_author_card_name_flow import node, connect, binary, vector

lib = unreal.MaterialEditingLibrary
root = '/Game/GameXXK/UI/Materials/CardPortrait'
name = 'M_CardPortraitRoundedMask'
unreal.EditorAssetLibrary.make_directory(root)
path = root + '/' + name
material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, root, unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
for expression in lib.get_material_expressions(material):
    lib.delete_material_expression(material, expression)
uv = node(material, unreal.MaterialExpressionTextureCoordinate)
texture = node(material, unreal.MaterialExpressionTextureSampleParameter2D)
texture.set_editor_property('parameter_name','ArtTexture')
texture.set_editor_property('texture', unreal.load_asset('/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Hero'))
connect(uv,texture,'UVs')
mask = node(material,unreal.MaterialExpressionCustom)
mask.set_editor_property('description','Card-local rounded art containment')
mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
names = ('UV','CardSize','ArtOrigin','ArtAxisX','ArtAxisY')
inputs=[]
for key in names:
    entry=unreal.CustomInput();entry.set_editor_property('input_name',key);inputs.append(entry)
mask.set_editor_property('inputs',inputs)
mask.set_editor_property('code','''
float2 size = max(CardSize.xy, 1.0);
float2 position = ArtOrigin.xy + UV.x * ArtAxisX.xy + UV.y * ArtAxisY.xy;
float unit = min(size.x, size.y) / 206.0;
float radius = 8.0 * unit;
float inset = 6.0 * unit;
float2 p = abs(position - size * 0.5) - (size * 0.5 - inset - radius);
float d = length(max(p, 0.0)) + min(max(p.x,p.y), 0.0) - radius;
float aa = max(fwidth(d), 0.5);
return 1.0 - smoothstep(-aa, aa, d);
''')
connect(uv,mask,'UV')
for key, value in [('CardSize',(206,285)),('ArtOrigin',(8,48)),('ArtAxisX',(190,0)),('ArtAxisY',(0,228))]:
    connect(vector(material,key,unreal.LinearColor(value[0],value[1],0,0)),mask,key)
vertex=node(material,unreal.MaterialExpressionVertexColor)
tint=node(material,unreal.MaterialExpressionMultiply)
assert lib.connect_material_expressions(texture,'RGB',tint,'A')
connect(vertex,tint,'B')
alpha=node(material,unreal.MaterialExpressionMultiply)
assert lib.connect_material_expressions(texture,'A',alpha,'A')
assert lib.connect_material_expressions(vertex,'A',alpha,'B')
opacity=binary(material,unreal.MaterialExpressionMultiply,alpha,mask)
assert lib.connect_material_property(tint,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
assert lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
lib.layout_material_expressions(material)
lib.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)
print(json.dumps({'material':path,'inset_at_206':6,'radius_at_206':8,'uses_render_target':False}))
