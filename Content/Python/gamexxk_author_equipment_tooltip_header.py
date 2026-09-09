"""Quality paper wash for equipment tooltip headers; the existing slot assets stay intact."""
import json
from pathlib import Path
import unreal
from gamexxk_author_equipment_quality import DEST, PALETTE, custom, instance, spectral_code
from gamexxk_author_card_name_flow import node, scalar, vector, rgb, connect

LIB = unreal.MaterialEditingLibrary

def main():
    path = DEST + '/M_EquipmentTooltipHeader'
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(
        'M_EquipmentTooltipHeader', DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    for expression in LIB.get_material_expressions(mat):
        LIB.delete_material_expression(mat, expression)
    inputs = {'UV': node(mat, unreal.MaterialExpressionTextureCoordinate),
              'Time': node(mat, unreal.MaterialExpressionTime)}
    paper = node(mat, unreal.MaterialExpressionTextureSampleParameter2D)
    paper.set_editor_property('parameter_name', 'Paper')
    paper.set_editor_property('texture', unreal.load_asset('/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot'))
    # Sample only the interior paper; the old baked border/black corners are
    # replaced by the same analytic round contour as the rarity frame.
    paper_uv = custom(mat, 'Remove the old baked paper border', 'return lerp(float2(.07,.07),float2(.93,.93),UV);',
                      {'UV': inputs['UV']}, unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    connect(paper_uv, paper, 'Coordinates')
    inputs['Paper'] = paper
    for key, value in [('CardWidth', 440), ('CardHeight', 720), ('HeaderHeight', 86), ('FacetStrength', 0), ('PrismStrength', 0)]:
        inputs[key] = scalar(mat, key, value)
    for key, value in [('GroundTop', 'eeeae3'), ('GroundBottom', 'bcb8b0')]:
        inputs[key] = vector(mat, key, rgb(value))
    code = '''
float2 size=max(float2(CardWidth,CardHeight),1.0);
float2 p=abs((UV-.5)*size)-(size*.5-4.0-8.0);
float d=length(max(p,0.0))+min(max(p.x,p.y),0.0)-8.0;
float inside=1.0-smoothstep(-.8,.8,d);
float wave=.5+.5*sin((UV.x+UV.y*CardHeight/CardWidth-Time*.12)*6.2831853);
''' + spectral_code() + '''
float headerY=saturate(UV.y*CardHeight/max(HeaderHeight,1.0));
float3 tint=lerp(GroundBottom.rgb,GroundTop.rgb,smoothstep(0.0,.9,headerY));
float grain=lerp(.88,1.0,dot(Paper.rgb,float3(.2126,.7152,.0722)));
float3 color=tint*grain;
float side=12.0,h=side*.8660254;
float2 q=float2(UV.x*CardWidth/side-.5*UV.y*CardHeight/h,UV.y*CardHeight/h);
float plane=step(1.0,frac(q).x+frac(q).y)>.5?.018:-.012;
color+=FacetStrength*plane;
color=lerp(color,spectral,PrismStrength*.13*pow(wave,20.0));
float fade=1.0-smoothstep(.12,1.0,headerY);
return float4(lerp(Paper.rgb,saturate(color),fade*.68),inside);
'''
    result = custom(mat, 'Quality paper fades to zero before base attributes', code, inputs,
                    unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    color = node(mat, unreal.MaterialExpressionComponentMask)
    alpha = node(mat, unreal.MaterialExpressionComponentMask)
    for channel in ('r', 'g', 'b'):
        color.set_editor_property(channel, True)
        alpha.set_editor_property(channel, False)
    color.set_editor_property('a', False)
    alpha.set_editor_property('a', True)
    connect(result, color, 'Input')
    connect(result, alpha, 'Input')
    vertex = node(mat, unreal.MaterialExpressionVertexColor)
    opacity = node(mat, unreal.MaterialExpressionMultiply)
    connect(alpha, opacity, 'A')
    assert LIB.connect_material_expressions(vertex, 'A', opacity, 'B')
    assert LIB.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert LIB.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
    LIB.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    report = []
    for rank, (_, _, top, bottom, _, _) in enumerate(PALETTE, 1):
        report.append(instance(f'MI_Equipment_R{rank:02d}_Header', mat,
            {'FacetStrength': 1 if rank == 10 else 0, 'PrismStrength': 1 if rank >= 9 else 0},
            {'GroundTop': top, 'GroundBottom': bottom}))
    out = Path(__file__).resolve().parents[2] / 'Saved/Codex/equipment-tooltip-header-materials.json'
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'ok': True, 'instances': len(report), 'report': str(out)}))

if __name__ == '__main__':
    try:
        main()
    except Exception:
        import traceback
        print(json.dumps({'ok': False, 'error': traceback.format_exc()}))
