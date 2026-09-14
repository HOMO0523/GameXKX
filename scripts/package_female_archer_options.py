"""Package three new archer candidates and the exact approved swallow master."""
import json,shutil,hashlib,zipfile
from pathlib import Path
from PIL import Image,ImageOps,ImageDraw,ImageFont
from package_blade_red_compact import sheet
from package_project_design_characters import bounds
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/Characters/female-archer-options-20260913'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def write(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    for folder in ['raw','characters','review']:(OUT/folder).mkdir(exist_ok=True)
    brief=read(OUT/'design-brief.json');jobs=[dict(brief['approved'],approved=True)]+[dict(j,approved=False) for j in brief['jobs']]
    records=[]
    for j in jobs:
        raw=OUT/'raw'/(j['slug']+'.png')
        if j['approved']:source=Path(j['source'])
        else:
            pf=OUT/'prompts'/(j['slug']+'.json');p=read(pf);source=Path(p['generatedPath'])
        if raw.resolve()!=source.resolve():shutil.copy2(source,raw)
        if not j['approved']:
            p.setdefault('originalGeneratedPath',str(source));p['generatedPath']=str(raw);write(pf,p)
        with Image.open(raw) as im:
            native=list(im.size);fit=ImageOps.contain(im.convert('RGB'),(1360,1360),Image.Resampling.LANCZOS)
            full=Image.new('RGB',(1600,1600),(244,239,223));full.paste(fit,((1600-fit.width)//2,(1600-fit.height)//2))
        dest=OUT/'characters'/(j['slug']+'.png');full.save(dest);box,_=bounds(full)
        assert min(box[0],box[1],1600-box[2],1600-box[3])>=120
        records.append(dict(slug=j['slug'],name=j['name'],summary=j['summary'],approved=j['approved'],file=str(dest.relative_to(OUT)),nativeSize=native,size=[1600,1600],bounds=box,sha256=sha(dest),rawSha256=sha(raw)))
    assert len(records)==4 and len({r['rawSha256'] for r in records})==4
    assert sha(OUT/'raw/A_swallow.png')==sha(Path(brief['approved']['source']))
    items=[(r['name'],OUT/r['file']) for r in records]
    sheet(items,OUT/'review/four-archers.png','国风女弓手 · 四款设计方向','A为确认款 · B/C/D为新增试稿 · 不同性格与剪裁 · 统一宝石画风',4,460)
    sheet(items,OUT/'review/four-archers-large.png','国风女弓手 · 四款放大核对','燕羽清灵 / 竹锋飒爽 / 杏风俏皮 / 月竹沉静',2,740)
    sheet(items,OUT/'review/readability.png','小尺寸轮廓辨识','同展示框等比缩放，非局内身高设定',4,220)
    write(OUT/'manifest.json',dict(count=4,newCount=3,approvedAExact=True,imported=False,phase='design-options-awaiting-user-selection',records=records))
    (OUT/'README.txt').write_text('国风女弓手 · 四款设计方向\n\n'+ '\n'.join(r['name']+'：'+r['summary'] for r in records)+'\n\nA直接沿用用户确认图，raw字节一致。B/C/D为三个独立新设计，不同发型、脸型、剪裁和个性。\ncharacters为1600×1600审阅画布，原生生成尺寸见raw与manifest。\n本批均为低持备战、箭尚未搭弦的造型。先看review总览，再按A/B/C/D或角色名反馈。\n尚未导入游戏，未制作动画。\n',encoding='utf-8-sig')
    archive=OUT/'GameXXK_四款国风女弓手_设计选择包.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for folder in ['raw','characters','review','prompts']:
            for p in sorted((OUT/folder).glob('*')):
                if p.is_file():z.write(p,p.relative_to(OUT))
        for name in ['design-brief.json','manifest.json','README.txt']:z.write(OUT/name,name)
    with zipfile.ZipFile(archive) as z:assert z.testzip() is None
    print(json.dumps(dict(count=4,newCount=3,approvedAExact=True,unique=True,safeBounds=True,zipIntegrity=True,zipBytes=archive.stat().st_size)))
if __name__=='__main__':main()
