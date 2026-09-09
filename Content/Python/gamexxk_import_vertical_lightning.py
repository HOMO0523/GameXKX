
def save_asset_with_texture_budget(*args, **kwargs):
    from gamexxk_texture_budget import save_loaded_asset
    return save_loaded_asset(*args, unreal_module=unreal, **kwargs)

import unreal
from pathlib import Path
dest='/Game/GameXXK/UI/Battle/VFX/VerticalLightning'
t=unreal.AssetImportTask();t.filename=str(Path(unreal.Paths.project_dir())/'SourceArt/UI/Battle/VFX/VerticalLightning/T_VerticalLightning.png');t.destination_path=dest;t.destination_name='T_VerticalLightning';t.automated=True;t.replace_existing=True;t.save=True;t.factory=unreal.TextureFactory()
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
asset=unreal.load_asset(dest+'/T_VerticalLightning')
for key,value in [('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON),('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI),('never_stream',True),('compression_no_alpha',False),('srgb',True),('address_x',unreal.TextureAddress.TA_CLAMP),('address_y',unreal.TextureAddress.TA_CLAMP)]:asset.set_editor_property(key,value)
save_asset_with_texture_budget(asset)
print(asset.get_path_name())
