"""Preserve original ink alpha and soften the camp banner's paper edges in UMG."""
import json
import unreal
from gamexxk_author_card_name_flow import node, connect, binary, scalar, rgb

lib = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()
root = '/Game/GameXXK/UI/Materials/Followup'
unreal.EditorAssetLibrary.make_directory(root)

def material(name):
    path = root + '/' + name
    m = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(name, root, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    for expression in lib.get_material_expressions(m):
        lib.delete_material_expression(m, expression)
    return m

def sample(m, path):
    s = node(m, unreal.MaterialExpressionTextureSample)
    s.set_editor_property('texture', unreal.load_asset(path))
    return s

def color(m, value):
    c = node(m, unreal.MaterialExpressionConstant3Vector)
    c.set_editor_property('constant', rgb(value))
    return c

def finish(m, final, opacity):
    assert lib.connect_material_property(final, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR), "Material link at line 31 failed: assert lib.connect_material_property(final, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)"
    assert lib.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY), "Material link at line 32 failed: assert lib.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)"
    lib.layout_material_expressions(m)
    lib.recompile_material(m)
    unreal.EditorAssetLibrary.save_loaded_asset(m)

ink = material('M_EndTurnVermilionInk')
tex = sample(ink, '/Game/GameXXK/UI/MainMenu/Textures/T_InkButtonBase')
tone = node(ink, unreal.MaterialExpressionLinearInterpolate)
connect(color(ink, '762820'), tone, 'A')
connect(color(ink, 'b8553b'), tone, 'B')
assert lib.connect_material_expressions(tex, 'G', tone, 'Alpha'), "Material link at line 42 failed: assert lib.connect_material_expressions(tex, 'G', tone, 'Alpha')"
vertex = node(ink, unreal.MaterialExpressionVertexColor)
tinted = node(ink, unreal.MaterialExpressionMultiply)
connect(tone, tinted, 'A')
connect(vertex, tinted, 'B')
alpha = node(ink, unreal.MaterialExpressionMultiply)
assert lib.connect_material_expressions(tex, 'A', alpha, 'A'), "Material link at line 48 failed: assert lib.connect_material_expressions(tex, 'A', alpha, 'A')"
assert lib.connect_material_expressions(vertex, 'A', alpha, 'B'), "Material link at line 49 failed: assert lib.connect_material_expressions(vertex, 'A', alpha, 'B')"
finish(ink, tinted, alpha)

banner = material('M_CampfireBannerSoftEdge')
tex = sample(banner, '/Game/GameXXK/UI/RouteCamp/T_CampfireRestBanner_V3')
uv = node(banner, unreal.MaterialExpressionTextureCoordinate)
masks = []
for channel, softness in [('r', 12), ('g', 10)]:
    component = node(banner, unreal.MaterialExpressionComponentMask)
    for c in ('r','g','b','a'):
        component.set_editor_property(c, c == channel)
    connect(uv, component, 'Input')
    inverse = node(banner, unreal.MaterialExpressionOneMinus)
    connect(component, inverse, 'Input')
    edge = binary(banner, unreal.MaterialExpressionMin, component, inverse)
    fade = node(banner, unreal.MaterialExpressionSaturate)
    connect(binary(banner, unreal.MaterialExpressionMultiply, edge, scalar(banner, 'Feather'+channel, softness)), fade, 'Input')
    masks.append(fade)
opacity = binary(banner, unreal.MaterialExpressionMultiply, masks[0], masks[1])
vertex = node(banner, unreal.MaterialExpressionVertexColor)
alpha = node(banner, unreal.MaterialExpressionMultiply)
connect(opacity, alpha, 'A')
assert lib.connect_material_expressions(vertex, 'A', alpha, 'B'), "Material link at line 71 failed: assert lib.connect_material_expressions(vertex, 'A', alpha, 'B')"
assert lib.connect_material_property(tex, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR), "Material link at line 72 failed: assert lib.connect_material_property(tex, 'RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR)"
assert lib.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY), "Material link at line 73 failed: assert lib.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)"
lib.layout_material_expressions(banner)
lib.recompile_material(banner)
unreal.EditorAssetLibrary.save_loaded_asset(banner)
print(json.dumps({'materials': [ink.get_path_name(), banner.get_path_name()]}))
