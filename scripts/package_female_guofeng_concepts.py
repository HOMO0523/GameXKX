"""Package four first-pass female character designs for reference-led review."""
import hashlib
import json
import shutil
import zipfile
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont,ImageOps
from package_project_design_characters import bounds

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/Characters/female-guofeng-concepts-20260912'
BG=(244,239,223)

def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def write(path,data):path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

def main():
    brief=read(OUT/'design-brief.json')
    for directory in ['raw','characters','review']:(OUT/directory).mkdir(exist_ok=True)
    base=ROOT/'SourceArt/Characters/project-design-gemstyle-all-20260910'
    originals={r['slug']:r for r in read(base/'manifest.json')['records']}
    records=[]
    for j in brief['jobs']:
        promptpath=OUT/'prompts'/(j['slug']+'.json')
        if not promptpath.exists():continue
        prompt=read(promptpath);generated=Path(prompt['generatedPath'])
        raw=OUT/'raw'/(j['slug']+'.png')
        if generated.resolve()!=raw.resolve():shutil.copy2(generated,raw)
        prompt.setdefault('originalGeneratedPath',str(generated));prompt['generatedPath']=str(raw);write(promptpath,prompt)
        with Image.open(raw) as image:
            native=list(image.size);image=ImageOps.contain(image.convert('RGB'),(1600,1600),Image.Resampling.LANCZOS)
            final=Image.new('RGB',(1600,1600),BG);final.paste(image,((1600-image.width)//2,(1600-image.height)//2))
            box,area=bounds(final)
            assert min(box[0],box[1],1600-box[2],1600-box[3])>=12,(j['slug'],box)
            path=OUT/'characters'/(j['slug']+'.png');final.save(path)
        original=originals[j['sourceSlug']]
        assert sha(base/original['file'])==original['sha256'],j['sourceSlug']+' prior design changed'
        records.append({'slug':j['slug'],'name':j['name'],'role':j['role'],'summary':j['summary'],'file':str(path.relative_to(OUT)),'sha256':sha(path),'raw':str(raw.relative_to(OUT)),'rawSha256':sha(raw),'nativeSize':native,'reviewSize':[1600,1600],'reviewResampled':native!=[1600,1600],'background':'warm ivory review','facing':'left','view':'three-quarter','prompt':str(promptpath.relative_to(OUT)),'userApproved':False,'imported':False})
        records[-1].update(foregroundBounds=box,foregroundFraction=round(area,4),visualReview='first draft ready for reference-led refinement')
    assert len(records)==4 and len({r['rawSha256'] for r in records})==4
    heading=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',34)
    label=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',28)
    small=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',18)
    for filename,columns,cell in [('four-women-overview.png',4,450),('four-women-large.png',2,700)]:
        rows=(len(records)+columns-1)//columns
        sheet=Image.new('RGB',(columns*cell+40,120+rows*(cell+82)),BG);d=ImageDraw.Draw(sheet)
        d.text((24,16),'国风女性角色 · 第一版造型试稿',font=heading,fill=(35,46,41))
        d.text((25,72),'丰满体态 · 剪裁与疏密 · 3/4朝左 · 后续结合参考图调整',font=small,fill=(92,95,80))
        for i,r in enumerate(records):
            x=20+(i%columns)*cell;y=110+(i//columns)*(cell+82)
            with Image.open(OUT/r['file']) as im:sheet.paste(im.resize((cell,cell),Image.Resampling.LANCZOS),(x,y))
            d.text((x+cell//2,y+cell+23),r['name'],font=label,fill=(35,46,41),anchor='mm')
            d.text((x+cell//2,y+cell+61),r['summary'],font=small,fill=(92,95,80),anchor='mm')
        sheet.save(OUT/'review'/filename)
    write(OUT/'manifest.json',{'count':4,'tool':'built-in image_gen','phase':'first-draft-awaiting-user-references','naming':'本批统一采用道士称呼','next':'用户参考图调整定稿后，再优化其他男性角色并安排接入','records':records})
    (OUT/'README.txt').write_text('''国风女性角色第一版 · 2026.09.12

本批为女刀客伙伴、女弓箭手伙伴、女道士伙伴、女主角四张独立试稿。
称呼统一使用“道士”。造型方向为成年女性、丰满体态、明确剪裁与疏密关系。
每位保持不同发型、剪裁与职业道具，采用项目的大块面、卡通眼睛及3/4朝左画法。

characters：1600×1600米白背景审阅单图。raw：原始生成图，原生尺寸见manifest。
review：四人总览和2×2大图。prompts：内置图像工具的逐张提示词和参考来源。

这是等待参考图进一步调整的第一版，尚未替换角色资产或制作动画。
其他男性角色的优化留待本批调整定稿后继续。
''',encoding='utf-8-sig')
    archive=OUT/'GameXXK_四位国风女性角色_第一版试稿.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for directory in ['characters','raw','review','prompts']:
            for p in sorted((OUT/directory).glob('*')):
                if p.is_file():z.write(p,p.relative_to(OUT))
        for name in ['design-brief.json','manifest.json','README.txt']:z.write(OUT/name,name)
    with zipfile.ZipFile(archive) as z:assert z.testzip() is None
    print(json.dumps({'count':4,'priorMaleImages':'unchanged','zipIntegrity':'passed','bytes':archive.stat().st_size},ensure_ascii=False))

if __name__=='__main__':main()
