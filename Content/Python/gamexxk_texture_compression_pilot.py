"""Verify UE build-only resize and BC7 while preserving the embedded art source."""
from pathlib import Path
import json
import unreal

ROOT=Path(unreal.Paths.get_project_file_path()).resolve().parent
OUT=ROOT/'Saved/ImageOptimization'
subsystem=unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
source_path='/Game/GameXXK/UI/StoryNodes/T_Story_S00_01'
pilot_path='/Game/GameXXK/UI/ImageOptimizationPilot/T_StoryCompressionPilot'
source=unreal.load_asset(source_path)
assert source
pilot=unreal.load_asset(pilot_path) if unreal.EditorAssetLibrary.does_asset_exist(pilot_path) else subsystem.duplicate_asset(source_path,pilot_path)
assert pilot
before=dict(source_id=pilot.blueprint_get_texture_source_id_string(),size=[pilot.blueprint_get_size_x(),pilot.blueprint_get_size_y()])
pilot.set_editor_property('power_of_two_mode',unreal.TexturePowerOfTwoSetting.RESIZE_TO_SPECIFIC_RESOLUTION)
pilot.set_editor_property('resize_during_build_x',1536)
pilot.set_editor_property('resize_during_build_y',512)
pilot.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_BC7)
assert unreal.EditorAssetLibrary.save_loaded_asset(pilot)
after=dict(source_id=pilot.blueprint_get_texture_source_id_string(),size=[pilot.blueprint_get_size_x(),pilot.blueprint_get_size_y()],memory=pilot.blueprint_get_memory_size())
data=unreal.EditorAssetLibrary.find_asset_data(pilot_path)
after['format']=data.get_tag_value('Format')
after['dimensions_tag']=data.get_tag_value('Dimensions')
task=unreal.AssetExportTask();task.object=pilot;task.filename=str(OUT/'pilot.dds');task.automated=True;task.prompt=False;task.replace_identical=True
after['dds_exported']=bool(unreal.Exporter.run_asset_export_task(task))
report=dict(before=before,after=after,asset=pilot_path)
(OUT/'compression-pilot.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False))
assert before['source_id']==after['source_id'],'build-only resizing changed embedded source'
assert after['size']==[1536,512]
