
def save_asset_with_texture_budget(*args, **kwargs):
    from gamexxk_texture_budget import save_loaded_asset
    return save_loaded_asset(*args, unreal_module=unreal, **kwargs)

import unreal
from pathlib import Path
name='T_UltimateLightning057_TransitionV3';dest='/Game/GameXXK/UI/Battle/VFX/UltimateLightning057'
task=unreal.AssetImportTask();task.filename=str(Path(unreal.Paths.project_dir())/'SourceArt/UI/Battle/VFX/UltimateLightning057TransitionV3/Atlases'/f'{name}.png');task.destination_name=name;task.destination_path=dest;task.automated=True;task.replace_existing=True;task.save=True;task.factory=unreal.TextureFactory()
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);t=unreal.load_asset(dest+'/'+name)
for p,v in [('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON),('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI),('never_stream',True),('compression_no_alpha',False),('srgb',True),('address_x',unreal.TextureAddress.TA_CLAMP),('address_y',unreal.TextureAddress.TA_CLAMP)]:t.set_editor_property(p,v)
save_asset_with_texture_budget(t);print(t.get_path_name())
