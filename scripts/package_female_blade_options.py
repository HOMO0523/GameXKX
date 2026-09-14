"""Package four distinct blade design candidates for group selection."""
import hashlib
import json
import shutil
import zipfile
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont,ImageOps
from package_project_design_characters import bounds

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/Characters/female-blade-options-20260912'
BG=(244,239,223)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8'))
def write(p,data):p.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

def main():
    for folder in ['raw','characters','review']:(OUT/folder).mkdir(exist_ok=True)
    records=[]
    for job in read(OUT/'design-brief.json')['jobs']:
        p=OUT/'prompts'/(job['slug']+'.json');record=read(p);source=Path(record['generatedPath'])
        raw=OUT/'raw'/(job['slug']+'.png')
        if source.resolve()!=raw.resolve():shutil.copy2(source,raw)
        record.setdefault('originalGeneratedPath',str(source));record['generatedPath']=str(raw);write(p,record)
        with Image.open(raw) as im:
            native=list(im.size);fit=ImageOps.contain(im.convert('RGB'),(1360,1360),Image.Resampling.LANCZOS)
            full=Image.new('RGB',(1600,1600),BG);full.paste(fit,((1600-fit.width)//2,(1600-fit.height)//2))
        box,area=bounds(full);assert min(box[0],box[1],1600-box[2],1600-box[3])>=120
        path=OUT/'characters'/(job['slug']+'.png');full.save(path)
        records.append({'slug':job['slug'],'name':job['name'],'summary':job['summary'],'file':str(path.relative_to(OUT)),'size':[1600,1600],'nativeSize':native,'raw':str(raw.relative_to(OUT)),'rawSha256':sha(raw),'sha256':sha(path),'foregroundBounds':box,'prompt':str(p.relative_to(OUT)),'userApproved':False,'imported':False})
    assert len(records)==4 and len({r['rawSha256'] for r in records})==4
    heading=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',34)
    namefont=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',28)
    small=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',18)
    for filename,cols,cell in [('four-options.png',4,460),('four-options-large.png',2,720)]:
        sheet=Image.new('RGB',(cols*cell+40,120+((len(records)+cols-1)//cols)*(cell+85)),BG);d=ImageDraw.Draw(sheet)
        d.text((24,16),'国风女刀客 · 四款设计方向',font=heading,fill=(35,46,41))
        d.text((25,72),'女性体态与剪裁 · 不同性格与轮廓 · 宝石画风 · 供讨论选择',font=small,fill=(91,95,82))
        for i,r in enumerate(records):
            x=20+(i%cols)*cell;y=112+(i//cols)*(cell+85)
            with Image.open(OUT/r['raw']) as im:sheet.paste(im.convert('RGB').resize((cell,cell),Image.Resampling.LANCZOS),(x,y))
            d.text((x+cell/2,y+cell+24),r['name'],font=namefont,fill=(35,46,41),anchor='mm')
            d.text((x+cell/2,y+cell+63),r['summary'],font=small,fill=(91,95,82),anchor='mm')
        sheet.save(OUT/'review'/filename)
        if cols==4:sheet.save(OUT/'review'/'四款女刀客_快速预览.jpg',quality=94,subsampling=0)
    write(OUT/'manifest.json',{'count':4,'phase':'design-directions-awaiting-group-selection','tool':'built-in image_gen','records':records})
    (OUT/'README.txt').write_text('''国风女刀客 · 四款设计方向

A 张扬妩媚：红黑、S形曲线、斜肩长摆。
B 冷艳克制：月白靛青、竖向月牙、长襟窄袖。
C 俏皮灵动：朱橙墨黑、活泼三角、短衣双刀。
D 慵懒危险：酒红暖灰、松弛圆弧、垂袖大刀。

四款均为成年女性角色的独立设计候选，保持项目宝石画风与大块面，强调服装剪裁、柔和女性体态、不同神态和发型。
先看review总览，再打开characters中的1600×1600单图。raw保留原始生成尺寸；prompts保存内置绘图工具的提示词。
讨论时可按A/B/C/D选择，也可指出喜欢哪款的脸、发型、剪裁或兵器。
当前为造型候选，尚未替换角色或制作动画。
''',encoding='utf-8-sig')
    archive=OUT/'GameXXK_四款女刀客_设计选择包.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for folder in ['characters','raw','review','prompts']:
            for p in sorted((OUT/folder).glob('*')):
                if p.is_file():z.write(p,p.relative_to(OUT))
        for name in ['design-brief.json','manifest.json','README.txt']:z.write(OUT/name,name)
    with zipfile.ZipFile(archive) as z:assert z.testzip() is None
    print(json.dumps({'count':4,'size':[1600,1600],'uniqueImages':True,'safeBounds':'passed','zipIntegrity':'passed','bytes':archive.stat().st_size},ensure_ascii=True))

if __name__=='__main__':main()
