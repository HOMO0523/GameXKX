"""Encode actual UE-exported UI source snapshots as validated lightweight WebP derivatives."""
from pathlib import Path
import hashlib
import json
import numpy as np
from PIL import Image,ImageOps
from project_texture_budget import ROOT


def main():
    plan=json.loads((ROOT/'SourceAssets/ImageDelivery/texture-budget-plan.json').read_text(encoding='utf-8'))
    out=ROOT/'SourceAssets/ImageDelivery/WebP';out.mkdir(parents=True,exist_ok=True)
    manifest_file=out.parent/'webp-manifest.json'
    manifest=json.loads(manifest_file.read_text(encoding='utf-8')) if manifest_file.exists() else dict(version=1,images={})
    jobs=[n for n in plan['records'] if n.get('source_snapshot')]
    for index,n in enumerate(jobs):
        source=Path(n['source_snapshot'])
        digest=hashlib.sha256(source.read_bytes()).hexdigest()
        original=Image.open(source)
        mode='RGBA' if 'A' in original.getbands() else 'RGB'
        original=original.convert(mode)
        size=n.get('target_size',list(original.size))
        old=manifest['images'].get(n['package'],{})
        path=(out/n['package'].removeprefix('/Game/')).with_suffix('.webp')
        if old.get('source_sha256')==digest and old.get('size')==size and path.exists() and hashlib.sha256(path.read_bytes()).hexdigest()==old.get('sha256'):
            continue
        # Resampling is only for the display derivative; existing UE source images remain untouched.
        image=ImageOps.contain(original,tuple(size),Image.Resampling.LANCZOS)
        if image.size!=tuple(size):
            padded=Image.new(mode,tuple(size),(244,234,213,0) if mode=='RGBA' else (244,234,213))
            padded.paste(image,((size[0]-image.width)//2,(size[1]-image.height)//2))
            image=padded
        path.parent.mkdir(parents=True,exist_ok=True)
        quality=90
        image.save(path,'WEBP',quality=quality,method=6,exact=True)
        # Keep quality 90 on complex atlases; no forced budget that erases their frame detail.
        is_atlas='atlas' in n['package'].lower() or 'walkloop' in n['package'].lower()
        while path.stat().st_size>500_000 and quality>82 and not is_atlas:
            quality-=4;image.save(path,'WEBP',quality=quality,method=6,exact=True)
        decoded=Image.open(path).convert(mode)
        assert decoded.size==image.size
        alpha_exact=(mode!='RGBA' or np.array_equal(np.asarray(image.getchannel('A')),np.asarray(decoded.getchannel('A'))))
        assert alpha_exact, n['package']+': alpha changed during encoding'
        assert hashlib.sha256(source.read_bytes()).hexdigest()==digest
        manifest['images'][n['package']]=dict(file=path.relative_to(ROOT).as_posix(),size=list(image.size),mode=mode,
            bytes=path.stat().st_size,source_bytes=source.stat().st_size,source_snapshot=str(source),source_sha256=digest,
            sha256=hashlib.sha256(path.read_bytes()).hexdigest(),quality=quality,alpha_exact=alpha_exact,
            budget_exception='animation_atlas_grid' if is_atlas and path.stat().st_size>500_000 else 'quality_floor' if path.stat().st_size>500_000 else None)
        if (index+1)%20==0 or index+1==len(jobs):
            manifest_file.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
            print(json.dumps(dict(encoded=index+1,total=len(jobs))),flush=True)
    manifest_file.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    rows=list(manifest['images'].values())
    print(json.dumps(dict(images=len(rows),source_bytes=sum(n['source_bytes'] for n in rows),webp_bytes=sum(n['bytes'] for n in rows),manifest=str(manifest_file))))


if __name__=='__main__':main()
