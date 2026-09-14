"""Package four Taoist character directions for individual design review."""
import json,shutil,hashlib,zipfile
from pathlib import Path
from PIL import Image,ImageOps
from package_blade_red_compact import sheet
from package_project_design_characters import bounds
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/Characters/female-taoist-options-20260913'
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def write(p,x):p.write_text(json.dumps(x,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    for name in ['raw','characters','review','references']:(OUT/name).mkdir(exist_ok=True)
    brief=read(OUT/'design-brief.json')
    shutil.copy2(brief['userReference'],OUT/'references/user-inspiration.png')
    shutil.copy2(brief['styleReference'],OUT/'references/approved-style.png')
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
        records.append(dict(slug=j['slug'],name=j['name'],summary=j['summary'],file=str(dest.relative_to(OUT)),raw=str(raw.relative_to(OUT)),size=[1600,1600],nativeSize=native,bounds=box,sha256=sha(dest),rawSha256=sha(raw),approved=False))
    assert len(records)==4 and len({r['rawSha256'] for r in records})==4
    items=[(r['name'],OUT/r['file']) for r in records]
    sheet(items,OUT/'review/four-taoists.png','国风女道士 · 四款设计方向','玄袖流弧 / 镇符斜线 / 灵葫圆形 / 观星直襟 · 宝石画风核对',4,470)
    sheet(items,OUT/'review/four-taoists-large.png','国风女道士 · 四款放大核对','不同性格、发型、剪裁、动作与专属法器',2,750)
    sheet(items,OUT/'review/readability.png','小尺寸图形辨识','同框等比缩放，非局内身高设定',4,220)
    write(OUT/'manifest.json',dict(count=4,phase='design-directions-awaiting-user-review',imported=False,records=records))
    (OUT/'README.txt').write_text('国风女道士 · 四款设计核对\n\n'+ '\n'.join(r['name']+'：'+r['summary'] for r in records)+'\n\nA：符纸与紫玉拂尘；B：桃木法剑与符纸；C：铜铃与封符葫芦；D：罗盘与玉指针。\n参考用户大袖、长发、飘带感觉，转换为国风道冠、交襟和各自法器；四款均为成年女性。\ncharacters为1600×1600审阅画布，原生生成尺寸见raw和manifest。\n尚未导入游戏，未制作动画；按A/B/C/D挑选或逐项调整。\n',encoding='utf-8-sig')
    archive=OUT/'GameXXK_四款国风女道士_设计核对包.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for folder in ['raw','characters','review','prompts','references']:
            for p in sorted((OUT/folder).glob('*')):
                if p.is_file():z.write(p,p.relative_to(OUT))
        for name in ['manifest.json','design-brief.json','README.txt']:z.write(OUT/name,name)
    with zipfile.ZipFile(archive) as z:assert z.testzip() is None
    print(json.dumps(dict(count=4,unique=True,safeBounds=True,zipIntegrity=True,zipBytes=archive.stat().st_size)))
if __name__=='__main__':main()
