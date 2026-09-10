"""Package the 21 gem-style monster review paintings without importing UE assets."""
import argparse
import shutil
import zipfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageOps

from package_project_design_characters import bounds, read, sha, write

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'SourceArt/Monsters/project-design-gemstyle-20260910'
BG=(244,239,223)
TIER={'normal':'普通','elite':'精英','boss':'首领'}
INK=(35,46,41)

def atomic_save(image,path):
    temporary=path.with_name(path.stem+'.next.png')
    image.save(temporary)
    temporary.replace(path)

def package(complete=False):
    for folder in ['raw','monsters','prompts','review']:(OUT/folder).mkdir(parents=True,exist_ok=True)
    plan=read(OUT/'jobs.json');records=[]
    for j in plan['jobs']:
        assert sha(ROOT/j['identityReference'])==j['identitySourceSha256'],j['slug']+' original changed'
        p=OUT/'prompts'/(j['slug']+'.json')
        if not p.exists():continue
        prompt=read(p);source=Path(prompt['generatedPath'])
        raw=OUT/'raw'/(j['slug']+'.png');target=OUT/'monsters'/(j['slug']+'.png')
        if source.resolve()!=raw.resolve():shutil.copy2(source,raw)
        prompt.setdefault('originalGeneratedPath',str(source));prompt['generatedPath']=str(raw)
        prompt['status']='generated-awaiting-user-review';write(p,prompt)
        with Image.open(raw) as native:
            native_size=list(native.size)
            painting=ImageOps.contain(native.convert('RGB'),(1600,1600),Image.Resampling.LANCZOS)
            canvas=Image.new('RGB',(1600,1600),BG)
            canvas.paste(painting,((1600-painting.width)//2,(1600-painting.height)//2))
            box,area=bounds(canvas)
            assert min(box[0],box[1],1600-box[2],1600-box[3])>=12,(j['slug'],'clipped or near edge',box)
            atomic_save(canvas,target)
        j['status']='generated-awaiting-user-review'
        records.append({**{k:j[k] for k in ['slug','name','id','chapter','tier','identityReference','identitySourceSha256','facing','view']},'file':f'monsters/{j["slug"]}.png','sha256':sha(target),'size':[1600,1600],'raw':f'raw/{j["slug"]}.png','rawSha256':sha(raw),'nativeSize':native_size,'resampledReviewCanvas':native_size!=[1600,1600],'foregroundBounds':box,'foregroundFraction':round(area,4),'background':'warm-ivory-review','userApproved':False,'imported':False})
        records[-1]['prompt']=f'prompts/{j["slug"]}.json'
        records[-1]['revision']=prompt.get('revision','initial-project-design-redraw')
        before=OUT/'refinements/broad-forms'/(j['slug']+'-before.png')
        if before.exists() and prompt.get('revision'):
            records[-1]['revisionInputSnapshot']=str(before.relative_to(OUT))
            records[-1]['revisionInputSha256']=sha(before)
    assert len({r['id'] for r in records})==len(records)
    assert len({r['rawSha256'] for r in records})==len(records)
    if complete:
        assert len(records)==21
        for chapter in [1,2,3]:
            for tier,n in [('normal',4),('elite',2),('boss',1)]:
                assert sum(r['chapter']==chapter and r['tier']==tier for r in records)==n
    write(OUT/'jobs.json',plan)
    write(OUT/'manifest.json',{'count':len(records),'expected':21,'complete':len(records)==21,'phase':'awaiting-user-visual-review','tool':'built-in image_gen','basis':'project original designs + approved character and gem icon rendering style','nativeResolutionNote':'原生生成尺寸逐图记录；1600×1600为统一审阅画布','imported':False,'records':records})
    sheets(records)
    if complete:
        (OUT/'README.md').write_text('''# 全21怪物：项目原设计 × 宝石 icon 画风

三章各4普通、2精英、1首领，全部独立绘制。以项目safe_frame_1600原图保留物种、体型、表情和标志特征，统一与批准角色相同的深色描边及大块面画法，怪物朝画面右、尽量3/4侧面。

已根据用户反馈减少皮毛碎切面，并统一检查卡通眼睛；豪猪、野猪、山猫去掉写实虹膜/玻璃反光。长毛及大面积皮毛采用大色面与少量轮廓毛簇。

- monsters：21张1600×1600全身审阅PNG，米白背景，尚未切透明底。
- raw：原始生成图，原生尺寸见manifest；1600审阅稿经过等比缩放。
- review：全部怪物、三章分组、首领与96px辨识度预览。
- prompts / jobs.json：内置image_gen逐张绘图的完整提示词、参考路径和身份清单。
- manifest.json：原设计与新图哈希、尺寸、边界及批准状态。

本包为离线美术审阅产物。等待用户确认后才切图、导入或制作动画。未修改原项目怪物图、UE贴图、动画、玩法和局内排版。
''',encoding='utf-8')
        zpath=OUT/'GameXXK_全21怪物_宝石画风审阅包.zip'
        with zipfile.ZipFile(zpath,'w',zipfile.ZIP_DEFLATED) as z:
            for folder in ['monsters','raw','prompts','review']:
                for p in sorted((OUT/folder).glob('*')):
                    if p.is_file():z.write(p,p.relative_to(OUT))
            for name in ['README.md','jobs.json','manifest.json','visual-review.json']:
                if (OUT/name).exists():z.write(OUT/name,name)
        with zipfile.ZipFile(zpath) as z:assert z.testzip() is None
    print({'count':len(records),'complete':len(records)==21,'sourceHashes':'unchanged','uniqueImages':True,'safeBounds':'pass','zip':'verified' if complete else 'not-built'})

def sheets(records):
    title=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',34)
    font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',26)
    small=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',19)
    sets=[('all-monsters.png',records,7,330,f'全21怪物 · 宝石 icon 画风 · {len(records)}/21')]
    for chapter in [1,2,3]:sets.append((f'chapter-{chapter}.png',[r for r in records if r['chapter']==chapter],4,450,f'第{chapter}章怪物 · 原设计与特征优化'))
    sets.append(('bosses.png',[r for r in records if r['tier']=='boss'],3,600,'三位首领 · 金钱鼠 / 黑熊 / 老虎'))
    for filename,rs,cols,cell,label in sets:
        if not rs:continue
        rows=(len(rs)+cols-1)//cols
        sheet=Image.new('RGB',(cols*cell+40,rows*(cell+76)+120),BG);d=ImageDraw.Draw(sheet)
        d.text((24,18),label,font=title,fill=INK)
        d.text((25,70),'保留原物种、体型与标志特征 · 3/4朝右 · 全身单体审阅稿',font=small,fill=(83,89,75))
        for i,r in enumerate(rs):
            x=20+(i%cols)*cell;y=110+(i//cols)*(cell+76)
            with Image.open(OUT/r['file']) as source:im=source.resize((cell,cell),Image.Resampling.LANCZOS)
            sheet.paste(im,(x,y))
            d.text((x+cell//2,y+cell+23),r['name'],font=font,anchor='mm',fill=INK)
            color={'normal':(103,113,88),'elite':(76,95,128),'boss':(163,90,40)}[r['tier']]
            d.text((x+cell//2,y+cell+57),f'第{r["chapter"]}章 · {TIER[r["tier"]]}',font=small,anchor='mm',fill=color)
        atomic_save(sheet,OUT/'review'/filename)
    if not records:return
    sheet=Image.new('RGB',(1780,130+((len(records)+6)//7)*180),BG);d=ImageDraw.Draw(sheet)
    d.text((24,16),'96px图形辨识检查 · 彩色 / 剪影',font=title,fill=INK)
    d.text((24,68),'观察角、耳、尾、鬃毛及各自体型在小尺寸下的区分',font=small,fill=(83,89,75))
    for i,r in enumerate(records):
        x=20+(i%7)*250;y=120+(i//7)*180
        with Image.open(OUT/r['file']) as source:im=source.convert('RGB')
        a=np.array(im).astype(np.int16);bg=np.median(np.concatenate((a[:8].reshape(-1,3),a[-8:].reshape(-1,3),a[:,:8].reshape(-1,3),a[:,-8:].reshape(-1,3))),axis=0)
        mask=Image.fromarray((np.max(np.abs(a-bg),axis=2)>55).astype('uint8')*255).resize((96,96),Image.Resampling.LANCZOS)
        sheet.paste(im.resize((96,96),Image.Resampling.LANCZOS),(x,y));sheet.paste(INK,(x+112,y,x+208,y+96),mask)
        d.text((x+104,y+125),r['name'],font=small,anchor='mm',fill=INK)
    atomic_save(sheet,OUT/'review'/'readability-96.png')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--complete',action='store_true');args=parser.parse_args();package(args.complete)
