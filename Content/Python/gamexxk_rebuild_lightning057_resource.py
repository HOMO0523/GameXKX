
def save_asset_with_texture_budget(*args, **kwargs):
    from gamexxk_texture_budget import save_loaded_asset
    return save_loaded_asset(*args, unreal_module=unreal, **kwargs)

import unreal
t=unreal.load_asset('/Game/GameXXK/UI/Battle/VFX/UltimateLightning057/T_UltimateLightning057_Atlas_04')
print([n for n in dir(t) if any(s in n for s in ('resource','compil','update','cache','source'))])
t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_DEFAULT)
t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
t.set_editor_property('defer_compression',False)
save_asset_with_texture_budget(t)
