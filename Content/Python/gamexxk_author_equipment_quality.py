"""Approved equipment palette, sharing the existing card font/frame graph and its 45-degree math."""
import json
from pathlib import Path
import unreal
from gamexxk_author_card_name_flow import node, scalar, vector, connect, binary, rgb

LIB=unreal.MaterialEditingLibrary
DEST='/Game/GameXXK/UI/Materials/EquipmentQuality'
ROOT=Path(__file__).resolve().parents[2]
PALETTE=[
 ('7a7770','56534d','eeeae3','bcb8b0','000000',0),
 ('51835b','c5eed2','edf2df','97b098','000000',0),
 ('4d84b8','cce6ff','eaf3f7','90adc5','000000',0),
 ('8067b2','e3d7ff','f0eafa','aa94c6','000000',0),
 ('cba43b','fff0b8','fff5d9','c5a653','000000',0),
 ('d28a38','ffe2b8','fff0d8','c58d47','ffc771',.24),
 ('c25836','ffd8bd','fcebdc','bc785e','ff9d68',.32),
 ('c47283','ffe1eb','fbeaf0','bc839c','ffb7cd',.40),
 ('8398aa','f3f8fc','f5f9fc','a8becf','d9efff',.50),
 ('7f94a8','f8fcff','fbfdff','b4c9d9','eff9ff',.62)]

def custom(mat,description,code,inputs,kind):
    out=node(mat,unreal.MaterialExpressionCustom);out.set_editor_property('description',description)
    out.set_editor_property('code',code);out.set_editor_property('output_type',kind)
    entries=[]
    for name in inputs:
        item=unreal.CustomInput();item.set_editor_property('input_name',name);entries.append(item)
    out.set_editor_property('inputs',entries)
    for name,source in inputs.items():connect(source,out,name)
    return out

def spectral_code():
    hexes=['63798c','bfcfdd','64e4ed','8e9eff','e0a4ff','ffffff','ffb2d1','ffe5a0','91e5c6','bdcddc','63798c']
    points=[0,.24,.32,.40,.46,.50,.55,.62,.68,.77,1]
    colors=[]
    for h in hexes:
        c=rgb(h);colors.append(f'float3({c.r:.7f},{c.g:.7f},{c.b:.7f})')
    code=f'float3 spectral={colors[0]};\n'
    for i in range(1,len(points)):
        code+=f'spectral=lerp(spectral,{colors[i]},saturate((wave-{points[i-1]})/{points[i]-points[i-1]:.5f}));\n'
    return code

