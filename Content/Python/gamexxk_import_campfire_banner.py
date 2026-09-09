"""Import the versioned campfire illustration, preserving its full 3:1 composition."""

def save_asset_with_texture_budget(*args, **kwargs):
    from gamexxk_texture_budget import save_loaded_asset
    return save_loaded_asset(*args, unreal_module=unreal, **kwargs)

import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
source = root / 'SourceArt/UI/RouteCamp/campfire_rest_banner_v3.png'
task = unreal.AssetImportTask()
task.filename = str(source)
task.destination_path = '/Game/GameXXK/UI/RouteCamp'
task.destination_name = 'T_CampfireRestBanner_V3'
task.automated = True
task.replace_existing = False
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset('/Game/GameXXK/UI/RouteCamp/T_CampfireRestBanner_V3')
if not texture:
    raise RuntimeError('Campfire texture import failed')
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('srgb', True)
texture.set_editor_property('never_stream', True)
save_asset_with_texture_budget(texture)
print(json.dumps({'asset': texture.get_path_name(), 'source': str(source), 'size': [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()]}))
