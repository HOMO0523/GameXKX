"""Create UI font fill/outline materials for the two actual card upgrades."""
import json
from pathlib import Path
import unreal

DEST = '/Game/GameXXK/UI/Materials/CardNameFlow'
LIB = unreal.MaterialEditingLibrary

def node(material, cls):
    return LIB.create_material_expression(material, cls, -300, 0)

def connect(source, target, pin):
    for candidate in (pin, '', 'None'):
        if LIB.connect_material_expressions(source, '', target, candidate):
            return
    raise RuntimeError(f'Cannot connect {source.get_name()} to {target.get_name()}:{pin}')

def binary(material, cls, a, b):
    result = node(material, cls)
    connect(a, result, 'A')
    connect(b, result, 'B')
    return result

def scalar(material, name, value):
    result = node(material, unreal.MaterialExpressionScalarParameter)
    result.set_editor_property('parameter_name', name)
    result.set_editor_property('default_value', value)
    return result

def vector(material, name, color):
    result = node(material, unreal.MaterialExpressionVectorParameter)
    result.set_editor_property('parameter_name', name)
    result.set_editor_property('default_value', color)
    return result

def rgb(hexcode):
    values = [int(hexcode[i:i+2], 16) / 255.0 for i in (0,2,4)]
    return unreal.LinearColor(*[v / 12.92 if v <= .04045 else ((v+.055)/1.055)**2.4 for v in values], 1)

