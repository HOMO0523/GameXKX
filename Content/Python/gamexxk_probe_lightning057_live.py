import unreal, builtins
s=getattr(builtins,'_lightning057_preview',None)
print('state',s and {k:str(v) for k,v in s.items() if k not in ('textures','stop')})
if s:
    print('brush',s['image'].get_editor_property('brush'))
m=unreal.load_asset('/Game/GameXXK/UI/Battle/VFX/UltimateLightning057/M_UltimateLightning057')
print('domain',m.get_editor_property('material_domain'),'blend',m.get_editor_property('blend_mode'))
print('expressions',[(x.get_name(),str(x.get_class().get_name())) for x in unreal.MaterialEditingLibrary.get_material_expressions(m)])
for prop in (unreal.MaterialProperty.MP_EMISSIVE_COLOR,unreal.MaterialProperty.MP_OPACITY):
    print('output',str(prop),unreal.MaterialEditingLibrary.get_material_property_input_node(m,prop))
for x in unreal.MaterialEditingLibrary.get_material_expressions(m):
    if isinstance(x,unreal.MaterialExpressionMultiply):
        print(x.get_name(),x.get_editor_property('a'),x.get_editor_property('b'))
