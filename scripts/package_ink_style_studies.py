"""Package independently generated style studies; only canvas sizing and layout."""
import hashlib,json,zipfile
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont,ImageOps

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'SourceArt/Characters/ink-style-studies-20260910'
BG=(244,239,223)

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    jobs=json.loads((ART/'study-plan.json').read_text(encoding='utf-8'))['styles']
    assert len(jobs)==4
    out=ART/'characters';out.mkdir(exist_ok=True)
    rows=[]
    for j in jobs:
        raw=ART/'raw'/(j['slug']+'.png');source=Image.open(raw).convert('RGB')
        fitted=ImageOps.contain(source,(1600,1600),Image.Resampling.LANCZOS)
        canvas=Image.new('RGB',(1600,1600),BG);canvas.paste(fitted,((1600-fitted.width)//2,(1600-fitted.height)//2))
        p=out/(j['slug']+'.png');canvas.save(p)
        rows.append({'slug':j['slug'],'label':j['label'],'file':str(p.relative_to(ART)).replace('\\','/'),
                     'raw':'raw/'+raw.name,'nativeSize':list(source.size),'size':[1600,1600],
                     'rawSha256':digest(raw),'sha256':digest(p),'independentlyGenerated':True,
                     'postprocessing':'canvas fit only; no style filters','userApproved':False})
    assert len({r['rawSha256'] for r in rows})==4
    board=Image.new('RGB',(2400,800),BG);d=ImageDraw.Draw(board)
    title=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',36)
    label=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',30)
    note=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',23)
    d.text((30,20),'同一主角 · 四档独立重绘 · 从清晰块面逐步走向水墨',font=title,fill=(41,49,44))
    d.text((30,70),'比较笔触、设色与留白；每档均为独立生成的全身角色，未改现有游戏与排版。',font=note,fill=(94,95,81))
    for i,(j,r) in enumerate(zip(jobs,rows)):
        x=20+i*595
        im=Image.open(ART/r['file']).resize((560,560),Image.Resampling.LANCZOS)
        board.paste(im,(x,120))
        d.text((x+280,708),j['label'],font=label,fill=(41,49,44),anchor='mm')
        d.text((x+280,752),j['short'],font=note,fill=(94,95,81),anchor='mm')
    board.save(ART/'comparison.png')
    manifest={'count':4,'tool':'built-in image_gen','phase':'style-exploration-awaiting-user',
              'currentLayoutUnchanged':True,'userApproved':False,'records':rows}
    (ART/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    with zipfile.ZipFile(ART/'GameXXK_四档水墨画风尝试.zip','w',zipfile.ZIP_DEFLATED) as z:
        for folder in ['characters','prompts']:
            for p in sorted((ART/folder).glob('*')):z.write(p,p.relative_to(ART))
        for file in ['comparison.png','study-plan.json','manifest.json','README.md']:
            if (ART/file).exists():z.write(ART/file,file)
    print(json.dumps({'ok':True,'count':4,'individualCanvas':[1600,1600],'comparison':str(ART/'comparison.png')},ensure_ascii=False))

if __name__=='__main__':main()