def author_name_parent():
    path=DEST+'/M_EquipmentNameFlow'
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        assert unreal.EditorAssetLibrary.duplicate_asset('/Game/GameXXK/UI/Materials/CardNameFlow/M_CardNameFlow',path)
    mat=unreal.load_asset(path)
    existing=next((e for e in LIB.get_material_expressions(mat) if isinstance(e,unreal.MaterialExpressionCustom) and e.get_editor_property('description')=='Equipment platinum extension'),None)
    if not existing:
        original=LIB.get_material_property_input_node(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if not original:raise RuntimeError('Missing original card font color graph')
        uv=node(mat,unreal.MaterialExpressionTextureCoordinate);time=node(mat,unreal.MaterialExpressionTime)
        code='float wave=.5+.5*sin((UV.x+UV.y/max(TextAspect,1.0)-Time*Speed)*6.2831853);\n'+spectral_code()+'return spectral;'
        prism=custom(mat,'Equipment platinum extension',code,{'UV':uv,'Time':time,'TextAspect':scalar(mat,'TextAspect',4),'Speed':scalar(mat,'Speed',.22)},unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        out=node(mat,unreal.MaterialExpressionLinearInterpolate);connect(original,out,'A');connect(prism,out,'B');connect(scalar(mat,'PrismStrength',0),out,'Alpha')
        assert LIB.connect_material_property(out,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat);return mat

def author_surface():
    path=DEST+'/M_EquipmentSurface'
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    mat=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset('M_EquipmentSurface',DEST,unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain',unreal.MaterialDomain.MD_UI);mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    for e in LIB.get_material_expressions(mat):LIB.delete_material_expression(mat,e)
    uv=node(mat,unreal.MaterialExpressionTextureCoordinate);time=node(mat,unreal.MaterialExpressionTime)
    texture=node(mat,unreal.MaterialExpressionTextureSampleParameter2D);texture.set_editor_property('parameter_name','Paper');texture.set_editor_property('texture',unreal.load_asset('/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelLarge'))
    inputs={'UV':uv,'Time':time,'Paper':texture}
    for name,value in [('CardWidth',164),('CardHeight',200),('AuraMode',0),('AuraStrength',0),('FacetStrength',0),('PrismStrength',0)]:inputs[name]=scalar(mat,name,value)
    for name,hexcode in [('GroundTop','eeeae3'),('GroundBottom','bcb8b0'),('AuraColor','ffffff')]:inputs[name]=vector(mat,name,rgb(hexcode))
    code='''
float2 size=max(float2(CardWidth,CardHeight),1.0);
float2 p=abs((UV-.5)*size)-(size*.5-8.0);
float d=length(max(p,0.0))+min(max(p.x,p.y),0.0)-4.0;
float inside=1.0-smoothstep(-1.0,1.0,d);
float wave=.5+.5*sin((UV.x+UV.y/max(CardWidth/CardHeight,.1)-Time*.18)*6.2831853);
'''+spectral_code()+'''
if(AuraMode>.5){
 float r=length(float2((UV.x-.5)*1.05,(UV.y-.52)*.95));
 float a=pow(saturate(1.0-r/.54),1.55)*AuraStrength*(.91+.09*cos(Time*1.256637));
 float3 glow=lerp(AuraColor.rgb,spectral,PrismStrength*.30);
 return float4(glow,a*inside);
}
float3 tint=lerp(GroundTop.rgb,GroundBottom.rgb,smoothstep(.0,1.0,UV.y));
float3 color=Paper.rgb*lerp(float3(1,1,1),tint,.72);
float side=max(6.0,CardWidth*24.0/164.0),h=side*.8660254;
float2 q=float2(UV.x*CardWidth/side-.5*UV.y*CardHeight/h,UV.y*CardHeight/h);
float2 f=frac(q),cell=floor(q);float upper=step(1.0,f.x+f.y);
float plane=(upper>.5?.025:-.018);
float shine=pow(saturate(.5+.5*sin(Time*.8+dot(cell,float2(1.91,2.37))+upper)),18.0)*.075;
color+=FacetStrength*(plane+shine);
float film=pow(wave,20.0)*PrismStrength*.23;
color=lerp(color,spectral,film);
return float4(saturate(color),inside*.78);
'''
    result=custom(mat,'Equipment paper tint, local aura, equilateral facets',code,inputs,unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    color=node(mat,unreal.MaterialExpressionComponentMask);alpha=node(mat,unreal.MaterialExpressionComponentMask)
    for n in ('r','g','b'):color.set_editor_property(n,True);alpha.set_editor_property(n,False)
    color.set_editor_property('a',False);alpha.set_editor_property('a',True)
    connect(result,color,'Input');connect(result,alpha,'Input')
    vertex=node(mat,unreal.MaterialExpressionVertexColor);opacity=node(mat,unreal.MaterialExpressionMultiply)
    connect(alpha,opacity,'A');assert LIB.connect_material_expressions(vertex,'A',opacity,'B')
    assert LIB.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert LIB.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    LIB.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat);return mat

def instance(name,parent,scalars,colors):
    path=DEST+'/'+name;tools=unreal.AssetToolsHelpers.get_asset_tools()
    item=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name,DEST,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    LIB.set_material_instance_parent(item,parent)
    for key,value in scalars.items():LIB.set_material_instance_scalar_parameter_value(item,key,value)
    for key,value in colors.items():LIB.set_material_instance_vector_parameter_value(item,key,rgb(value) if isinstance(value,str) else value)
    LIB.update_material_instance(item);unreal.EditorAssetLibrary.save_loaded_asset(item)
    return {'path':path,'scalars':scalars,'colors':{k:v if isinstance(v,str) else [v.r,v.g,v.b,v.a] for k,v in colors.items()}}

def main():
    unreal.EditorAssetLibrary.make_directory(DEST)
    name_parent=author_name_parent();surface_parent=author_surface();report=[]
    for rank,(edge,fill,top,bottom,aura,power) in enumerate(PALETTE,1):
        prefix=f'MI_Equipment_R{rank:02d}_'
        speed=0 if rank==1 else .22 if rank<6 else .32
        edge_colors={'ColorA':edge,'ColorB':edge,'SheenColor':'f5f8ff'}
        if rank in (4,6):
            card=unreal.load_asset('/Game/GameXXK/UI/Materials/CardNameFlow/MI_CardName'+('Rare' if rank==4 else 'Epic')+'Outline')
            edge_colors={k:LIB.get_material_instance_vector_parameter_value(card,k) for k in ('ColorA','ColorB','SheenColor')}
        report.append(instance(prefix+'Fill',name_parent,{'Speed':0,'SheenStrength':0,'FillOpacity':.96,'FrameMode':0,'PrismStrength':0},{'ColorA':fill,'ColorB':fill,'SheenColor':fill}))
        base={'Speed':speed,'Sharpness':26 if rank<6 else 20,'SheenStrength':0 if rank==1 else .55 if rank<6 else .82,'FillOpacity':1,'FrameMode':0,'PrismStrength':1 if rank>=9 else 0}
        report.append(instance(prefix+'Outline',name_parent,base,edge_colors))
        report.append(instance(prefix+'Frame',name_parent,dict(base,FrameMode=1,StrokeWidth=1.5 if rank==1 else 2.2 if rank<6 else 2.8),edge_colors))
        report.append(instance(prefix+'Surface',surface_parent,{'AuraMode':0,'AuraStrength':0,'FacetStrength':1 if rank==10 else 0,'PrismStrength':1 if rank>=9 else 0},{'GroundTop':top,'GroundBottom':bottom}))
        if rank>=6:report.append(instance(prefix+'Aura',surface_parent,{'AuraMode':1,'AuraStrength':power,'FacetStrength':0,'PrismStrength':.8 if rank>=9 else 0},{'AuraColor':aura}))
    out=ROOT/'Saved/Codex/equipment-quality-materials.json';out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'ok':True,'instances':len(report),'report':str(out)}))

if __name__=='__main__':
    try:main()
    except Exception:
        import traceback
        print(json.dumps({'ok':False,'error':traceback.format_exc()}))
