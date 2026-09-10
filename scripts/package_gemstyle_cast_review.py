"""Package full-body design candidates for review; never import or replace actors."""
import argparse
import hashlib
import json
import shutil
import zipfile
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont,ImageOps

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'SourceArt/Characters/gemstyle-redesign-20260910'
BG=(244,239,223)

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def foreground(im):
    c=np.array(im.convert('RGB')).astype(np.int16)
    background=np.median(np.concatenate([c[:8].reshape(-1,3),c[-8:].reshape(-1,3),c[:,:8].reshape(-1,3),c[:,-8:].reshape(-1,3)]),axis=0)
    return np.max(np.abs(c-background),axis=2)>40

def package_one(j):
    record=json.loads((ART/'prompts-v2'/(j['slug']+'.json')).read_text(encoding='utf-8'))
    source=Path(record['generatedPath']);raw=ART/'raw-v2'/(j['slug']+'.png')
    raw.parent.mkdir(parents=True,exist_ok=True)
    if raw.exists() and digest(raw)!=digest(source):
        revision=ART/'revisions'/(j['slug']+'-superseded-'+digest(raw)[:10]+'.png')
        revision.parent.mkdir(exist_ok=True)
        if not revision.exists():shutil.copy2(raw,revision)
    shutil.copy2(source,raw)
    original=Image.open(raw).convert('RGB')
    image=ImageOps.contain(original,(1600,1600),Image.Resampling.LANCZOS)
    final=Image.new('RGB',(1600,1600),BG)
    final.paste(image,((1600-image.width)//2,(1600-image.height)//2))
    if j.get('reviewFlipHorizontal'):final=ImageOps.mirror(final)
    target=ART/'fullbody-1600'/j['group']/(j['slug']+'.png')
    target.parent.mkdir(parents=True,exist_ok=True)
    final.save(target)
    mask=foreground(final)
    yy,xx=np.where(mask)
    assert len(xx)>15000,j['slug']+' empty subject'
    bounds=[int(xx.min()),int(yy.min()),int(xx.max())+1,int(yy.max())+1]
    assert min(bounds[0],bounds[1],1600-bounds[2],1600-bounds[3])>=8,j['slug']+' touches canvas edge'
    assert digest(ROOT/j['identityReference'])==j['identitySourceSha256'],j['slug']+' source identity changed'
    return {'slug':j['slug'],'id':j['id'],'name':j['name'],'group':j['group'],'tier':j.get('tier'),
            'raw':str(raw.relative_to(ROOT)).replace('\\','/'),'rawSha256':digest(raw),'nativeSize':list(original.size),
            'reviewPng':str(target.relative_to(ROOT)).replace('\\','/'),'sha256':digest(target),'size':[1600,1600],
            'mode':'RGB','background':'warm-ivory-review','foregroundBounds':bounds,
            'identityReference':j['identityReference'],'identitySourceSha256':j['identitySourceSha256'],
            'reviewCanvasResampled':original.size!=(1600,1600),'visualReview':'pending','userApproved':False,'imported':False}

def sheets(jobs,records):
    by={r['slug']:r for r in records}
    titlefont=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',30)
    font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',22)
    small=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',16)
    out=ART/'review';out.mkdir(exist_ok=True)
    for group,columns,label in [('characters',5,'角色'),('monsters',7,'怪物')]:
        selected=[j for j in jobs if j['group']==group]
        cell=300 if group=='characters' else 240
        rows=(len(selected)+columns-1)//columns
        sheet=Image.new('RGB',(columns*cell+32,rows*(cell+48)+100),BG)
        silhouettes=Image.new('RGB',(columns*cell+32,rows*175+100),BG)
        d=ImageDraw.Draw(sheet);sd=ImageDraw.Draw(silhouettes)
        n=sum(j['slug'] in by for j in selected)
        d.text((20,15),f'{label} · 全新设计 · {n}/{len(selected)} · 待确认',font=titlefont,fill=(33,49,44))
        d.text((20,57),'以现有1600全身稿核对身份 · 重新设计轮廓、体型、配色与姿态',font=small,fill=(82,91,78))
        sd.text((20,15),f'{label} · 96px色块与剪影检查 · 待确认',font=titlefont,fill=(33,49,44))
        for i,j in enumerate(selected):
            if j['slug'] not in by:continue
            im=Image.open(ROOT/by[j['slug']]['reviewPng']).convert('RGB')
            x=16+(i%columns)*cell;y=94+(i//columns)*(cell+48)
            sheet.paste(im.resize((cell,cell),Image.Resampling.LANCZOS),(x,y))
            text=j['name']
            if group=='monsters':text+=' · '+{'normal':'普通','elite':'精英','boss':'首领'}[j['tier']]
            d.text((x+cell//2,y+cell+17),text,font=font if group=='characters' else small,anchor='mm',fill=(33,49,44))
            sy=90+(i//columns)*175
            silhouettes.paste(im.resize((96,96),Image.Resampling.LANCZOS),(x+15,sy))
            mask=Image.fromarray((foreground(im)*255).astype(np.uint8)).resize((96,96),Image.Resampling.LANCZOS)
            silhouettes.paste((28,38,39),(x+123,sy,x+219,sy+96),mask)
            sd.text((x+cell//2,sy+124),j['name'],font=small,anchor='mm',fill=(33,49,44))
        sheet.save(out/(group+'-overview.png'));silhouettes.save(out/(group+'-silhouettes.png'))

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--complete',action='store_true');args=parser.parse_args()
    jobs=json.loads((ART/'art-jobs.json').read_text(encoding='utf-8'))['jobs']
    records=[package_one(j) for j in jobs if (ART/'prompts-v2'/(j['slug']+'.json')).exists()]
    if args.complete:
        assert len(records)==34 and len({r['id'] for r in records})==34
        assert len({r['rawSha256'] for r in records})==34
        assert sum(r['group']=='characters' for r in records)==13
        assert sum(r['group']=='monsters' for r in records)==21
    report={'schemaVersion':2,'count':len(records),'expected':34,'complete':len(records)==34,
            'phase':'design-review-awaiting-user','userApproved':False,'imported':False,
            'tool':'built-in image_gen','records':records}
    (ART/'manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    sheets(jobs,records)
    if args.complete:
        with zipfile.ZipFile(ART/'GameXXK_角色与怪物_34张全身审阅稿.zip','w',zipfile.ZIP_DEFLATED) as z:
            for directory in ['fullbody-1600','review','prompts-v2']:
                for p in sorted((ART/directory).rglob('*')):
                    if p.is_file():z.write(p,p.relative_to(ART))
            for file in ['art-jobs.json','manifest.json','README.md']:
                if (ART/file).exists():z.write(ART/file,file)
    print(json.dumps({'count':len(records),'complete':len(records)==34,'size':[1600,1600],'phase':'awaiting-user-review'},ensure_ascii=False))

if __name__=='__main__':main()
