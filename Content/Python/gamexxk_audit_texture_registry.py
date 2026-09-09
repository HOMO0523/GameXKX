"""Inventory registered project texture assets without loading levels or modifying assets."""
from pathlib import Path
import json
import unreal

ROOT=Path(unreal.Paths.get_project_file_path()).resolve().parent
OUT=ROOT/'Saved/ImageOptimization'
OUT.mkdir(parents=True,exist_ok=True)
registry=unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,
    include_hard_package_references=True,include_searchable_names=False,
    include_soft_management_references=False,include_hard_management_references=False)
assets=registry.get_assets_by_path('/Game',recursive=True)
rows=[]
for a in assets:
    cls=str(a.asset_class_path.asset_name)
    if not cls.startswith('Texture'):
        continue
    package=str(a.package_name)
    tags={}
    for key in ('Dimensions','Format','HasAlphaChannel','TextureGroup','LODGroup','CompressionSettings','ImportedSize','SourceFile','VirtualTextureStreaming'):
        try:
            value=a.get_tag_value(key)
            if value is not None:tags[key]=str(value)
        except Exception:pass
    local=ROOT/'Content'/(package.removeprefix('/Game/')+'.uasset')
    refs=sorted(str(p) for p in registry.get_referencers(package,options))
    rows.append(dict(package=package,object_path=package+'.'+str(a.asset_name),asset_class=cls,
                     tags=tags,uasset_bytes=local.stat().st_size if local.exists() else None,
                     referencers=refs))
result=dict(texture_count=len(rows),assets_scanned=len(assets),textures=rows)
(OUT/'registry.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(texture_count=len(rows),assets_scanned=len(assets),report=str(OUT/'registry.json')),ensure_ascii=False))
