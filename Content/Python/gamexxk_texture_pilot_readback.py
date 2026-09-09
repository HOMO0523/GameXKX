from pathlib import Path
import json
import unreal
root=Path(unreal.Paths.get_project_file_path()).resolve().parent
p=unreal.load_asset('/Game/GameXXK/UI/ImageOptimizationPilot/T_StoryCompressionPilot')
r=dict(size=[p.blueprint_get_size_x(),p.blueprint_get_size_y()],memory=p.blueprint_get_memory_size(),
       settings={k:str(p.get_editor_property(k)) for k in ('power_of_two_mode','resize_during_build_x','resize_during_build_y','compression_settings')},
       source_id=p.blueprint_get_texture_source_id_string())
unreal.EditorAssetLibrary.save_loaded_asset(p,only_if_is_dirty=False)
a=unreal.EditorAssetLibrary.find_asset_data(p.get_path_name());r['format']=a.get_tag_value('Format')
(root/'Saved/ImageOptimization/pilot-readback.json').write_text(json.dumps(r,indent=2),encoding='utf-8')
print(json.dumps(r))
