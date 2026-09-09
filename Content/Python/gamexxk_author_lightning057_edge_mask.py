"""UI atlas material: aspect-preserving cover and local-frame feathering."""
import unreal
from gamexxk_author_card_name_flow import node, scalar

def apply_edge_mask(material):
    lib=unreal.MaterialEditingLibrary
    expressions=lib.get_material_expressions(material)
    sample=next(x for x in expressions if isinstance(x,unreal.MaterialExpressionTextureSampleParameter2D))
    uv=next(x for x in expressions if isinstance(x,unreal.MaterialExpressionTextureCoordinate))
    rect=next(x for x in expressions if isinstance(x,unreal.MaterialExpressionCustom) and x.get_editor_property('output_type')==unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    def wire(a,out,b,pin):
        if not lib.connect_material_expressions(a,out,b,pin):
            raise RuntimeError('Failed material wire: '+pin)
    existing={str(x.get_editor_property('parameter_name')):x for x in expressions if isinstance(x,unreal.MaterialExpressionScalarParameter)}
    aspect=existing.get('ViewportAspect') or scalar(material,'ViewportAspect',16/9)
    width=existing.get('EdgeFeather') or scalar(material,'EdgeFeather',.09)
    width.set_editor_property('default_value',.09)
    offset=existing.get('OffsetX') or scalar(material,'OffsetX',.12)
    compact=existing.get('CloudCompact') or scalar(material,'CloudCompact',1)
    lightning_only=existing.get('OnlyLightning') or scalar(material,'OnlyLightning',1)
    lightning_only.set_editor_property('default_value',1)
    framing=existing.get('FrameScale') or scalar(material,'FrameScale',.9)
    inputs=[]
    for name in ('UV','U0','V0','U1','V1','FlipX','ViewportAspect','OffsetX','FrameScale'):
        item=unreal.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
    rect.set_editor_property('inputs',inputs)
    rect.set_editor_property('code','float a=max(ViewportAspect,0.1); float2 scale=float2(min(a/(16.0/9.0),1.0),min((16.0/9.0)/a,1.0)); float2 local=(UV-0.5)*scale/max(FrameScale,0.1)+0.5+float2(OffsetX,0); local=saturate(local); local.x=lerp(local.x,1-local.x,saturate(FlipX)); return lerp(float2(U0,V0),float2(U1,V1),local);')
    wire(uv,'',rect,'UV')
    for key in ('U0','V0','U1','V1','FlipX'):
        wire(existing[key],'',rect,key)
    wire(aspect,'',rect,'ViewportAspect')
    wire(offset,'',rect,'OffsetX')
    wire(framing,'',rect,'FrameScale')
    mask=next((x for x in expressions if isinstance(x,unreal.MaterialExpressionCustom) and x.get_editor_property('desc')=='LocalFrameEdgeFeather'),None)
    if mask is None:
        mask=node(material,unreal.MaterialExpressionCustom)
    mask.set_editor_property('desc','LocalFrameEdgeFeather')
    mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs=[]
    for name in ('UV','Alpha','ViewportAspect','EdgeFeather','RGB','OffsetX','CloudCompact','OnlyLightning','FrameScale'):
        item=unreal.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
    mask.set_editor_property('inputs',inputs)
    mask.set_editor_property('code','float a=max(ViewportAspect,0.1); float2 halfSize=float2(a,1)*0.5; float radius=min(0.16,min(halfSize.x,halfSize.y)); float2 p=(UV-0.5)*float2(a,1); float2 q=abs(p)-(halfSize-radius); float sdf=length(max(q,0))+min(max(q.x,q.y),0)-radius; float edge=smoothstep(0,max(EdgeFeather,0.001),-sdf); float2 scale=float2(min(a/(16.0/9.0),1.0),min((16.0/9.0)/a,1.0)); float2 local=(UV-0.5)*scale+0.5+float2(OffsetX,0); float2 sd=min(local,1-local); float frameEdge=smoothstep(0,0.07,min(sd.x,sd.y)); float gold=smoothstep(0.04,0.18,RGB.r-RGB.b); float cloudRadius=length((local-float2(0.45,0.48))/float2(0.52,0.42)); float cloudMask=1-smoothstep(0.65,1.3,cloudRadius); float cloud=lerp(1,cloudMask,saturate(CloudCompact)*(1-gold)); return Alpha*min(edge,frameEdge)*cloud;')
    wire(uv,'',mask,'UV');wire(sample,'A',mask,'Alpha');wire(aspect,'',mask,'ViewportAspect');wire(width,'',mask,'EdgeFeather')
    wire(sample,'RGB',mask,'RGB');wire(offset,'',mask,'OffsetX');wire(compact,'',mask,'CloudCompact')
    code=mask.get_editor_property('code')
    code=code.replace('float2 local=(UV-0.5)*scale+0.5+float2(OffsetX,0);','float2 local=(UV-0.5)*scale/max(FrameScale,0.1)+0.5+float2(OffsetX,0);')
    code=code.replace('return Alpha*min(edge,frameEdge)*cloud;','float lightning=max(smoothstep(0.055,0.14,RGB.r-RGB.b)*smoothstep(0.035,0.10,RGB.r),smoothstep(0.65,0.90,min(RGB.r,min(RGB.g,RGB.b)))); return Alpha*min(edge,frameEdge)*lerp(cloud,lightning,saturate(OnlyLightning));')
    mask.set_editor_property('code',code)
    wire(lightning_only,'',mask,'OnlyLightning');wire(framing,'',mask,'FrameScale')
    # Slate opacity still controls only playback entry/exit; the image centre stays opaque.
    vertex=next(x for x in expressions if isinstance(x,unreal.MaterialExpressionVertexColor))
    alpha=next((x for x in expressions if isinstance(x,unreal.MaterialExpressionMultiply) and x.get_editor_property('desc')=='FeatheredOpacity'),None)
    if alpha is None:alpha=node(material,unreal.MaterialExpressionMultiply)
    alpha.set_editor_property('desc','FeatheredOpacity')
    wire(mask,'',alpha,'A');wire(vertex,'A',alpha,'B')
    if not lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY):raise RuntimeError('Opacity output failed')
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)

if __name__=='__main__':
    apply_edge_mask(unreal.load_asset('/Game/GameXXK/UI/Battle/VFX/UltimateLightning057/M_UltimateLightning057'))
    print('EdgeFeather=.09; aspect-preserving cover; centre opacity unchanged')
