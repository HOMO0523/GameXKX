"""Load registered textures in small batches and record actual settings/source references."""
from pathlib import Path
import gc
import json
import os
import time
import unreal

ROOT=Path(unreal.Paths.get_project_file_path()).resolve().parent
OUT=ROOT/'Saved/ImageOptimization'
data=json.loads((OUT/'registry.json').read_text(encoding='utf-8'))
prefix=os.environ.get('GAMEXXK_IMAGE_PREFIX','/Game')
limit=int(os.environ.get('GAMEXXK_IMAGE_LIMIT','0'))
selected=[n for n in data['textures'] if n['package'].startswith(prefix)]
if limit:selected=selected[:limit]
rows=[]
destination=OUT/'loaded-textures.json'
for i,entry in enumerate(selected):
    start=time.monotonic()
    row=dict(entry)
    try:
        texture=unreal.load_asset(entry['object_path'])
        row['loaded']=bool(texture)
        if texture:
            row['built_size']=[texture.blueprint_get_size_x(),texture.blueprint_get_size_y()] if isinstance(texture,unreal.Texture2D) else []
            row['resource_bytes']=texture.blueprint_get_memory_size()
            row['source_disk_memory_bytes']=list(texture.blueprint_get_texture_source_disk_and_memory_size())
            row['source_id']=texture.blueprint_get_texture_source_id_string()
            row['settings']={}
            for key in ('compression_settings','compression_no_alpha','compression_none','mip_gen_settings','lod_group','srgb','never_stream','filter','max_texture_size','lod_bias','power_of_two_mode','resize_during_build_x','resize_during_build_y','chroma_key_texture','chroma_key_color','virtual_texture_streaming'):
                try:
                    v=texture.get_editor_property(key)
                    row['settings'][key]=v if isinstance(v,(str,int,float,bool)) else str(v)
                except Exception:pass
            try:row['source_files']=list(texture.get_editor_property('asset_import_data').extract_filenames())
            except Exception:row['source_files']=[]
            row['source_files_exist']=[Path(p).is_file() for p in row['source_files']]
            # These exports are exact source snapshots, never re-imported automatically.
            dims=row['built_size']
            if '/GameXXK/UI/' in entry['package'] and dims and dims[0]*dims[1]*4>=512*1024:
                target=OUT/'SourceSnapshots'/entry['package'].removeprefix('/Game/')
                target=target.with_suffix('.png');target.parent.mkdir(parents=True,exist_ok=True)
                task=unreal.AssetExportTask();task.object=texture;task.filename=str(target)
                task.automated=True;task.prompt=False;task.replace_identical=True
                row['source_exported']=bool(unreal.Exporter.run_asset_export_task(task))
                if row['source_exported']:row['source_snapshot']=str(target)
        del texture
    except Exception as exc:
        row['error']=str(exc)
    row['load_seconds']=round(time.monotonic()-start,3)
    rows.append(row)
    if (i+1)%4==0:
        gc.collect();unreal.SystemLibrary.collect_garbage()
    if (i+1)%20==0 or i+1==len(selected):
        destination.write_text(json.dumps(dict(complete=i+1==len(selected),requested=len(selected),textures=rows),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(dict(inspected=i+1,total=len(selected),errors=sum('error' in n or not n.get('loaded') for n in rows))))
print(json.dumps(dict(report=str(destination),loaded=len(rows),errors=sum('error' in n or not n.get('loaded') for n in rows))))
