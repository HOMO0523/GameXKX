"""Import the approved transparent triangle; no existing art is overwritten."""
import json
from pathlib import Path
import unreal
from gamexxk_texture_budget import save_loaded_asset

root=Path(__file__).resolve().parents[2]
source=root/'SourceArt/UI/Controls/InkDisclosure-20260910/T_UI_InkDisclosureDown.png'
destination='/Game/GameXXK/UI/Controls/T_UI_InkDisclosureDown'
assert source.is_file()
task=unreal.AssetImportTask()
task.filename=str(source)
task.destination_path='/Game/GameXXK/UI/Controls'
task.destination_name='T_UI_InkDisclosureDown'
task.automated=True
task.replace_existing=False
task.save=False
texture=unreal.EditorAssetLibrary.load_asset(destination) if unreal.EditorAssetLibrary.does_asset_exist(destination) else None
if not texture:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture=unreal.EditorAssetLibrary.load_asset(destination)
assert isinstance(texture,unreal.Texture2D)
for key,value in {
    'mip_gen_settings':unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
    'compression_settings':unreal.TextureCompressionSettings.TC_EDITOR_ICON,
    'lod_group':unreal.TextureGroup.TEXTUREGROUP_UI,
    'filter':unreal.TextureFilter.TF_BILINEAR,
    'address_x':unreal.TextureAddress.TA_CLAMP,
    'address_y':unreal.TextureAddress.TA_CLAMP,
    'srgb':True,'never_stream':True,'compression_no_alpha':False,
}.items():texture.set_editor_property(key,value)
assert int(texture.blueprint_get_size_x())==256 and int(texture.blueprint_get_size_y())==128
assert save_loaded_asset(texture,unreal_module=unreal)
print(json.dumps({'asset':texture.get_path_name(),'size':[256,128],'saved':True}))
