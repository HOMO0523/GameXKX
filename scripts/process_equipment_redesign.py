"""User-authorized equipment cutout, normalization, packaging and asset checks."""
import argparse
import hashlib
import json
import shutil
import zipfile
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageOps

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'SourceArt/UI/Equipment/gemstyle-redesign-20260910'

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def binary_propagation(seed, mask):
    result = seed & mask
    while True:
        expanded = result.copy()
        expanded[1:] |= result[:-1]
        expanded[:-1] |= result[1:]
        expanded[:,1:] |= result[:,:-1]
        expanded[:,:-1] |= result[:,1:]
        expanded &= mask
        if np.array_equal(expanded,result): return result
        result = expanded

def process(job):
    slug = job['slug']
    record = json.loads((ART/'prompts'/f'{slug}.json').read_text(encoding='utf-8'))
    source = Path(record['generatedPath'])
    raw = ART/'raw'/f'{slug}.png'
    raw.parent.mkdir(parents=True,exist_ok=True)
    if raw.exists() and sha(raw) != sha(source):
        previous = ART/'revisions'/(slug+'-superseded-'+sha(raw)[:12]+'.png')
        previous.parent.mkdir(exist_ok=True)
        if not previous.exists(): shutil.copy2(raw,previous)
    shutil.copy2(source,raw)
    im = Image.open(raw)
    a = np.array(im.convert('RGBA'))
    rgb = a[:,:,:3].copy()
    if im.mode == 'RGBA' and a[:,:,3].min() == 0:
        method = 'native-alpha'
        removed = int((a[:,:,3]==0).sum())
    else:
        c = a[:,:,:3].astype(np.int16)
        excess = np.minimum(c[:,:,0],c[:,:,2])-c[:,:,1]
        # Grow from unmistakable chroma background, including enclosed holes.
        # Blue-violet highlights are not seeds and remain behind dark outlines.
        balanced = np.abs(c[:,:,0]-c[:,:,2]) <= 24
        seed = (c[:,:,0]>225)&(c[:,:,2]>225)&(c[:,:,1]<55)&balanced
        candidate = (excess>22)&(c[:,:,1]<105)&balanced
        mask = binary_propagation(seed, mask=candidate)
        # Remove up to two source pixels of chroma bleed adjoining the cutout.
        # This stays at the contour; enclosed violet paint is never a seed.
        for _ in range(2):
            edge=mask.copy()
            edge[1:] |= mask[:-1]
            edge[:-1] |= mask[1:]
            edge[:,1:] |= mask[:,:-1]
            edge[:,:-1] |= mask[:,1:]
            mask |= edge & (excess>15) & (np.abs(c[:,:,0]-c[:,:,2])<70) & (c[:,:,1]<115)
        a[:,:,3][mask]=0
        removed=int(mask.sum())
        method='magenta-chroma-with-edge-cleanup'
    assert removed > 0 and np.array_equal(rgb,a[:,:,:3]),slug
    clean = Image.fromarray(a)
    bounds=clean.getchannel('A').point(lambda v:255 if v>=16 else 0).getbbox()
    assert bounds,slug
    crop=clean.crop(bounds)
    size=tuple(max(1,round(v*452/max(crop.size))) for v in crop.size)
    thumb=crop.resize(size,Image.Resampling.LANCZOS)
    final=Image.new('RGBA',(512,512),(0,0,0,0))
    final.alpha_composite(thumb,((512-size[0])//2,(512-size[1])//2))
    # Lanczos can create a few low-alpha chroma ringing pixels outside the ink.
    pixels=np.array(final).astype(np.int16)
    ringing=(pixels[:,:,0]>220)&(pixels[:,:,2]>220)&(pixels[:,:,1]<65)&(np.abs(pixels[:,:,0]-pixels[:,:,2])<35)&(pixels[:,:,3]<160)
    pixels[:,:,3][ringing]=0
    final=Image.fromarray(pixels.astype(np.uint8))
    if job.get('flipHorizontal'):
        final=ImageOps.mirror(final)
    path=ART/'icons'/f'{slug}.png'
    path.parent.mkdir(exist_ok=True)
    final.save(path)
    bgra=np.array(final)[:,:,[2,1,0,3]]
    alpha=np.array(final.getchannel('A'))
    assert not any(edge.any() for edge in (alpha[0],alpha[-1],alpha[:,0],alpha[:,-1])),slug
    assert (alpha>127).sum()>5000,slug
    pixels=np.array(final).astype(np.int16)
    magenta=(pixels[:,:,0]>225)&(pixels[:,:,2]>225)&(pixels[:,:,1]<55)&(np.abs(pixels[:,:,0]-pixels[:,:,2])<=24)&(pixels[:,:,3]>64)
    assert not magenta.any(),f'{slug}: chroma residual'
    return {'slug':slug,'name':job['name'],'set':job['set'],'slot':job['slot'],'asset':job['asset'],
        'raw':str(raw.relative_to(ROOT)).replace('\\','/'),'rawSha256':sha(raw),
        'icon':str(path.relative_to(ROOT)).replace('\\','/'),'sha256':sha(path),
        'size':[512,512],'mode':'RGBA','sourceBGRASha1':hashlib.sha1(bgra.tobytes()).hexdigest().upper(),'transparentBorder':True,'alphaBounds':list(final.getchannel('A').getbbox()),
        'opaquePixelCount':int((alpha>127).sum()),'removedBackgroundPixels':removed,'method':method,'chromaResidualPixels':int(magenta.sum()),
        'subjectPixelsRecoloredBeforeResize':0,'flipHorizontal':bool(job.get('flipHorizontal')),'visualReview':'pending','imported':False}

def review(jobs,records):
    by={r['slug']:r for r in records}
    font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',19)
    titlefont=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',28)
    small=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',14)
    columns=min(6,len(jobs))
    rows=(len(jobs)+columns-1)//columns
    label='强化石与洗炼砂' if len(jobs)==2 else '装备全新设计'
    for mode in ('overview','small-sizes'):
        cellw,cellh=(200,220) if mode=='overview' else (200,132)
        canvas=Image.new('RGB',(32+columns*cellw,100+cellh*rows),(239,233,218))
        d=ImageDraw.Draw(canvas)
        d.text((22,12),f'{label} · {len(records)}/{len(jobs)}',font=titlefont,fill=(34,51,46))
        d.text((23,55),'按名称与主题设计 · 宝石同系列' if mode=='overview' else '64px / 48px / 32px · 浅底和深底',font=font,fill=(75,89,82))
        for i,j in enumerate(jobs):
            if j['slug'] not in by:continue
            x,y=16+(i%columns)*cellw,95+(i//columns)*cellh
            icon=Image.open(ROOT/by[j['slug']]['icon']).convert('RGBA')
            if mode=='overview':
                d.rounded_rectangle((x+2,y,x+cellw-6,y+cellh-10),radius=12,fill=(249,246,237),outline=(215,211,195))
                thumb=icon.resize((180,180),Image.Resampling.LANCZOS)
                canvas.paste(thumb,(x+7,y+3),thumb)
                d.text((x+97,y+196),j['name'],font=font,anchor='mm',fill=(34,51,46))
            else:
                for offset,side in ((1,64),(75,48),(136,32)):
                    thumb=icon.resize((side,side),Image.Resampling.LANCZOS)
                    if side==48:d.rectangle((x+offset-3,y-3,x+offset+side+3,y+side+3),fill=(29,38,42))
                    canvas.paste(thumb,(x+offset,y),thumb)
                d.text((x+97,y+82),j['name'],font=font,anchor='mm',fill=(34,51,46))
        target=ART/'review'/f'equipment-{mode}.png'
        target.parent.mkdir(exist_ok=True)
        canvas.save(target)

def main():
    global ART
    parser=argparse.ArgumentParser()
    parser.add_argument('--complete',action='store_true')
    parser.add_argument('--materials',action='store_true')
    args=parser.parse_args()
    if args.materials: ART=ROOT/'SourceArt/UI/Items/gemstyle-resources-20260910'
    jobs=json.loads((ART/'art-jobs.json').read_text(encoding='utf-8'))['jobs']
    expected=2 if args.materials else 42
    previous_path=ART/'manifest.json'
    previous=json.loads(previous_path.read_text(encoding='utf-8')) if previous_path.exists() else {}
    previous_icons={r['slug']:r for r in previous.get('icons',[])}
    records=[process(j) for j in jobs if (ART/'prompts'/f"{j['slug']}.json").exists()]
    for r in records:
        old=previous_icons.get(r['slug'],{})
        if old.get('sha256')==r['sha256']:
            r['imported']=old.get('imported',False)
            r['visualReview']=old.get('visualReview','pending')
    if args.complete:
        assert len(records)==expected and len({r['slug'] for r in records})==expected
        assert len({r['sha256'] for r in records})==expected
        assert {r['slug'] for r in records}=={j['slug'] for j in jobs}
    report={'schemaVersion':2,'designBasis':'names-and-themes-only','expectedUniqueIcons':expected,
        'count':len(records),'complete':len(records)==expected,'tool':'built-in image_gen',
        'backgroundAndResizeAuthorized':True,'visualReview':'pending','icons':records}
    unchanged=len(records)==len(previous_icons) and all(previous_icons.get(r['slug'],{}).get('sha256')==r['sha256'] for r in records)
    if unchanged:
        for key in ('approvedForRuntime','approval','runtimeImported','importReport','visualReview'):
            if key in previous: report[key]=previous[key]
    (ART/'manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    review(jobs,records)
    if args.complete:
        zip_name='GameXXK_强化石与洗炼砂_2张.zip' if args.materials else 'GameXXK_装备全新设计_42张.zip'
        with zipfile.ZipFile(ART/zip_name,'w',zipfile.ZIP_DEFLATED) as archive:
            for folder in ('icons','prompts','review'):
                for path in sorted((ART/folder).glob('*')):archive.write(path,path.relative_to(ART))
            for file in ('art-jobs.json','manifest.json','README.md'):
                if (ART/file).exists():archive.write(ART/file,file)
    print(json.dumps({'count':len(records),'complete':len(records)==expected,'allChecksPassed':True,'directory':str(ART)},ensure_ascii=False))

if __name__=='__main__':main()
