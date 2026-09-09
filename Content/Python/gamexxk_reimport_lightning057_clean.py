
def save_asset_with_texture_budget(*args, **kwargs):
    from gamexxk_texture_budget import save_loaded_asset
    return save_loaded_asset(*args, unreal_module=unreal, **kwargs)

import unreal
from pathlib import Path
dest='/Game/GameXXK/UI/Battle/VFX/UltimateLightning057'
task=unreal.AssetImportTask()
task.filename=str(Path(unreal.Paths.project_dir())/'SourceArt/UI/Battle/VFX/UltimateLightning057/Atlases/T_UltimateLightning057_Atlas_04.png')
task.destination_path=dest
task.destination_name='T_UltimateLightning057_Atlas_04_Rebuilt'
task.automated=True
task.save=True
task.factory=unreal.TextureFactory()
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
t=unreal.load_asset(dest+'/'+task.destination_name)
for prop,val in [('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON),('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS),('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI),('never_stream',True),('compression_no_alpha',False),('srgb',True),('address_x',unreal.TextureAddress.TA_CLAMP),('address_y',unreal.TextureAddress.TA_CLAMP)]:
    t.set_editor_property(prop,val)
save_asset_with_texture_budget(t)
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([t])
print(t.get_path_name())
