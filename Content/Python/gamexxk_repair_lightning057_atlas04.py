"""Reimport the atlas whose editor platform resource has zero mips."""

def save_asset_with_texture_budget(*args, **kwargs):
    from gamexxk_texture_budget import save_loaded_asset
    return save_loaded_asset(*args, unreal_module=unreal, **kwargs)

import builtins
import json
from pathlib import Path
import unreal
preview=getattr(builtins,'_lightning057_preview',None)
if preview:
    preview['stop']()
name='T_UltimateLightning057_Atlas_04'
dest='/Game/GameXXK/UI/Battle/VFX/UltimateLightning057'
texture=unreal.load_asset(dest+'/'+name)
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).close_all_editors_for_asset(texture)
task=unreal.AssetImportTask()
task.filename=str(Path(unreal.Paths.project_dir())/'SourceArt/UI/Battle/VFX/UltimateLightning057/Atlases'/f'{name}.png')
task.destination_path=dest
task.destination_name=name
task.automated=True
task.replace_existing=True
task.save=True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture=unreal.load_asset(dest+'/'+name)
texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('compression_no_alpha',False)
save_asset_with_texture_budget(texture)
unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets([texture])
print(json.dumps({'reimported':texture.get_path_name()}))
