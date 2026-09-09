"""Apply/read back bounded batches of source-preserving UE texture build settings."""
from pathlib import Path
import gc
import hashlib
import json
import shutil
import sys
import unreal

ROOT=Path(unreal.Paths.get_project_file_path()).resolve().parent
OUT=ROOT/'Saved/ImageOptimization'
plan=json.loads((ROOT/'SourceAssets/ImageDelivery/texture-budget-plan.json').read_text(encoding='utf-8'))
records={n['package']:n for n in plan['records'] if n['action']=='optimize'}
report_path=OUT/'applied-textures.json'
report=json.loads(report_path.read_text(encoding='utf-8')) if report_path.exists() else dict(version=1,textures={})
mode=sys.argv[1]
paths=sys.argv[2:]
results=[]
for path in paths:
    assert path in records,path
    n=records[path]
    t=unreal.load_asset(path)
    assert t and isinstance(t,unreal.Texture2D),path
    audit=json.loads(unreal.GameXXKTextureAuditLibrary.inspect_texture(t,True))
    assert audit.get('source_read'),path+': cannot verify original pixels'
    source_id=t.blueprint_get_texture_source_id_string()
    # Some imported legacy assets receive a new source GUID when loaded in the editor.
    # Preserve the currently loaded source through this operation, not a commandlet GUID.
    if mode=='verify':
        assert audit['source_pixels_sha1']==report['textures'][path]['source_pixels_sha1'],path+': embedded art pixels changed during optimization'
    if mode=='apply':
        local=ROOT/'Content'/(path.removeprefix('/Game/')+'.uasset')
        assert local.resolve().is_relative_to((ROOT/'Content').resolve())
        backup=OUT/'AssetBackup'/path.removeprefix('/Game/')
        backup=backup.with_suffix('.uasset');backup.parent.mkdir(parents=True,exist_ok=True)
        if not backup.exists():
            shutil.copy2(local,backup)
            for suffix in ('.uexp','.ubulk'):
                extra=local.with_suffix(suffix)
                if extra.exists():shutil.copy2(extra,backup.with_suffix(suffix))
        original_hash=hashlib.sha256(backup.read_bytes()).hexdigest()
        target=n['target_size']
        if target!=n['original_size']:
            t.set_editor_property('power_of_two_mode',unreal.TexturePowerOfTwoSetting.RESIZE_TO_SPECIFIC_RESOLUTION)
            t.set_editor_property('resize_during_build_x',target[0])
            t.set_editor_property('resize_during_build_y',target[1])
        t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_BC7)
        assert unreal.EditorAssetLibrary.save_loaded_asset(t)
        report['textures'][path]=dict(status='compiling',source_id=source_id,source_pixels_sha1=audit['source_pixels_sha1'],source_format=audit['source_format'],source_size=[audit['source_width'],audit['source_height']],audit_source_id=n['source_id'],original_uasset_sha256=original_hash,
            before_size=n['original_size'],target_size=target,before_settings=n['settings'],backup=str(backup))
        result=dict(path=path,status='compiling')
    elif mode=='verify':
        size=[audit['width'],audit['height']]
        memory=audit['resource_bytes']
        # RHI reports tiled allocation, not the raw block payload; small non-square textures
        # can need several 64KiB pages. Verify the actual saved pixel format below.
        ready=size==n['target_size'] and memory>0
        # Existing sRGB/filter/alpha settings and every source pixel identity must stay intact.
        same_flags=all(t.get_editor_property(k)==n['settings'][k] for k in ('srgb','compression_no_alpha','never_stream'))
        assert same_flags,path+': unrelated texture flags changed'
        assert str(t.get_editor_property('filter'))==n['settings']['filter']
        result=dict(path=path,status='ready' if ready else 'pending',size=size,resource_bytes=memory,
                    compression=str(t.get_editor_property('compression_settings')),source_id=t.blueprint_get_texture_source_id_string())
        if ready:
            assert unreal.EditorAssetLibrary.save_loaded_asset(t,only_if_is_dirty=False)
            result['format']=audit['format']
            if result['format']!='BC7':result['status']='format_mismatch'
            result['raw_bc7_payload_bytes']=n['estimated_after_bytes']
        report['textures'].setdefault(path,{}).update(result)
    else:raise ValueError(mode)
    results.append(result)
    del t
report_path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
if mode=='verify' and all(n['status']=='ready' for n in results):
    # Editor GC retains standalone assets. Explicitly unload only this saved batch;
    # otherwise hundreds of 4K source/platform buffers accumulate during offline work.
    gc.collect()
    packages=[unreal.find_package(path) for path in paths]
    packages=[p for p in packages if p]
    if packages:
        unloaded,error=unreal.EditorLoadingAndSavingUtils.unload_packages(packages)
        report['last_unload']=dict(any_unloaded=unloaded,error=str(error),paths=paths)
        report_path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(mode=mode,results=results,ready_count=sum(n.get('status')=='ready' for n in report['textures'].values())),ensure_ascii=False))