def main():
    unreal.EditorAssetLibrary.make_directory(DEST)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    path = DEST + '/M_CardNameFlow'
    material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else asset_tools.create_asset('M_CardNameFlow', DEST, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    for expression in LIB.get_material_expressions(material):
        LIB.delete_material_expression(material, expression)
    uv = node(material, unreal.MaterialExpressionTextureCoordinate)
    x = node(material, unreal.MaterialExpressionComponentMask)
    x.set_editor_property('r', True)
    for channel in ('g', 'b', 'a'):
        x.set_editor_property(channel, False)
    connect(uv, x, 'Input')
    y = node(material, unreal.MaterialExpressionComponentMask)
    for channel in ('r', 'b', 'a'):
        y.set_editor_property(channel, False)
    y.set_editor_property('g', True)
    connect(uv, y, 'Input')
    # Per-title width/height compensation keeps the sweep at 45 screen degrees.
    diagonal = binary(material, unreal.MaterialExpressionAdd, x,
        binary(material, unreal.MaterialExpressionDivide, y, scalar(material, 'TextAspect', 4)))
    time = node(material, unreal.MaterialExpressionTime)
    speed = scalar(material, 'Speed', .22)
    travel = binary(material, unreal.MaterialExpressionMultiply, time, speed)
    phase = binary(material, unreal.MaterialExpressionSubtract, diagonal, travel)
    wave = node(material, unreal.MaterialExpressionSine)
    connect(phase, wave, 'Input')
    half = scalar(material, 'Half', .5)
    normalized = binary(material, unreal.MaterialExpressionAdd, binary(material, unreal.MaterialExpressionMultiply, wave, half), half)
    band = node(material, unreal.MaterialExpressionPower)
    connect(normalized, band, 'Base')
    connect(scalar(material, 'Sharpness', 36), band, 'Exp')
    base = node(material, unreal.MaterialExpressionLinearInterpolate)
    connect(vector(material, 'ColorA', rgb('294e9a')), base, 'A')
    connect(vector(material, 'ColorB', rgb('63329b')), base, 'B')
    connect(normalized, base, 'Alpha')
    sheen = node(material, unreal.MaterialExpressionLinearInterpolate)
    connect(base, sheen, 'A')
    connect(vector(material, 'SheenColor', rgb('cbc7ff')), sheen, 'B')
    connect(binary(material, unreal.MaterialExpressionMultiply, band, scalar(material, 'SheenStrength', .58)), sheen, 'Alpha')
    vertex = node(material, unreal.MaterialExpressionVertexColor)
    assert LIB.connect_material_property(sheen, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = node(material, unreal.MaterialExpressionMultiply)
    assert LIB.connect_material_expressions(vertex, 'A', opacity, 'A')
    connect(scalar(material, 'FillOpacity', 1), opacity, 'B')
    frame = node(material, unreal.MaterialExpressionCustom)
    frame.set_editor_property('description', 'Antialiased rounded rarity frame')
    frame.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    names = ('UV', 'CardWidth', 'CardHeight', 'StrokeWidth')
    inputs = []
    for name in names:
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        inputs.append(entry)
    frame.set_editor_property('inputs', inputs)
    frame.set_editor_property('code', '''
float2 size = float2(CardWidth, CardHeight);
float radius = 8.0;
float inset = 4.0;
float2 p = abs((UV - 0.5) * size) - (size * 0.5 - inset - radius);
float d = length(max(p, 0.0)) + min(max(p.x, p.y), 0.0) - radius;
float aa = max(fwidth(d), 0.65);
return 1.0 - smoothstep(StrokeWidth * 0.5 - aa, StrokeWidth * 0.5 + aa, abs(d));
''')
    connect(uv, frame, 'UV')
    for name, value in [('CardWidth',206), ('CardHeight',285), ('StrokeWidth',2.6)]:
        connect(scalar(material, name, value), frame, name)
    frame_alpha = node(material, unreal.MaterialExpressionLinearInterpolate)
    connect(scalar(material, 'One', 1), frame_alpha, 'A')
    connect(frame, frame_alpha, 'B')
    connect(scalar(material, 'FrameMode', 0), frame_alpha, 'Alpha')
    masked_opacity = binary(material, unreal.MaterialExpressionMultiply, opacity, frame_alpha)
    assert LIB.connect_material_property(masked_opacity, '', unreal.MaterialProperty.MP_OPACITY)
    LIB.layout_material_expressions(material)
    LIB.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    variants = {
        'RareFill': ('88abf8', 'b99bef', 'f8f6ff', .22, 42, .92, .96),
        'RareOutline': ('243b91', '572786', 'a9a3f5', .22, 26, .55, 1),
        'EpicFill': ('ffc46a', 'ffdda0', 'fff8d8', .32, 34, .98, .96),
        'EpicOutline': ('7e3217', 'c08221', 'ffdd74', .32, 20, .82, 1),
    }
    report = []
    for suffix, values in variants.items():
        name = 'MI_CardName' + suffix
        target = DEST + '/' + name
        instance = unreal.load_asset(target) if unreal.EditorAssetLibrary.does_asset_exist(target) else asset_tools.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        LIB.set_material_instance_parent(instance, material)
        LIB.update_material_instance(instance)
        for key, color in zip(('ColorA', 'ColorB', 'SheenColor'), values[:3]):
            LIB.set_material_instance_vector_parameter_value(instance, key, rgb(color))
        for key, value in zip(('Speed', 'Sharpness', 'SheenStrength', 'FillOpacity'), values[3:]):
            LIB.set_material_instance_scalar_parameter_value(instance, key, value)
        LIB.update_material_instance(instance)
        unreal.EditorAssetLibrary.save_loaded_asset(instance)
        actual = {key: LIB.get_material_instance_scalar_parameter_value(instance, key)
                  for key in ('Speed', 'Sharpness', 'SheenStrength', 'FillOpacity')}
        for key, expected in zip(('Speed', 'Sharpness', 'SheenStrength', 'FillOpacity'), values[3:]):
            assert abs(actual[key] - expected) < .0001, (key, actual[key], expected)
        report.append({'asset': target, 'parameters': values, 'actual_scalar_parameters': actual})
    for tier, thickness in [('Rare',2.6), ('Epic',3.4)]:
        name = 'MI_CardFrame' + tier
        target = DEST + '/' + name
        instance = unreal.load_asset(target) if unreal.EditorAssetLibrary.does_asset_exist(target) else asset_tools.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        LIB.set_material_instance_parent(instance, unreal.load_asset(DEST + '/MI_CardName' + tier + 'Outline'))
        for key, value in [('FrameMode',1), ('StrokeWidth',thickness), ('TextAspect',206/285)]:
            LIB.set_material_instance_scalar_parameter_value(instance, key, value)
        colors = ('243b91','572786') if tier == 'Rare' else ('7e3217','c08221')
        for key, hexcode in zip(('ColorA','ColorB'), colors):
            c = rgb(hexcode)
            LIB.set_material_instance_vector_parameter_value(instance,key,unreal.LinearColor(c.r*1.6,c.g*1.6,c.b*1.6,1))
        LIB.update_material_instance(instance)
        unreal.EditorAssetLibrary.save_loaded_asset(instance)
        report.append({'asset': target, 'stroke_width': thickness, 'palette_parent': tier+'Outline', 'frame_linear_brightness':1.6})
    destination = Path(__file__).resolve().parents[2] / 'Saved/Codex/CardEffects-20260907/card-name-materials.json'
    destination.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'ok': True, 'parent': path, 'variants': report}))

if __name__ == '__main__':
    try:
        main()
    except Exception:
        import traceback
        raise RuntimeError(traceback.format_exc())
