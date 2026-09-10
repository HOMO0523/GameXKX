import json
from pathlib import Path
import unreal

root=Path(str(unreal.Paths.project_dir())).resolve()
manifest=json.loads((root/'SourceAssets/AnimationProduction/gemstyle-pilot-20260910/keyframe-pilot/manifest.json').read_text(encoding='utf-8'))
directory='/Game/GameXXK/BattleAnimations/GemstylePilot'
unreal.EditorAssetLibrary.make_directory(directory)
records=[]
for r in manifest['records']:
    task=unreal.AssetImportTask()
    task.filename=str(root/r['atlas']);task.destination_path=directory;task.destination_name=r['textureName']
    task.automated=True;task.save=False;task.replace_existing=True;task.replace_existing_settings=False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture=unreal.load_asset(r['assetPath']);assert texture is not None
    texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property('filter',unreal.TextureFilter.TF_BILINEAR)
    texture.set_editor_property('srgb',True)
    texture.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture)
    size=[texture.blueprint_get_size_x(),texture.blueprint_get_size_y()];assert size==[2048,2048]
    records.append({'id':r['id'],'path':texture.get_path_name(),'size':size,'frameCount':r['frameCount'],'fps':r['fps']})
report={'ok':True,'newSiblingAssetsOnly':True,'records':records}
path=root/'Saved/Diagnostics/GemstyleKeyframePilot-20260910/import.json';path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False))
