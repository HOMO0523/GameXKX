"""Package four user-selected archers with individual themed bow-and-arrow sets."""
import json,shutil,hashlib,zipfile
from pathlib import Path
from PIL import Image,ImageOps
from package_project_design_characters import bounds
from package_blade_red_compact import sheet
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/Characters/female-archer-options-20260913/themed-bows-v1'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def write(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    for folder in ['raw','characters','review']:(OUT/folder).mkdir(exist_ok=True)
    jobs=read(OUT/'design-brief.json')['jobs'];records=[]
    for j in jobs:
        pf=OUT/'prompts'/(j['slug']+'.json');p=read(pf);source=Path(p['generatedPath']);raw=OUT/'raw'/(j['slug']+'.png')
        if source.resolve()!=raw.resolve():shutil.copy2(source,raw)
        p.setdefault('originalGeneratedPath',str(source));p['generatedPath']=str(raw);write(pf,p)
        with Image.open(raw) as im:
            native=list(im.size);fit=ImageOps.contain(im.convert('RGB'),(1360,1360),Image.Resampling.LANCZOS)
            full=Image.new('RGB',(1600,1600),(244,239,223));full.paste(fit,((1600-fit.width)//2,(1600-fit.height)//2))
        dest=OUT/'characters'/(j['slug']+'.png');full.save(dest);box,_=bounds(full)
        assert min(box[0],box[1],1600-box[2],1600-box[3])>=120
        assert sha(Path(j['source']))==sha(Path(j['reference']))
        records.append(dict(slug=j['slug'],name=j['name'],file=str(dest.relative_to(OUT)),raw=str(raw.relative_to(OUT)),nativeSize=native,size=[1600,1600],bounds=box,sha256=sha(dest),rawSha256=sha(raw),referenceSha256=sha(Path(j['reference']))))
    assert len(records)==4 and len({r['rawSha256'] for r in records})==4
    items=[(r['name'],OUT/r['file']) for r in records]
    sheet(items,OUT/'review/four-themed-bows.png','四款女弓手 · 专属弓箭设计','流云圆弧 / 燕翎流线 / 劲竹折线 / 素月长弧 · 国风宝石画风',4,470)
    sheet(items,OUT/'review/four-themed-bows-large.png','四款女弓手 · 专属弓箭放大核对','按用户选定四张造型修改弓、箭头、箭羽和箭筒细节',2,750)
    old=[('原弓箭 '+j['name'].split(' · ')[0],Path(j['reference'])) for j in jobs]
    sheet(old+items,OUT/'review/before-after.png','弓箭设计对照 · 上原稿 / 下专属设计','仅等比缩放排版，人物插画由imagegen编辑',4,430)
    sheet(items,OUT/'review/readability.png','小尺寸弓箭辨识','检查造型、色块、弓弦与箭杆',4,220)
    write(OUT/'manifest.json',dict(count=4,imported=False,phase='themed-weapons-awaiting-review',records=records))
    (OUT/'README.txt').write_text('四款女弓手 · 专属弓箭核对包\n\n杏风：暖杏木流云轻弓、圆弧造型、杏白箭羽。\n燕羽：墨青燕翎风弓、大羽形弓臂、青灰燕尾箭羽。\n竹锋：竹绿劲竹战弓、明确竹节与硬朗转折、深绿箭羽。\n月竹：月白素月弧弓、简洁长弧、白青箭羽。\n\n以用户本次指定四张低持备战图为准；箭尚未搭弦。造型参考保存在references。\ncharacters为1600×1600审阅画布，原始尺寸见raw与manifest。\n未导入游戏，待逐项核对。\n',encoding='utf-8-sig')
    archive=OUT/'GameXXK_四款女弓手_专属弓箭核对包.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for folder in ['raw','characters','review','prompts','references']:
            for p in sorted((OUT/folder).glob('*')):
                if p.is_file():z.write(p,p.relative_to(OUT))
        for name in ['design-brief.json','manifest.json','README.txt']:z.write(OUT/name,name)
    with zipfile.ZipFile(archive) as z:assert z.testzip() is None
    print(json.dumps(dict(count=4,unique=True,safeBounds=True,referenceHashes=True,zipIntegrity=True,zipBytes=archive.stat().st_size)))
if __name__=='__main__':main()
