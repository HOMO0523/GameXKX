"""Package butterfly-element Taoist design variants for review."""
import json,shutil,hashlib,zipfile
from pathlib import Path
from PIL import Image,ImageOps
from package_blade_red_compact import sheet
from package_project_design_characters import bounds
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/Characters/female-taoist-options-20260913/butterfly-elements-v2'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def write(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    for d in ['raw','characters','review','references']:(OUT/d).mkdir(exist_ok=True)
    brief=read(OUT/'design-brief.json')
    style=Path('D:/UE5 demo/GameXXK/SourceArt/Characters/cast-unified-20260913/references/approved_blade.png')
    user_ref=Path('C:/Users/shxuw/AppData/Local/Temp/codex-clipboard-13868676-ffcd-4659-9c2d-4e2c86c89682.png')
    shutil.copy2(style,OUT/'references/approved-gemstyle.png');shutil.copy2(user_ref,OUT/'references/user-butterfly-inspiration.png')
    records=[]
    for j in brief['jobs']:
        pf=OUT/'prompts'/(j['slug']+'.json');p=read(pf);source=Path(p['generatedPath']);raw=OUT/'raw'/(j['slug']+'.png')
        if source.resolve()!=raw.resolve():shutil.copy2(source,raw)
        p.setdefault('originalGeneratedPath',str(source));p['generatedPath']=str(raw);write(pf,p)
        with Image.open(raw) as im:
            native=list(im.size);fit=ImageOps.contain(im.convert('RGB'),(1360,1360),Image.Resampling.LANCZOS)
            full=Image.new('RGB',(1600,1600),(244,239,223));full.paste(fit,((1600-fit.width)//2,(1600-fit.height)//2))
        dest=OUT/'characters'/(j['slug']+'.png');full.save(dest);box,_=bounds(full)
        assert min(box[0],box[1],1600-box[2],1600-box[3])>=120
        records.append(dict(slug=j['slug'],name=j['name'],summary=j['summary'],file=str(dest.relative_to(OUT)),raw=str(raw.relative_to(OUT)),nativeSize=native,size=[1600,1600],bounds=box,sha256=sha(dest),rawSha256=sha(raw),approved=False))
    assert len(records)==4 and len({r['rawSha256'] for r in records})==4
    items=[(r['name'],OUT/r['file']) for r in records]
    sheet(items,OUT/'review/four-butterfly-taoists.png','国风女道士 · 蝴蝶元素四款方向','蝴蝶翼形进入剪裁与轮廓 · 安静站姿 · 少量短飘带 · 宝石画风',4,470)
    sheet(items,OUT/'review/four-butterfly-taoists-large.png','国风女道士 · 四款放大核对','紫蝶清幽 / 墨蝶利落 / 粉蝶灵巧 / 青蝶沉静',2,750)
    sheet(items,OUT/'review/readability.png','小尺寸图形辨识','同框等比缩放，检查翼形轮廓、脸型和道具聚焦',4,220)
    write(OUT/'manifest.json',dict(count=4,phase='butterfly-element-review',coreElement='butterfly silhouette/wing panels/vein divisions/antenna curves',imported=False,records=records,sourceHashes={'style':sha(style),'userReference':sha(user_ref)}))
    (OUT/'README.txt').write_text('国风女道士 · 蝴蝶元素四款设计核对包\n\n'+ '\n'.join(r['name']+'：'+r['summary'] for r in records)+'\n\n这组不是把蝴蝶贴花贴到道袍上，而是把前翼/后翼形状、少量翼脉和触角弧线归纳进肩领、襟片、衣摆和发饰。道士元素减到一个小型中国法器或符纸点缀，姿势统一为安静微偏重心站姿，仅两条短飘带。\n\n四款均为成年女性、紧凑游戏比例、自然曲线和1600×1600审阅画布；原生生成图见raw。先看review总览，再按A/B/C/D反馈。未导入游戏、未制作动画。\n',encoding='utf-8-sig')
    archive=OUT/'GameXXK_四款蝴蝶元素国风女道士_设计核对包.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for d in ['raw','characters','review','prompts','references']:
            for p in sorted((OUT/d).glob('*')):
                if p.is_file():z.write(p,p.relative_to(OUT))
        for n in ['design-brief.json','manifest.json','README.txt']:z.write(OUT/n,n)
    with zipfile.ZipFile(archive) as z:assert z.testzip() is None
    print(json.dumps(dict(count=4,unique=True,safeBounds=True,sourceHashes=True,zipIntegrity=True,zipBytes=archive.stat().st_size)))
if __name__=='__main__':main()
