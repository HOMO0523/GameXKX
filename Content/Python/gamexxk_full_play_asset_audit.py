"""Export current runtime textures and read their settings for an evidence-only art audit."""
from pathlib import Path
import json
import unreal
from gamexxk_validate_battle_animation_texture_memory import _texture_size,_resource_size_bytes

root=Path(unreal.Paths.get_project_file_path()).resolve().parent
out=root/'Saved/Codex/FullPlayAudit-20260906/atlases';out.mkdir(exist_ok=True)
paths=['/Game/GameXXK/UI/Training/Generated/walkloop_pilot_v1/character_00_hero_walk_left/atlas_1K/T_TrainingHeroWalkLeft_1K']
paths += ['/Game/GameXXK/BattleAnimations/Atlases/T_'+base+'_atlas' for base in ('character_00_hero_1k_idle','character_00_hero_1k_attack_punch','character_01_blade_1k_idle','character_01_blade_1k_attack','character_07_tusi_chief_1k_idle','enemy_01_rooster_1k_idle','enemy_01_rooster_1k_attack','enemy_05_ironfeather_1k_idle','enemy_05_ironfeather_1k_attack')]
rows=[]
for path in paths:
    texture=unreal.load_asset(path)
    row={'path':path,'loaded':bool(texture)}
    if texture:
        row['size']=_texture_size(texture);row['memory_bytes']=_resource_size_bytes(texture)
        for key in ('filter','compression_settings','mip_gen_settings','lod_group','srgb','never_stream'):
            try:row[key]=str(texture.get_editor_property(key))
            except Exception:pass
        try:row['source_files']=list(texture.get_editor_property('asset_import_data').extract_filenames())
        except Exception:pass
        task=unreal.AssetExportTask();task.object=texture;task.filename=str(out/(texture.get_name()+'.png'));task.automated=True;task.prompt=False;task.replace_identical=True
        row['exported']=bool(unreal.Exporter.run_asset_export_task(task));row['file']=task.filename
    rows.append(row)
(out/'runtime-assets.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(rows,ensure_ascii=False))
