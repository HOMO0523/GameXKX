"""Package red compact blade revisions; scale review canvases uniformly only."""
import hashlib
import json
import shutil
import zipfile
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageOps
from package_project_design_characters import bounds

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / 'SourceArt/Characters/female-blade-options-20260912'
OUT = BASE / 'refinements/red-compact-v2-20260913'
BG = (244,239,223)
def read(p): return json.loads(p.read_text(encoding='utf-8'))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p, x): p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def font(n): return ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',n)

def sheet(items, path, title, subtitle, cols, cell=430):
    rows=(len(items)+cols-1)//cols
    canvas=Image.new('RGB',(cols*cell+40,110+rows*(cell+54)),BG)
    d=ImageDraw.Draw(canvas)
    d.text((24,14),title,font=font(30),fill=(35,46,41))
    d.text((24,62),subtitle,font=font(18),fill=(91,95,82))
    for i,(label,p) in enumerate(items):
        x=20+(i%cols)*cell; y=104+(i//cols)*(cell+54)
        with Image.open(p) as im:
            box,_=bounds(im)
            crop=im.convert('RGB').crop(box)
            fit=ImageOps.contain(crop,(cell-36,cell-28),Image.Resampling.LANCZOS)
            canvas.paste(fit,(x+(cell-fit.width)//2,y+cell-14-fit.height))
        d.text((x+cell/2,y+cell+20),label,font=font(22),fill=(35,46,41),anchor='mm')
    canvas.save(path)

def main():
    for name in ['raw','characters','review']: (OUT/name).mkdir(exist_ok=True)
    records=[]
    jobs=read(OUT/'design-brief.json')['jobs']
    for j in jobs:
        pf=OUT/'prompts'/(j['slug']+'.json'); p=read(pf)
        source=Path(p['generatedPath']); raw=OUT/'raw'/(j['slug']+'.png')
        if source.resolve()!=raw.resolve(): shutil.copy2(source,raw)
        p.setdefault('originalGeneratedPath',str(source)); p['generatedPath']=str(raw); write(pf,p)
        with Image.open(raw) as im:
            native=list(im.size)
            fit=ImageOps.contain(im.convert('RGB'),(1360,1360),Image.Resampling.LANCZOS)
            full=Image.new('RGB',(1600,1600),BG)
            full.paste(fit,((1600-fit.width)//2,(1600-fit.height)//2))
        box,_=bounds(full)
        assert min(box[0],box[1],1600-box[2],1600-box[3])>=120
        target=OUT/'characters'/(j['slug']+'.png'); full.save(target)
        records.append(dict(slug=j['slug'],name=j['name'],file=str(target.relative_to(OUT)),nativeSize=native,size=[1600,1600],sha256=sha(target),rawSha256=sha(raw),foregroundBounds=box,imported=False))
    assert len(records)==4 and len({r['rawSha256'] for r in records})==4
    new=[(r['name'],OUT/r['file']) for r in records]
    sheet(new,OUT/'review/four-options.png','女刀客 · 红色统一与紧凑比例修订','保留四款性格与曲线 · 大色块与简化轮廓 · 待选择',4)
    sheet(new,OUT/'review/four-options-large.png','女刀客 · 四款修订单体','A 张扬 / B 冷艳 / C 俏皮 / D 慵懒',2,700)
    old=[('原稿 '+j['name'],BASE/'raw'/(j['slug']+'.png')) for j in jobs]
    sheet(old+new,OUT/'review/before-after.png','比例与配色 · 上原稿 / 下修订','仅等比缩放展示，图中人物比例由重绘完成',4)
    refbase=ROOT/'SourceArt/Characters/project-design-gemstyle-all-20260910/characters'
    refs=[('现有主角',refbase/'hero.png'),('现有弓手',refbase/'hunter.png'),('现有药师',refbase/'healer.png')]
    reference_hashes={str(p.relative_to(ROOT)):sha(p) for _,p in refs}
    sheet(refs+new,OUT/'review/roster-style-comparison.png','现有角色 × 女刀客修订 · 画风对照','等比缩放至同一展示框，对照头身比例、眼睛、描边和明暗；非局内身高设定',7,350)
    write(OUT/'manifest.json',dict(count=4,phase='review-only',records=records,referenceHashes=reference_hashes))
    (OUT/'README.txt').write_text('四款女刀客红色与紧凑比例修订\n\nA 张扬妩媚 / B 冷艳克制 / C 俏皮灵动 / D 慵懒危险\n统一红色点缀，保留中性色、曲线与服装剪裁；重绘紧凑比例与宝石大块面。\ncharacters 为1600×1600审阅画布，raw保留原生生成尺寸。未导入游戏。\nreview含四款总览、前后对比、现有角色画风对照。\n',encoding='utf-8-sig')
    archive=OUT/'GameXXK_四款女刀客_红色紧凑版.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for folder in ['characters','raw','review','prompts']:
            for p in sorted((OUT/folder).glob('*')):
                if p.is_file(): z.write(p,p.relative_to(OUT))
        for name in ['manifest.json','design-brief.json','README.txt']:z.write(OUT/name,name)
    with zipfile.ZipFile(archive) as z: assert z.testzip() is None
    assert all(sha(ROOT/p)==h for p,h in reference_hashes.items())
    print(json.dumps(dict(count=4,size=[1600,1600],unique=True,safeBounds=True,referenceUnchanged=True,zipIntegrity=True,zipBytes=archive.stat().st_size)))

if __name__=='__main__': main()
