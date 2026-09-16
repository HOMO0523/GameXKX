"""Import the approved generated currency icon; leaves the original ingot intact."""
from pathlib import Path
import unreal

source = Path(unreal.Paths.project_dir()) / 'SourceArt/UI/Items/Currency/20260916/T_Item_GoldCoin.png'
task = unreal.AssetImportTask()
task.filename = str(source)
task.destination_path = '/Game/GameXXK/UI/Items'
task.destination_name = 'T_Item_GoldCoin'
task.automated = True
task.replace_existing = True
task.save = False
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset('/Game/GameXXK/UI/Items/T_Item_GoldCoin')
assert texture
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('srgb', True)
texture.set_editor_property('max_texture_size', 256)
assert unreal.EditorAssetLibrary.save_loaded_asset(texture)
print('GOLD_COIN_IMPORT_OK ' + texture.get_path_name())
