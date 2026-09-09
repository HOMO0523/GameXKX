"""Cold object reload of a bounded batch, plus an importer-boundary regression check."""
from pathlib import Path
import gc
import json
import sys
import unreal

ROOT=Path(unreal.Paths.get_project_file_path()).resolve().parent
OUT=ROOT/'Saved/ImageOptimization'
applied=json.loads((OUT/'applied-textures.json').read_text(encoding='utf-8'))['textures']
mode=sys.argv[1] if len(sys.argv)>1 else 'reload'
paths=sys.argv[2:]
result=[]
for path in paths:
    before=applied[path]
    t=unreal.load_asset(path)
    assert t,path
    data=json.loads(unreal.GameXXKTextureAuditLibrary.inspect_texture(t,True))
    assert data['format']=='BC7',path+': not actually BC7'
    assert [data['width'],data['height']]==before['target_size'],path+': built size drift'
    assert data['source_pixels_sha1']==before['source_pixels_sha1'],path+': source pixel drift'
    if mode=='import-boundary':
        from gamexxk_texture_budget import save_loaded_asset
        t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        assert save_loaded_asset(t)
        after=json.loads(unreal.GameXXKTextureAuditLibrary.inspect_texture(t,True))
        assert after['format']=='BC7' and after['source_pixels_sha1']==data['source_pixels_sha1']
        data=after
    result.append(dict(path=path,**data))
    del t
    gc.collect()
    package=unreal.find_package(path)
    if package:unreal.EditorLoadingAndSavingUtils.unload_packages([package])
record=OUT/'reload-verification.json'
old=json.loads(record.read_text(encoding='utf-8')) if record.exists() else dict(textures={})
for item in result:old['textures'][item['path']]=item
if mode=='import-boundary':old['import_boundary_checked']=paths
record.write_text(json.dumps(old,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(mode=mode,verified=len(result),total_verified=len(old['textures'])),ensure_ascii=False))
